#!/usr/bin/env python3
"""Bake a phase of the real Florida Keys into the game frame.

  devkit/pyenv/bin/python Tools/geo/bake.py p1 <raw dir> <out dir> [--preview out.png]

Inputs (public data in the raw dir; Phase 2's are fetched by Tools/geo/fetch_p2.sh):
  dem_<src>_half.tif    NOAA NCEI DEM mosaic (CUDEM / Coastal Relief Model, with USGS land elevation), WGS84, metres
  coast_<src>.json      OpenStreetMap natural=coastline ways (c) OpenStreetMap contributors, ODbL 1.0
  osm_<src>_extra.json  OpenStreetMap US-1, bridges, place names, mangroves, marinas, reefs
<src> is the phase's own name unless its PHASES entry names another (p2_bay is cut from p2's downloads).

Phases after the first share an edge with one already baked (PHASES 'join'). Along that edge the new grid takes the
old one's heights and shore distances exactly, and the difference is faded out over JOIN_FADE cells, so the two
files meet with no step. Bake order: p1, p2, p2_bay, p3, p4, p4_bay.

Phase 3 of the map is p3 (the Lower Keys, west of p1: Big Pine Key to Key West), p4 (Key Largo, east and north of
p2) and p4_bay (Florida Bay and the Gulf north of everything else, which squares the map off). Their downloads are
fetched by Tools/geo/fetch_p3.sh. Each is baked twice like p2 (see below), and writes <phase>_road.txt: where US-1
really runs on the phase's stretch of the chain line, and its bridges that are not on that line.

p2 is baked twice: once, then bake_marks.py, then again - the second time it reads p2_marks.txt and cuts the marked
channel legs the 10 m grid has pinched shut (PHASES 'cut_marks'). Then bake_marks.py and bake_mangrove.py once more.

Output <out dir>/<phase>.kgeo (little endian):
  char[4] 'KGEO', int32 version (2)
  double X0, Z0, Cell; int32 NX, NZ          the grid, game metres; row-major, z rows outer, [iz * NX + ix]
  double Scale, ThetaRad, Lat0, Lon0, AnchorX, AnchorZ
  int32 NChain; double ChainX0, ChainStep    US-1's line: game z at x = ChainX0 + i * ChainStep
  int16 Height[NZ * NX]                      centimetres, + land, - water depth (never 0: land >= 20, water <= -30)
  int16 Shore[NZ * NX]                       decimetres from the shoreline, + at sea, - on land (clamped +-3276 m)
  float Chain[NChain]
  uint8 Island[NZ * NX]                      index of the nearest island
  int32 NIslands, then each: int32 name bytes, utf8 name, double X, Z, Area, MinX, MinZ, MaxX, MaxZ,
                             int32 N, float ring[N] (x0, z0, x1, z1, ... closed outline, game metres)
and <out dir>/<phase>_places.json: islands (name, centre, area, outline rings), bridges, mangroves, marinas, reefs.
Phases with 'clip' also get: bridges (each US-1 bridge once, with its name and its two ends a and b), other_bridges
(the old railway bridges beside them), lights, and reefs and marinas kept to the phase's own rectangle.
"""
import sys, json, struct, math, os
import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage
from shapely.geometry import LineString, Polygon, Point
from shapely.ops import linemerge, polygonize, unary_union

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frame as F
from kgeo import KGeo

