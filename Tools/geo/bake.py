#!/usr/bin/env python3
"""Bake a phase of the real Florida Keys into the game frame.

  devkit/pyenv/bin/python Tools/geo/bake.py p1 <raw dir> <out dir> [--preview out.png]

Inputs (public data, fetched by Tools/geo/fetch.sh into the raw dir):
  dem_<phase>_half.tif    NOAA NCEI DEM mosaic (CUDEM / Coastal Relief Model, with USGS land elevation), WGS84, metres
  coast_<phase>.json      OpenStreetMap natural=coastline ways (c) OpenStreetMap contributors, ODbL 1.0
  osm_<phase>_extra.json  OpenStreetMap US-1, bridges, place names, mangroves, marinas, reefs

Output <out dir>/<phase>.kgeo (little endian):
  char[4] 'KGEO', int32 version (1)
  double X0, Z0, Cell; int32 NX, NZ          the grid, game metres; row-major, z rows outer, [iz * NX + ix]
  double Scale, ThetaRad, Lat0, Lon0, AnchorX, AnchorZ
  int32 NChain; double ChainX0, ChainStep    US-1's line: game z at x = ChainX0 + i * ChainStep
  int16 Height[NZ * NX]                      centimetres, + land, - water depth (never 0: land >= 20, water <= -30)
  int16 Shore[NZ * NX]                       decimetres from the shoreline, + at sea, - on land (clamped +-3276 m)
  float Chain[NChain]
and <out dir>/<phase>_places.json: islands (name, centre, area, outline rings), bridges, mangroves, marinas, reefs.
"""
import sys, json, struct, math, os
import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage
from shapely.geometry import LineString, Polygon, Point
from shapely.ops import linemerge, polygonize, unary_union

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frame as F

PHASES = {
    # game rect (x0, x1, z0, z1) and the lon/lat box the raw DEM was fetched for (w, s, e, n)
    'p1': dict(rect=(-8300, 7500, -4200, 6200), box=(-81.38, 24.46, -80.79, 24.91)),
}
CELL = 10.0
FINE = 2          # the land mask is rasterised FINE x finer than the grid