PHASES = {
    # game rect (x0, x1, z0, z1) and the lon/lat box the raw DEM was fetched for (w, s, e, n)
    'p1': dict(rect=(-8300, 7500, -4200, 6200), box=(-81.38, 24.46, -80.79, 24.91)),
    # Phase 2, the Upper Keys: Long Key to Tavernier. East edge 23500 (Tavernier is at 21800, Conch Reef at 23030);
    # north edge -6920 (the chain climbs to z = -5300 by the east edge, and 1360 m steps keep Phase 1's scenery on
    # the same spots: see the status file); south edge 6200, the same as Phase 1.
    'p2': dict(rect=(7500, 23500, -6920, 6200), box=(-81.41, 24.60, -80.35, 25.12), src='p2', pad=60, clip=True,
               join=[('p1', 'west')], cut_marks=True, canals='osm_p2_built.json'),
    # the strip of Florida Bay north of Phase 1 that squares the map off
    'p2_bay': dict(rect=(-8300, 7500, -6920, -4200), box=(-81.41, 24.60, -80.35, 25.12), src='p2', pad=60, clip=True,
                   join=[('p1', 'south'), ('p2', 'east')], chain_from='p1'),
    # Phase 3, the Lower Keys: Big Pine Key to Key West. West edge -30060: Key West harbor is at -24830, Sand Key Light
    # at -28330, and the 21760 m from Phase 1's edge is 16 steps of 1360 m, so the scenery laid out on grids counted from
    # the map's corner (palms 16 m, houses 34 m, terrain 8 m, depth 10 m) keeps its spots. North and south as Phase 2.
    'p3': dict(rect=(-30060, -8300, -6920, 6200), box=(-82.05, 24.26, -81.22, 24.86), src='p3', pad=60, clip=True, gen=3,
               join=[('p1', 'east'), ('p2_bay', 'east')], chain_end='east', cut_marks=True, canals='osm_p3_built.json', road=True),
    # Key Largo: east and north of Phase 2, to the top of the island (Ocean Reef is at x 33470, Carysfort Reef at 34025,
    # the Card Sound bridge at z -12300). North edge -13720 is five more steps of 1360 m. US-1 runs along the island as
    # far as x 27000 and then leaves it for Jewfish Creek and the mainland, nearly straight up the map: from there the
    # chain line (the keys' axis, which the game measures bay side and ocean side from) follows the middle of Key Largo
    # itself ('chain': the island that point is on), and 'road' says the road is only on it as far as 27000.
    'p4': dict(rect=(23500, 35500, -13720, 6200), box=(-80.62, 24.72, -79.98, 25.42), src='p4', pad=60, clip=True, gen=4,
               join=[('p2', 'west')], cut_marks=True, canals='osm_p4_built.json', chain=dict(xmax=27000.0, on=(-80.4470, 25.0860)), road=(23500.0, 27000.0)),
    # Florida Bay and the Gulf north of all the rest, which squares the map off: up to the mainland at Flamingo. Its
    # downloads are the other phases' put together (fetch_p3.sh p4_bay); the DEM is 1 arc-second.
    'p4_bay': dict(rect=(-30060, 23500, -13720, -6920), box=(-82.10, 24.60, -80.45, 25.30), src='p4_bay', pad=60, clip=True, gen=4,
                   join=[('p3', 'south'), ('p2_bay', 'south'), ('p2', 'south'), ('p4', 'east')], chain_from=['p3', 'p1', 'p2'], cut_marks=True),
}
JOIN_FADE = 30    # cells over which a joined edge's correction fades out
CUT_MAX, CUT_HALF_W = 20.0, 8.0   # marked channel legs: how much land a leg may clip and still be cut, and how wide
CELL = 10.0
# islands everyone knows by name, with a point that is on them (lon, lat); first match wins
KNOWN = [
    ('Marathon (Vaca Key)', -81.0730, 24.7180), ('Boot Key', -81.1010, 24.6980), ('Key Colony Beach', -81.0190, 24.7215),
    ('Fat Deer Key', -81.0120, 24.7330), ('Grassy Key', -80.9550, 24.7600), ('Crawl Key', -80.9800, 24.7480),
    ("Knight's Key", -81.1245, 24.7075), ('Pigeon Key', -81.1553, 24.7040), ('Duck Key', -80.9130, 24.7740),
    ('Bahia Honda Key', -81.2700, 24.6600), ('Little Duck Key', -81.2290, 24.6810), ('Missouri Key', -81.2380, 24.6760),
    ('Ohio Key', -81.2460, 24.6720), ('No Name Key', -81.3250, 24.6980), ('Conch Key', -80.8880, 24.7900),
    ('Long Point Key', -80.9870, 24.7480), ('Sombrero Key', -81.1110, 24.6270),
    # Phase 2
    ('Long Key', -80.8230, 24.8170), ('Lower Matecumbe Key', -80.7050, 24.8600), ('Upper Matecumbe Key', -80.6350, 24.9150),
    ('Windley Key', -80.5950, 24.9500), ('Plantation Key', -80.5530, 24.9850), ('Tavernier (Key Largo)', -80.5150, 25.0100),
    ('Fiesta Key', -80.7900, 24.8420), ('Lignumvitae Key', -80.6970, 24.9020), ('Indian Key', -80.6770, 24.8780),
    # Phase 3, the Lower Keys (US-1's causeways join some keys into one outline: it takes the best known name on it)
    ('Key West', -81.7800, 24.5550), ('Stock Island', -81.7400, 24.5700), ('Boca Chica Key', -81.6900, 24.5750),
    ('Sugarloaf Key', -81.5400, 24.6750), ('Lower Sugarloaf Key', -81.5511, 24.6214), ('Summerland Key', -81.4450, 24.6620),
    ('Ramrod Key', -81.4150, 24.6600), ('Middle Torch Key', -81.4050, 24.6850), ('Fleming Key', -81.7950, 24.5850),
    ('Geiger Key', -81.6550, 24.5780), ('Knockemdown Key', -81.4800, 24.7000), ('Wisteria Island', -81.8110, 24.5690),
    # Key Largo
    ('Key Largo', -80.4470, 25.0860), ('Rodriguez Key', -80.4530, 25.0530), ('El Radabob Key', -80.3700, 25.1250),
]
# OpenStreetMap's spelling put right
RENAME = {'Nile Channel Bridge': 'Niles Channel Bridge'}
# US-1 bridges OpenStreetMap leaves unnamed, by a point on them (lon, lat)
BRIDGE_NAMES = [('Tavernier Creek Bridge', -80.5285, 25.0035), ('Long Key Viaduct', -80.8600, 24.7950), ('Channel 5 Bridge', -80.7750, 24.8380),
                ('Channel 2 Bridge', -80.7520, 24.8420), ('Lignumvitae Channel Bridge', -80.6920, 24.8700), ('Indian Key Channel Bridge', -80.6780, 24.8800),
                ('Tea Table Key Bridge', -80.6600, 24.8900), ('Tea Table Relief Bridge', -80.6520, 24.8950), ('Whale Harbor Bridge', -80.6090, 24.9390),
                ('Snake Creek Bridge', -80.5870, 24.9520),
                # Phase 3 (points on the chain line, from the first bake's list of unnamed spans)
                ('Boca Chica Channel Bridge', -81.7163, 24.5747), ('Cow Key Channel Bridge', -81.7507, 24.5687)]
# reefs and reef lights everyone knows by name that OpenStreetMap has no outline for (lon, lat); each is checked against
# the DEM when baked (the summary prints the depth found there)
KNOWN_REEFS = [('Tennessee Reef', -80.7817, 24.7450), ('Alligator Reef', -80.6185, 24.8517), ('Crocker Reef', -80.5270, 24.9080),
               ('Davis Reef', -80.5050, 24.9230), ('Conch Reef', -80.4560, 24.9570), ('Hen and Chickens', -80.5500, 24.9350),
               ('Cheeca Rocks', -80.6160, 24.9040),
               # Phase 3
               ('Looe Key', -81.4030, 24.5460), ('American Shoal', -81.5190, 24.5250), ('Maryland Shoal', -81.5800, 24.5100),
               ('Pelican Shoal', -81.6300, 24.5000), ('Eastern Sambo', -81.6650, 24.4900), ('Middle Sambo', -81.6800, 24.4880),
               ('Western Sambo', -81.7150, 24.4800), ('Sand Key', -81.8775, 24.4539),
               # Key Largo
               ('Molasses Reef', -80.3760, 25.0120), ('French Reef', -80.3500, 25.0340), ('Grecian Rocks', -80.3050, 25.1080),
               ('Key Largo Dry Rocks', -80.2970, 25.1230), ('Elbow Reef', -80.2580, 25.1450), ('Carysfort Reef', -80.2115, 25.2217),
               ('Pickles Reef', -80.4150, 24.9850)]