def main():
    phase, raw, out = sys.argv[1], sys.argv[2], sys.argv[3]
    preview = sys.argv[sys.argv.index('--preview') + 1] if '--preview' in sys.argv else None
    P = PHASES[phase]
    x0, x1, z0, z1 = P['rect']
    w, s, e, n = P['box']
    nx, nz = int(round((x1 - x0) / CELL)) + 1, int(round((z1 - z0) / CELL)) + 1

    Image.MAX_IMAGE_PIXELS = None
    dem = np.array(Image.open(f'{raw}/dem_{phase}_half.tif')).astype(np.float32)
    dh, dw = dem.shape
    dem = np.where(np.isfinite(dem) & (dem > -1e4), dem, np.nan)
    if np.isnan(dem).any():
        idx = ndimage.distance_transform_edt(np.isnan(dem), return_distances=False, return_indices=True)
        dem = dem[tuple(idx)]

    def sample(gx, gz):
        """DEM metres at game points (arrays)."""
        a = (gx - F.ANCHOR_X) * F.SCALE
        b = -(gz - F.ANCHOR_Z) * F.SCALE
        c, sn = math.cos(F.THETA), math.sin(F.THETA)
        lon = F.LON0 + (a * c - b * sn) / F.M_LON
        lat = F.LAT0 + (a * sn + b * c) / F.M_LAT
        col = (lon - w) / (e - w) * dw - .5
        row = (n - lat) / (n - s) * dh - .5
        return ndimage.map_coordinates(dem, [row, col], order=1, mode='nearest')

    # ---- the land mask on the fine grid: OSM's coastline polygons, plus anything the DEM holds clear of the water
    fc = CELL / FINE
    fnx, fnz = (nx - 1) * FINE + 1, (nz - 1) * FINE + 1
    gx = x0 + np.arange(fnx) * fc
    gz = z0 + np.arange(fnz) * fc
    GX, GZ = np.meshgrid(gx, gz)
    fine = sample(GX, GZ)

    coast = json.load(open(f'{raw}/coast_{phase}.json'))['elements']
    lines = []
    for wy in coast:
        pts = [F.to_game(p['lon'], p['lat']) for p in wy['geometry']]
        if len(pts) >= 2:
            lines.append(LineString(pts))
    merged = linemerge(lines)
    polys = [p for p in polygonize(merged) if p.area > 60]
    # coastlines cut off by the edge of the data box never close: those islands come from the DEM instead
    mask_img = Image.new('L', (fnx, fnz), 0)
    draw = ImageDraw.Draw(mask_img)
    for p in polys:
        draw.polygon([((x - x0) / fc, (z - z0) / fc) for x, z in p.exterior.coords], fill=1)
        for hole in p.interiors:
            draw.polygon([((x - x0) / fc, (z - z0) / fc) for x, z in hole.coords], fill=0)
    osm_land = np.array(mask_img, dtype=bool)
    dem_land = fine > .35
    lab, cnt = ndimage.label(dem_land & ~osm_land)
    if cnt:
        sizes = ndimage.sum(np.ones_like(lab), lab, index=np.arange(1, cnt + 1)) * fc * fc
        # a patch of DEM-only land counts if it is big enough to be an islet, or it touches OSM land (a wider shore)
        touch = ndimage.maximum(ndimage.binary_dilation(osm_land, iterations=2).astype(np.uint8), lab, index=np.arange(1, cnt + 1))
        keep = np.zeros(cnt + 1, bool)
        keep[1:] = (sizes > 400) & ((sizes > 2500) | (touch > 0))
        dem_land = keep[lab]
    land = osm_land | dem_land
    land = ndimage.binary_opening(land, iterations=1) | osm_land

    # ---- signed distance from the shoreline (game metres)
    d_out = ndimage.distance_transform_edt(~land) * fc
    d_in = ndimage.distance_transform_edt(land) * fc
    shore_f = np.where(land, -(d_in - fc / 2), d_out - fc / 2)

    # ---- heights: real elevation on land, real depth at sea, agreeing with the mask
    h = fine.copy()
    h = np.where(land, np.maximum(h, .2 + np.minimum(d_in, 40) * .02), np.minimum(h, -.3))
    # down to the 10 m grid (the fine grid is only for a clean shoreline)
    height = h[::FINE, ::FINE]
    shore = shore_f[::FINE, ::FINE]
    landg = land[::FINE, ::FINE]
    assert height.shape == (nz, nx)

    # ---- US-1's line: z as a function of x
    extra = json.load(open(f'{raw}/osm_{phase}_extra.json'))['elements']
    us1 = []
    for el in extra:
        t = el.get('tags', {})
        if el['type'] == 'way' and t.get('highway') == 'trunk' and 'geometry' in el:
            us1 += [F.to_game(p['lon'], p['lat']) for p in el['geometry']]
    us1 = np.array(sorted(us1))
    step = 50.0
    cx = np.arange(x0, x1 + step, step)
    # densify between consecutive points so long bridge spans don't leave holes, then bin by x
    order = np.argsort(us1[:, 0])
    ux, uz = us1[order, 0], us1[order, 1]
    keep = np.concatenate([[True], np.diff(ux) > .01])
    ux, uz = ux[keep], uz[keep]
    cz = np.interp(cx, ux, uz)
    cz = ndimage.gaussian_filter1d(cz, 3, mode='nearest')

    # ---- places
    names = []
    for el in extra:
        t = el.get('tags', {})
        if t.get('place') in ('island', 'islet') and t.get('name'):
            if el['type'] == 'node':
                names.append((t['name'], F.to_game(el['lon'], el['lat'])))
            elif 'geometry' in el:
                g = [F.to_game(p['lon'], p['lat']) for p in el['geometry']]
                names.append((t['name'], (sum(p[0] for p in g) / len(g), sum(p[1] for p in g) / len(g))))
    islands = []
    for p in sorted(polys, key=lambda q: -q.area):
        c = p.representative_point()
        if not (x0 <= c.x <= x1 and z0 <= c.y <= z1):
            continue
        nm = ''
        for name, (px, pz) in names:
            if p.contains(Point(px, pz)) or p.distance(Point(px, pz)) < 25:
                nm = name
                break
        sp = p.simplify(1.5)
        if sp.is_empty or sp.geom_type != 'Polygon':
            sp = p
        mnx, mnz, mxx, mxz = p.bounds
        islands.append(dict(name=nm, x=round(p.centroid.x, 1), z=round(p.centroid.y, 1), area=round(p.area), bounds=[round(v, 1) for v in (mnx, mnz, mxx, mxz)],
                            ring=[round(v, 1) for xy in sp.exterior.coords for v in xy]))

    def ways(pred):
        outl = []
        for el in extra:
            t = el.get('tags', {})
            if el['type'] == 'way' and 'geometry' in el and pred(t):
                g = [F.to_game(p['lon'], p['lat']) for p in el['geometry']]
                outl.append(dict(name=t.get('name', ''), pts=[round(v, 1) for xy in g for v in xy]))
        return outl

    places = dict(
        phase=phase, rect=[x0, x1, z0, z1], scale=F.SCALE, theta_deg=math.degrees(F.THETA),
        attribution='Coastlines, roads and place names (c) OpenStreetMap contributors, ODbL 1.0. Elevation and bathymetry: NOAA NCEI / USGS (public domain).',
        islands=islands,
        us1_bridges=ways(lambda t: t.get('highway') == 'trunk' and t.get('bridge')),
        old_bridges=ways(lambda t: t.get('bridge') and t.get('name') in ('Old Seven Mile Bridge', 'Bahia Honda Bridge')),
        mangroves=ways(lambda t: t.get('wetland') == 'mangrove'),
        marinas=ways(lambda t: t.get('leisure') == 'marina') + [dict(name=el['tags'].get('name', ''), pts=[round(v, 1) for v in F.to_game(el['lon'], el['lat'])])
                                                               for el in extra if el['type'] == 'node' and el.get('tags', {}).get('leisure') == 'marina'],
        reefs=ways(lambda t: t.get('natural') == 'reef'),
        runways=ways(lambda t: t.get('aeroway') == 'runway'),
        localities=[dict(name=el['tags']['name'], kind=el['tags']['place'], pts=[round(v, 1) for v in F.to_game(el['lon'], el['lat'])])
                    for el in extra if el['type'] == 'node' and el.get('tags', {}).get('place') and el['tags'].get('name')],
    )

    os.makedirs(out, exist_ok=True)
    with open(f'{out}/{phase}.kgeo', 'wb') as f:
        f.write(b'KGEO' + struct.pack('<i', 1))
        f.write(struct.pack('<dddii', x0, z0, CELL, nx, nz))
        f.write(struct.pack('<dddddd', F.SCALE, F.THETA, F.LAT0, F.LON0, F.ANCHOR_X, F.ANCHOR_Z))
        f.write(struct.pack('<idd', len(cz), float(cx[0]), step))
        f.write(np.clip(np.round(height * 100), -32000, 32000).astype('<i2').tobytes())
        f.write(np.clip(np.round(shore * 10), -32760, 32760).astype('<i2').tobytes())
        f.write(cz.astype('<f4').tobytes())
    json.dump(places, open(f'{out}/{phase}_places.json', 'w'), separators=(',', ':'))

    wet = ~landg
    print(f'{phase}: grid {nx} x {nz} at {CELL} m, {os.path.getsize(f"{out}/{phase}.kgeo") / 1e6:.1f} MB')
    print(f'  land {landg.mean() * 100:.2f}%  islands {len(islands)} ({sum(1 for i in islands if i["name"])} named)  highest {height.max():.1f} m  deepest {-height.min():.1f} m')
    print(f'  water under 1 m {((height > -1) & wet).sum() / wet.sum() * 100:.1f}%  under 2 m {((height > -2) & wet).sum() / wet.sum() * 100:.1f}%  over 45 m {(height < -45).sum() / wet.sum() * 100:.1f}%')
    print(f'  US-1: z from {cz.min():.0f} to {cz.max():.0f}; bridges {len(places["us1_bridges"])}, mangrove areas {len(places["mangroves"])}, marinas {len(places["marinas"])}, reefs {len(places["reefs"])}')

    if preview:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots(figsize=(22, 14.5))
        dep = np.where(landg, np.nan, -height)
        im = ax.imshow(dep, extent=[x0, x1, z1, z0], cmap='turbo_r', vmin=0, vmax=14)
        ax.imshow(np.where(landg, height, np.nan), extent=[x0, x1, z1, z0], cmap='Greys_r', vmin=-1, vmax=4)
        ax.contour(np.linspace(x0, x1, nx), np.linspace(z0, z1, nz), -height, levels=[45], colors='w', linewidths=.6)
        ax.plot(cx, cz, 'm', lw=.8)
        for i in islands:
            if i['name']:
                ax.text(i['x'], i['z'], i['name'], fontsize=6, color='k', ha='center')
        for b in places['us1_bridges']:
            ax.plot(b['pts'][0::2], b['pts'][1::2], 'r', lw=1.5)
        ax.set_title(f'{phase} in the game frame (metres; 1 : {F.SCALE:g}); colour = depth clipped at 14 m, white line = 45 m, magenta = US-1, red = bridges')
        plt.colorbar(im, shrink=.5)
        plt.savefig(preview, dpi=70, bbox_inches='tight')


if __name__ == '__main__':
    main()