KNOWN_LIGHTS = [('Alligator Reef Light', -80.6185, 24.8517), ('Tennessee Reef Light', -80.7817, 24.7450), ('Looe Key Light', -81.4030, 24.5460),
                ('Molasses Reef Light', -80.3760, 25.0120), ('Carysfort Reef Light', -80.2115, 25.2217), ('Elbow Reef Light', -80.2580, 25.1450)]
# waters known by name (lon, lat): they go in the places file with the depth the grid has there
KNOWN_WATERS = [('Largo Sound', -80.3900, 25.1330), ('Blackwater Sound', -80.4300, 25.1700), ('Barnes Sound', -80.3800, 25.2500), ('Card Sound', -80.3300, 25.3200),
                ('Tarpon Basin', -80.4350, 25.1200), ('Buttonwood Sound', -80.4600, 25.1000)]
# road bridges that are not on the chain line, by their name in OpenStreetMap (they go in the places file with their two ends)
SIDE_BRIDGES = ('Jewfish Creek Bridge', 'Card Sound Bridge')
# charted shoals the elevation data has lost (lon, lat, radius in game metres, depth on the crest). Sand Key: NOAA's
# mosaic is the coarse Coastal Relief Model out here and has 21 m where the chart has a reef awash with a light on it.
SHOALS = {'p3': [('Sand Key', -81.8775, 24.4539, 150.0, 1.0)]}
FINE = 2          # the land mask is rasterised FINE x finer than the grid


def main():
    phase, raw, out = sys.argv[1], sys.argv[2], sys.argv[3]
    preview = sys.argv[sys.argv.index('--preview') + 1] if '--preview' in sys.argv else None
    P = PHASES[phase]
    src = P.get('src', phase)
    rx0, rx1, rz0, rz1 = P['rect']
    # the work is done on a rectangle PAD cells wider all round, so shore distances see the land past the edge, and
    # cut back to the phase's own rectangle at the end
    PAD = P.get('pad', 0)
    x0, x1, z0, z1 = (rx0 - PAD * CELL, rx1 + PAD * CELL, rz0 - PAD * CELL, rz1 + PAD * CELL) if PAD else P['rect']
    w, s, e, n = P['box']
    nx, nz = int(round((x1 - x0) / CELL)) + 1, int(round((z1 - z0) / CELL)) + 1

    Image.MAX_IMAGE_PIXELS = None
    dem = np.array(Image.open(f'{raw}/dem_{src}_half.tif')).astype(np.float32)
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

    shoal_note = ''
    for name, lon, lat, rad, dep in SHOALS.get(phase, []):
        sx, sz = F.to_game(lon, lat)
        rr = np.hypot(GX - sx, GZ - sz) / rad
        was = float(-fine[rr < .1].max()) if (rr < .1).any() else float('nan')
        fine = np.where(rr < 1, np.maximum(fine, -(dep + (np.maximum(-fine, dep) - dep) * rr ** 2)), fine)
        shoal_note += f'{name} raised to {dep:.1f} m at {sx:.0f}, {sz:.0f} (the data had {was:.1f} m); '

    coast = json.load(open(f'{raw}/coast_{src}.json'))['elements']
    lines = []
    for wy in coast:
        pts = [F.to_game(p['lon'], p['lat']) for p in wy['geometry']]
        if len(pts) >= 2:
            lines.append(LineString(pts))
    if P.get('clip'):
        # coastlines cut off by the edge of the data box are closed along that edge (Key Largo runs on out of it): the
        # box's outline joins the coastlines, and of the pieces that touch it the ones the DEM holds dry are land
        ring = LineString([F.to_game(lo, la) for lo, la in ((w + .002, s + .002), (e - .002, s + .002), (e - .002, n - .002), (w + .002, n - .002), (w + .002, s + .002))])
        rs = np.random.RandomState(7)
        polys = []
        boxed_src = set()      # outlines closed along the data box: the mainland, cut off by it
        for p in polygonize(unary_union(lines + [ring])):
            if p.area <= 60:
                continue
            if p.exterior.distance(ring) < .5:
                boxed_src.add(id(p))
                mnx, mnz, mxx, mxz = p.bounds
                pts = [(p.representative_point().x, p.representative_point().y)]
                for _ in range(4000):
                    if len(pts) >= 60:
                        break
                    q = (rs.uniform(mnx, mxx), rs.uniform(mnz, mxz))
                    if p.contains(Point(*q)):
                        pts.append(q)
                if np.mean(sample(np.array([q[0] for q in pts]), np.array([q[1] for q in pts])) > .1) < .5:
                    continue
            polys.append(p)
        # cut to the working rectangle; an island belongs to this phase when most of what is left lies in the phase's own
        box_poly = Polygon([(x0, z0), (x1, z0), (x1, z1), (x0, z1)])
        rect_poly = Polygon([(rx0, rz0), (rx1, rz0), (rx1, rz1), (rx0, rz1)])
        cut = []
        boxed = set()
        for p in polys:
            q = p.intersection(box_poly)
            for part in (q.geoms if hasattr(q, 'geoms') else [q]):
                if part.geom_type == 'Polygon' and part.area > 60:
                    cut.append(part)
                    if id(p) in boxed_src:
                        boxed.add(id(part))
        polys = cut
        # an island belongs to one phase only: not this one if a neighbour already baked lists it, else whichever of
        # the phases baked this way holds most of it
        # (phases of the same 'gen' are baked together and share islands out by area; one of an earlier gen is finished,
        # and what it lists is its own)
        gen = P.get('gen', 2)
        theirs = []
        for other, _ in P.get('join', []):
            if not PHASES[other].get('clip') or PHASES[other].get('gen', 2) < gen:
                theirs += [Polygon(i['ring']) for i in KGeo(f'{out}/{other}.kgeo').islands if len(i['ring']) >= 4]
        theirs = [t if t.is_valid else t.buffer(0) for t in theirs]
        sib = [Polygon([(a, c), (b, c), (b, d), (a, d)]) for k, v in PHASES.items() if k != phase and v.get('clip') and v.get('gen', 2) == gen for a, b, c, d in [v['rect']]]
        mine = set()
        for p in polys:
            a_in = p.intersection(rect_poly).area
            if a_in <= 0 or any(a_in < p.intersection(o).area for o in sib):
                continue
            if any(t.intersects(p) and t.intersection(p).area > .5 * min(t.area, p.area) for t in theirs):
                continue
            mine.add(id(p))
    else:
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

    # ---- the residential canals, too narrow for the grid: every second one widened, the rest filled (canals.py)
    canal_note = ''
    canal_depth = None
    if P.get('canals') and os.path.exists(f"{raw}/{P['canals']}"):
        import canals
        if '--canal-debug' in sys.argv:
            canals.DEBUG = {}
        canal_w, canal_fill, canal_depth, rpt = canals.cut(polys, f"{raw}/{P['canals']}", F.to_game, x0, z0, fnx, fnz, fc, fine)
        land = (land | canal_fill) & ~canal_w
        land &= ~(canal_depth > 0)
        fine = np.where(canal_depth > 0, np.minimum(fine, -canal_depth), fine)
        # what is left of the canals that were not kept: puddles of a few cells among the streets become land
        wl, wc = ndimage.label(~land)
        if wc:
            ix = np.arange(1, wc + 1)
            small = (ndimage.sum(np.ones_like(wl), wl, index=ix) * fc * fc < 900) & (ndimage.minimum(rpt['near'], wl, index=ix) > 0)
            puddle = np.concatenate([[False], small])[wl]
            land |= puddle
            rpt['puddles'] = int(small.sum())
        if canals.DEBUG:
            D = canals.DEBUG
            rgb = np.zeros(D['land'].shape + (3,), np.uint8)
            rgb[...] = (150, 200, 225)
            rgb[D['land']] = (222, 208, 160)
            rgb[D['narrow']] = (120, 150, 235)
            rgb[D['fill']] = (235, 150, 120)
            rgb[D['skel']] = (255, 255, 255)
            rgb[D['canal']] = (200, 0, 0)
            np.save(sys.argv[sys.argv.index('--canal-debug') + 1], rgb)
        # for the game: "width n x z x z ..." a line (KeysGeoReal reads it; KeysGeo cuts the banks square, KeysCanals walls them)
        with open(f'{out}/{phase}_canals.txt', 'w') as cf:
            cf.write('# canal centre lines, game metres: width, then x z pairs (Tools/geo/canals.py)\n')
            for pts in rpt['out_lines']:
                cf.write('%g %s\n' % (canals.WIDTH, ' '.join('%.1f %.1f' % q for q in pts)))
            cf.write('# the ways out to deep water: w, width, then x z pairs\n')
            for pts in rpt['ways']:
                cf.write('w %g %s\n' % (canals.DREDGE_W, ' '.join('%.1f %.1f' % q for q in pts)))
        canal_note = ('canals: %(lines)d centre lines kept, %(metres).0f m; dropped %(short)d short and %(crowded)d crowded; %(cut_cells)d fine cells of land cut, '
                      '%(fill_cells)d filled; %(dredged)d mouths dredged to deep water, %(no_way_out)d with none in reach; %(bridged)d breaks joined, %(puddles)d puddles filled' % rpt)

    # ---- marked channels the grid has pinched shut: where the line between two consecutive daybeacons of a channel
    # (the legs the game dredges, KeysGeo BuildNavSegs) only just clips land - under CUT_MAX metres of it - the land is
    # cut CUT_HALF_W either side of the line. Needs <phase>_marks.txt from an earlier bake of this phase; longer
    # crossings (a causeway, an island) are left alone.
    cut_note = ''
    marks_path = f'{out}/{phase}_marks.txt'
    if P.get('cut_marks') and os.path.exists(marks_path):
        chans = {}
        for ln in open(marks_path):
            v = ln.split(None, 5)
            if ln.startswith('#') or len(v) < 6 or v[2] not in 'RG':
                continue
            nm = v[5].strip()
            for word in (' Daybeacon', ' Light', ' Buoy'):
                k = nm.rfind(word)
                if k > 0:
                    nm = nm[:k]
            chans.setdefault(nm, []).append((int(v[3]), float(v[0]), float(v[1])))
        # 'clips land' counts the water within a fine cell of the shore too: a channel that narrow closes on the 10 m grid
        tight = ndimage.binary_dilation(land, iterations=1)
        cuts = []
        for nm, ms in sorted(chans.items()):
            ms.sort()
            for (na, ax, az), (nb, bx, bz) in zip(ms[:-1], ms[1:]):
                L = math.hypot(bx - ax, bz - az)
                if L > 420 or L < 1:
                    continue
                t = np.linspace(0, 1, max(2, int(L / (fc / 2))))
                li = np.clip(np.round((ax + (bx - ax) * t - x0) / fc).astype(int), 0, fnx - 1)
                lj = np.clip(np.round((az + (bz - az) * t - z0) / fc).astype(int), 0, fnz - 1)
                dry = float(tight[lj, li].sum() * L / len(t))
                if 0 < dry < CUT_MAX:
                    i0, i1 = max(0, li.min() - 4), min(fnx, li.max() + 5)
                    j0, j1 = max(0, lj.min() - 4), min(fnz, lj.max() + 5)
                    QX, QZ = np.meshgrid(x0 + np.arange(i0, i1) * fc, z0 + np.arange(j0, j1) * fc)
                    tt = np.clip(((QX - ax) * (bx - ax) + (QZ - az) * (bz - az)) / (L * L), 0, 1)
                    near_line = np.hypot(QX - ax - (bx - ax) * tt, QZ - az - (bz - az) * tt) <= CUT_HALF_W
                    land[j0:j1, i0:i1] &= ~near_line
                    fine[j0:j1, i0:i1] = np.where(near_line, np.minimum(fine[j0:j1, i0:i1], -1.5), fine[j0:j1, i0:i1])
                    cuts.append(f'{nm} {na}-{nb} ({dry:.0f} m)')
        cut_note = f'{len(cuts)} channel legs cut through' + (': ' + '; '.join(cuts) if cuts else '')

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
    extra = json.load(open(f'{raw}/osm_{src}_extra.json'))['elements']
    us1 = []
    for el in extra:
        t = el.get('tags', {})
        if el['type'] == 'way' and t.get('highway') == 'trunk' and 'geometry' in el:
            us1 += [F.to_game(p['lon'], p['lat']) for p in el['geometry']]
    chain_note = ''
    if P.get('chain'):
        xmax = P['chain']['xmax']
        us1 = [q for q in us1 if q[0] <= xmax]
        # past the road: the middle of the island named (the one that point is on), every 50 m of x (the widest stretch
        # of land on that line)
        on = Point(*F.to_game(*P['chain']['on']))
        big = [q for q in polys if q.contains(on)][0]
        mnx, mnz, mxx, mxz = big.bounds
        last, n_axis = None, 0
        for ax_x in np.arange(xmax + 50, x1 + 50, 50.0):
            cut = big.intersection(LineString([(ax_x, mnz - 10), (ax_x, mxz + 10)])) if ax_x <= mxx else None
            parts = [g for g in (cut.geoms if hasattr(cut, 'geoms') else [cut]) if g.geom_type == 'LineString'] if cut is not None and not cut.is_empty else []
            if parts:
                seg = max(parts, key=lambda g: g.length)
                last = (seg.coords[0][1] + seg.coords[-1][1]) / 2
                n_axis += 1
            if last is not None:
                us1.append((float(ax_x), float(last)))
        chain_note = f'chain line: US-1 to x {xmax:.0f}, then the middle of the island it is on ({big.area / 1e4:.0f} ha) for {n_axis} samples to x {min(mxx, x1):.0f}, held from there'
    us1 = np.array(sorted(us1))
    step = 50.0
    cx = np.arange(rx0, rx1 + step, step)
    # densify between consecutive points so long bridge spans don't leave holes, then bin by x
    order = np.argsort(us1[:, 0])
    ux, uz = us1[order, 0], us1[order, 1]
    keep = np.concatenate([[True], np.diff(ux) > .01])
    ux, uz = ux[keep], uz[keep]
    cz = np.interp(cx, ux, uz)
    if P.get('clip'):
        # smoothed with the road beyond both ends in view (Phase 1's was smoothed against its own ends)
        cpad = np.arange(rx0 - 12 * step, rx1 + 12.5 * step, step)
        cz = ndimage.gaussian_filter1d(np.interp(cpad, ux, uz), 3, mode='nearest')[12:12 + len(cx)]
    else:
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
    id_img = Image.new('L', (fnx, fnz), 0)
    id_draw = ImageDraw.Draw(id_img)
    for p in sorted(polys, key=lambda q: -q.area):
        c = p.representative_point()
        if (id(p) not in mine) if P.get('clip') else not (rx0 <= c.x <= rx1 and rz0 <= c.y <= rz1):
            continue
        nm = ''
        for name, lon, lat in KNOWN:
            if p.contains(Point(*F.to_game(lon, lat))):
                nm = name
                break
        if not nm:
            # otherwise the OSM island name whose label sits nearest this outline's middle
            best = 1e18
            for name, (px, pz) in names:
                if p.contains(Point(px, pz)) or p.distance(Point(px, pz)) < 25:
                    d = (px - p.centroid.x) ** 2 + (pz - p.centroid.y) ** 2
                    if d < best:
                        best, nm = d, name
        if P.get('gen', 2) >= 4 and id(p) in boxed:
            nm = ''     # (the mainland: an island's name tag that happens to sit on it is not its name, and it stays unnamed
                        # so the game, which builds houses on named keys, leaves the Everglades alone)
        sp = p.simplify(1.5)
        if sp.is_empty or sp.geom_type != 'Polygon':
            sp = p
        mnx, mnz, mxx, mxz = p.bounds
        if len(islands) < 254:
            id_draw.polygon([((x - x0) / fc, (z - z0) / fc) for x, z in p.exterior.coords], fill=len(islands) + 1)
        islands.append(dict(name=nm, x=round(p.centroid.x, 1), z=round(p.centroid.y, 1), area=round(p.area), bounds=[round(v, 1) for v in (mnx, mnz, mxx, mxz)],
                            ring=[round(v, 1) for xy in sp.exterior.coords for v in xy]))

    islands = islands[:254]
    ids = np.array(id_img)
    near = ndimage.distance_transform_edt(ids == 0, return_distances=False, return_indices=True)
    isl_id = (ids[tuple(near)][::FINE, ::FINE] - 1).astype(np.uint8)   # the nearest island's index, everywhere

    # ---- back to the phase's own rectangle
    if PAD:
        height, shore, landg, isl_id = (a[PAD:-PAD, PAD:-PAD] for a in (height, shore, landg, isl_id))
        x0, x1, z0, z1 = rx0, rx1, rz0, rz1
        nx, nz = int(round((x1 - x0) / CELL)) + 1, int(round((z1 - z0) / CELL)) + 1
        assert height.shape == (nz, nx)
    if P.get('join'):
        height, shore = height.astype(np.float64), np.clip(shore.astype(np.float64), -3276, 3276)   # as the file keeps it

    # ---- the edges shared with phases already baked: take their values along the edge, fade the difference out
    join_notes = []
    for other, side in P.get('join', []):
        g = KGeo(f'{out}/{other}.kgeo')
        gx1, gz1 = g.x0 + (g.nx - 1) * g.cell, g.z0 + (g.nz - 1) * g.cell
        along_x = side in ('south', 'north')              # the shared edge runs along x
        n_along = nx if along_x else nz
        a0, o0, on = (x0, g.x0, g.nx) if along_x else (z0, g.z0, g.nz)
        k0 = int(round((o0 - a0) / CELL))                 # the other grid's first cell, in this grid's cells along the edge
        lo, hi = max(0, k0), min(n_along, k0 + on)        # the overlap
        assert hi - lo > 1, f'{phase} does not touch {other}'
        osl = slice(lo - k0, hi - k0)
        if side == 'west':
            assert abs(gx1 - x0) < 1e-6; theirs = (g.height[osl, -1], g.shore[osl, -1])
        elif side == 'east':
            assert abs(g.x0 - x1) < 1e-6; theirs = (g.height[osl, 0], g.shore[osl, 0])
        elif side == 'south':
            assert abs(g.z0 - z1) < 1e-6; theirs = (g.height[0, osl], g.shore[0, osl])
        else:
            assert abs(gz1 - z0) < 1e-6; theirs = (g.height[-1, osl], g.shore[-1, osl])
        fade = np.clip(1 - np.arange(JOIN_FADE + 1) / JOIN_FADE, 0, 1)
        for arr, their, label in ((height, theirs[0], 'height'), (shore, theirs[1], 'shore')):
            edge = {'west': arr[:, 0], 'east': arr[:, -1], 'south': arr[-1, :], 'north': arr[0, :]}[side]
            d = np.zeros(n_along)
            d[lo:hi] = their - edge[lo:hi]
            worst = float(np.abs(d).max())
            # past the end of the shared edge the correction dies away along the edge too
            if lo > 0:
                d[max(0, lo - JOIN_FADE):lo] = d[lo] * np.linspace(0, 1, JOIN_FADE + 1)[:-1][-min(lo, JOIN_FADE):]
            if hi < n_along:
                m = min(n_along - hi, JOIN_FADE)
                d[hi:hi + m] = d[hi - 1] * np.linspace(1, 0, JOIN_FADE + 1)[1:][:m]
            for k in range(JOIN_FADE + 1):
                if side == 'west': arr[:, k] += d * fade[k]
                elif side == 'east': arr[:, nx - 1 - k] += d * fade[k]
                elif side == 'south': arr[nz - 1 - k, :] += d * fade[k]
                else: arr[k, :] += d * fade[k]
            join_notes.append(f'{label} off by up to {worst:.2f} m before the join to {other} ({side})')
    if P.get('join'):
        # still land >= 0.2 and water <= -0.3, by the (joined) shore distance
        landg = shore < 0
        height = np.where(landg, np.maximum(height, .2), np.minimum(height, -.3))
        # and US-1's line meets the line next door
        if isinstance(P.get('chain_from'), list):
            # the line of every phase south of this strip, each over its own stretch of x
            cz = np.zeros(len(cx))
            for other in reversed(P['chain_from']):
                g = KGeo(f'{out}/{other}.kgeo')
                m = (cx >= g.x0 - 1) & (cx <= g.x0 + (g.nx - 1) * g.cell + 1)
                cz[m] = np.asarray(g.chain_z(cx[m]), np.float64)
        else:
            g = KGeo(f'{out}/{P.get("chain_from", P["join"][0][0])}.kgeo')
        if isinstance(P.get('chain_from'), list):
            pass
        elif P.get('chain_from'):
            cz = np.asarray(g.chain_z(cx), np.float64)
        elif P.get('chain_end') == 'east':
            # the line next door is past this phase's east end
            dz0 = float(g.chain_z(cx[-1])) - cz[-1]
            cz = cz + dz0 * np.clip(1 - np.arange(len(cz))[::-1] / 16.0, 0, 1)
            join_notes.append(f'US-1 line off by {dz0:.1f} m before the join')
        else:
            dz0 = float(g.chain_z(cx[0])) - cz[0]
            cz = cz + dz0 * np.clip(1 - np.arange(len(cz)) / 16.0, 0, 1)
            join_notes.append(f'US-1 line off by {dz0:.1f} m before the join')

    in_rect = lambda x, z: x0 <= x <= x1 and z0 <= z <= z1

    def ways(pred, clip=False):
        outl = []
        for el in extra:
            t = el.get('tags', {})
            if el['type'] == 'way' and 'geometry' in el and pred(t):
                g = [F.to_game(p['lon'], p['lat']) for p in el['geometry']]
                if clip and P.get('clip') and not any(in_rect(*q) for q in g):
                    continue
                outl.append(dict(name=t.get('name', ''), pts=[round(v, 1) for xy in g for v in xy]))
        return outl

    def centre(el):
        if el['type'] == 'node':
            return F.to_game(el['lon'], el['lat'])
        g = el.get('geometry') or [q for m in el.get('members', []) for q in m.get('geometry', [])]
        if not g:
            return None
        return F.to_game(sum(q['lon'] for q in g) / len(g), sum(q['lat'] for q in g) / len(g))

    more = {}
    if P.get('clip'):
        # each US-1 bridge once: the trunk road's bridge ways joined end to end, named from the bridge outline beside
        # them (man_made=bridge) or from BRIDGE_NAMES
        spans = []
        for el in extra:
            t = el.get('tags', {})
            if el['type'] == 'way' and 'geometry' in el and t.get('highway') == 'trunk' and t.get('bridge'):
                g = sorted(F.to_game(q['lon'], q['lat']) for q in el['geometry'])
                if P.get('gen', 2) >= 4 and not any(in_rect(*q) for q in g):
                    continue
                for sp in spans:
                    # (from Key Largo on the road also runs up the map: spans one above the other are not one bridge)
                    if P.get('gen', 2) >= 4 and min(abs(g[0][1] - sp[0][1]), abs(g[0][1] - sp[1][1]), abs(g[-1][1] - sp[0][1]), abs(g[-1][1] - sp[1][1])) > 300:
                        continue
                    if g[0][0] <= sp[1][0] + 40 and g[-1][0] >= sp[0][0] - 40:
                        if g[0][0] < sp[0][0]: sp[0] = g[0]
                        if g[-1][0] > sp[1][0]: sp[1] = g[-1]
                        break
                else:
                    spans.append([g[0], g[-1]])
        outlines = [(RENAME.get(el['tags']['name'], el['tags']['name']), centre(el)) for el in extra if el.get('tags', {}).get('man_made') == 'bridge' and el['tags'].get('name') and centre(el)]
        bridges, used = [], set()
        for a, b in sorted(spans):
            mx, mz = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
            if not (in_rect(*a) or in_rect(*b) or in_rect(mx, mz)):
                continue
            nm, best = '', 1e18
            for name, (px, pz) in outlines:
                d = (px - mx) ** 2 + (pz - mz) ** 2
                if a[0] - 60 <= px <= b[0] + 60 and abs(pz - mz) < 80 and not name.startswith('Old') and d < best:
                    nm, best = name, d
            for name, lon, lat in BRIDGE_NAMES:
                px, pz = F.to_game(lon, lat)
                if not nm and a[0] - 150 <= px <= b[0] + 150 and abs(pz - mz) < 200:
                    nm = name
            used.add(nm)
            bridges.append(dict(name=nm, a=[round(a[0], 1), round(a[1], 1)], b=[round(b[0], 1), round(b[1], 1)], length=round(math.hypot(b[0] - a[0], b[1] - a[1]), 1)))
        more['bridges'] = bridges
        more['other_bridges'] = [dict(name=name, pts=[round(px, 1), round(pz, 1)]) for name, (px, pz) in outlines if name not in used and in_rect(px, pz)]
        # reefs and lights: OpenStreetMap's, and the ones known by name, each with the depth the grid has there
        cell_h = lambda x, z: float(height[int(round((z - z0) / CELL)), int(round((x - x0) / CELL))])
        reefs = []
        for el in extra:
            t = el.get('tags', {})
            c = centre(el) if t.get('natural') == 'reef' else None
            if c and in_rect(*c):
                reefs.append(dict(name=t.get('name', ''), pts=[round(c[0], 1), round(c[1], 1)], depth=round(-cell_h(*c), 1)))
        for name, lon, lat in KNOWN_REEFS:
            c = F.to_game(lon, lat)
            if in_rect(*c) and not any(r['name'].lower().startswith(name.lower()) for r in reefs):
                # the shoalest water within 250 m of the charted name is the reef's crest
                j, i = int(round((c[1] - z0) / CELL)), int(round((c[0] - x0) / CELL))
                win = height[max(0, j - 25):j + 26, max(0, i - 25):i + 26]
                jj, ii = np.unravel_index(np.argmax(win), win.shape)
                reefs.append(dict(name=name, pts=[round(x0 + (max(0, i - 25) + ii) * CELL, 1), round(z0 + (max(0, j - 25) + jj) * CELL, 1)], depth=round(-float(win.max()), 1)))
        lights = []
        for el in extra:
            t = el.get('tags', {})
            c = centre(el) if t.get('man_made') == 'lighthouse' else None
            if c and in_rect(*c):
                lights.append(dict(name=t.get('name', ''), pts=[round(c[0], 1), round(c[1], 1)], depth=round(-cell_h(*c), 1)))
        for name, lon, lat in KNOWN_LIGHTS:
            c = F.to_game(lon, lat)
            if in_rect(*c) and not any(l['name'] == name for l in lights):
                if P.get('gen', 2) >= 3:
                    # on the shoalest water within 250 m, as the reef of the same name is
                    j, i = int(round((c[1] - z0) / CELL)), int(round((c[0] - x0) / CELL))
                    win = height[max(0, j - 25):j + 26, max(0, i - 25):i + 26]
                    jj, ii = np.unravel_index(np.argmax(np.where(win < 0, win, -1e9)), win.shape)
                    c = (x0 + (max(0, i - 25) + ii) * CELL, z0 + (max(0, j - 25) + jj) * CELL)
                lights.append(dict(name=name, pts=[round(c[0], 1), round(c[1], 1)], depth=round(-cell_h(*c), 1)))
        more['reef_sites'] = reefs
        more['lights'] = lights
        if P.get('gen', 2) >= 4:
            waters = []
            for name, lon, lat in KNOWN_WATERS:
                c = F.to_game(lon, lat)
                if in_rect(*c):
                    # (the point is from memory of the chart: moved to the nearest water a metre deep within 800 m)
                    j, i = int(round((c[1] - z0) / CELL)), int(round((c[0] - x0) / CELL))
                    ja, ia = max(0, j - 80), max(0, i - 80)
                    win = height[ja:j + 81, ia:i + 81]
                    jj, ii = np.nonzero(win <= -1.0)
                    if len(jj):
                        k = int(np.argmin((jj + ja - j) ** 2 + (ii + ia - i) ** 2))
                        c = (x0 + (ii[k] + ia) * CELL, z0 + (jj[k] + ja) * CELL)
                    waters.append(dict(name=name, pts=[round(c[0], 1), round(c[1], 1)], depth=round(-cell_h(*c), 1)))
            if waters:
                more['waters'] = waters
            side = []
            for el in extra:
                t = el.get('tags', {})
                if t.get('man_made') == 'bridge' and t.get('name') in SIDE_BRIDGES and el.get('geometry'):
                    q = [F.to_game(v['lon'], v['lat']) for v in el['geometry']]
                    a, b = max(((u, v) for u in q for v in q), key=lambda uv: (uv[0][0] - uv[1][0]) ** 2 + (uv[0][1] - uv[1][1]) ** 2)
                    if in_rect(*a) or in_rect(*b):
                        a, b = sorted((a, b))
                        side.append(dict(name=t['name'], a=[round(a[0], 1), round(a[1], 1)], b=[round(b[0], 1), round(b[1], 1)], length=round(math.hypot(b[0] - a[0], b[1] - a[1]), 1)))
            if side:
                more['side_bridges'] = side
        if P.get('road'):
            # where US-1 itself runs on this phase's stretch of the chain line (sites_p3.py passes it to the game)
            rw, re_ = P['road'] if isinstance(P['road'], tuple) else (float(ux.min()), float(ux.max()))
            more['road'] = [round(max(rx0, rw), 1), round(min(rx1, re_), 1)]

    places = dict(
        phase=phase, **(dict(src=src) if P.get('gen', 2) >= 3 else {}), rect=[x0, x1, z0, z1], scale=F.SCALE, theta_deg=math.degrees(F.THETA),
        attribution='Coastlines, roads and place names (c) OpenStreetMap contributors, ODbL 1.0. Elevation and bathymetry: NOAA NCEI / USGS (public domain).',
        islands=islands,
        us1_bridges=ways(lambda t: t.get('highway') == 'trunk' and t.get('bridge'), True),
        old_bridges=ways(lambda t: t.get('bridge') and t.get('name') in ('Old Seven Mile Bridge', 'Bahia Honda Bridge')),
        mangroves=ways(lambda t: t.get('wetland') == 'mangrove'),
        marinas=ways(lambda t: t.get('leisure') == 'marina', True) + [dict(name=el['tags'].get('name', ''), pts=[round(v, 1) for v in F.to_game(el['lon'], el['lat'])])
                                                                     for el in extra if el['type'] == 'node' and el.get('tags', {}).get('leisure') == 'marina'
                                                                     and (not P.get('clip') or in_rect(*F.to_game(el['lon'], el['lat'])))],
        reefs=ways(lambda t: t.get('natural') == 'reef', True),
        runways=ways(lambda t: t.get('aeroway') == 'runway', True),
        localities=[dict(name=el['tags']['name'], kind=el['tags']['place'], pts=[round(v, 1) for v in F.to_game(el['lon'], el['lat'])])
                    for el in extra if el['type'] == 'node' and el.get('tags', {}).get('place') and el['tags'].get('name')
                    and (not P.get('clip') or in_rect(*F.to_game(el['lon'], el['lat'])))],
        **more,
    )

    if P.get('gen', 2) >= 3:
        # OpenStreetMap's marina names are real businesses' names, which the project does not use: the sites are kept,
        # the names are not (sites_p3.py gives each an invented one)
        for m in places['marinas']:
            m['name'] = ''
    os.makedirs(out, exist_ok=True)
    with open(f'{out}/{phase}.kgeo', 'wb') as f:
        f.write(b'KGEO' + struct.pack('<i', 2))
        f.write(struct.pack('<dddii', x0, z0, CELL, nx, nz))
        f.write(struct.pack('<dddddd', F.SCALE, F.THETA, F.LAT0, F.LON0, F.ANCHOR_X, F.ANCHOR_Z))
        f.write(struct.pack('<idd', len(cz), float(cx[0]), step))
        f.write(np.clip(np.round(height * 100), -32000, 32000).astype('<i2').tobytes())
        f.write(np.clip(np.round(shore * 10), -32760, 32760).astype('<i2').tobytes())
        f.write(cz.astype('<f4').tobytes())
        f.write(isl_id.tobytes())
        f.write(struct.pack('<i', len(islands)))
        for i in islands:
            nm = i['name'].encode('utf8')
            f.write(struct.pack('<i', len(nm)) + nm)
            f.write(struct.pack('<ddd4d', i['x'], i['z'], float(i['area']), *i['bounds']))
            f.write(struct.pack('<i', len(i['ring'])))
            f.write(np.array(i['ring'], '<f4').tobytes())
    json.dump(places, open(f'{out}/{phase}_places.json', 'w'), separators=(',', ':'))

    wet = ~landg
    print(f'{phase}: grid {nx} x {nz} at {CELL} m, {os.path.getsize(f"{out}/{phase}.kgeo") / 1e6:.1f} MB')
    print(f'  land {landg.mean() * 100:.2f}%  islands {len(islands)} ({sum(1 for i in islands if i["name"])} named)  highest {height.max():.1f} m  deepest {-height.min():.1f} m')
    print(f'  water under 1 m {((height > -1) & wet).sum() / wet.sum() * 100:.1f}%  under 2 m {((height > -2) & wet).sum() / wet.sum() * 100:.1f}%  over 45 m {(height < -45).sum() / wet.sum() * 100:.1f}%')
    print(f'  US-1: z from {cz.min():.0f} to {cz.max():.0f}; bridges {len(places["us1_bridges"])}, mangrove areas {len(places["mangroves"])}, marinas {len(places["marinas"])}, reefs {len(places["reefs"])}')
    if P.get('clip'):
        print('  US-1 bridges: ' + '; '.join(f'{b["name"] or "?"} {b["a"][0]:.0f}..{b["b"][0]:.0f}' for b in places['bridges']))
        print('  reefs: ' + '; '.join(f'{r["name"] or "?"} {r["depth"]:.1f} m' for r in places['reef_sites']) + ' | lights: ' + '; '.join(f'{l["name"]} {l["depth"]:.1f} m' for l in places['lights']))
        print('  biggest islands: ' + '; '.join(f'{i["name"] or "?"} {i["area"] / 1e4:.0f} ha' for i in islands[:14]))
    if P.get('clip'):
        print('  islands cut by the phase edge: ' + ('; '.join(f'{i["name"] or "?"} at {i["x"]:.0f}, {i["z"]:.0f}' for i in islands if i['bounds'][0] < x0 or i['bounds'][2] > x1 or i['bounds'][1] < z0 or i['bounds'][3] > z1) or 'none'))
    if chain_note:
        print('  ' + chain_note)
    if places.get('side_bridges'):
        print('  bridges off the chain line: ' + '; '.join(f'{b["name"]} {b["a"][0]:.0f}, {b["a"][1]:.0f} to {b["b"][0]:.0f}, {b["b"][1]:.0f}' for b in places['side_bridges']))
    if places.get('waters'):
        print('  waters: ' + '; '.join(f'{w["name"]} {w["depth"]:.1f} m' for w in places['waters']))
    if shoal_note:
        print('  shoals: ' + shoal_note)
    if cut_note:
        print('  ' + cut_note)
    if canal_note:
        print('  ' + canal_note)
    for note in join_notes:
        print('  join: ' + note)

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
