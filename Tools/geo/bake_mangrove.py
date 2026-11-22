"""bake_mangrove.py <phase> <osm wetland json> <geo dir> [--built <osm streets json>] [--preview png]: where the
mangroves stand on a baked phase.

OpenStreetMap has no wetland=mangrove here, but its coastal natural=wetland and shore-side natural=wood areas in the
Keys are mangrove, and so are the small unnamed keys and the low, unbuilt shores. Writes <geo dir>/<phase>_mangrove.bin:
'KMGV', int32 nx, int32 nz, then nx*nz bytes on the .kgeo grid: 0 none .. 255 solid mangrove; values 1..127 are the
fringe standing in the water (prop roots, no ground under them).

The mangrove creeks (the CREEKS table in Source/KeysRun/KeysMangroveLife.cpp, which the game cuts through the ground)
get mangrove down both banks: land within CORRIDOR metres of a creek's edge, estate lots (<phase>_plat.txt) left out.

--built names an OpenStreetMap extract of residential streets and residential/commercial land use (fetch_p2.sh writes
osm_p2_built.json). A low shore within BUILT_REACH metres of one is a seawall or a lawn, not mangrove, unless OSM maps
it as wetland or wood. Without it only the islands named in BUILT count as built-up (Phase 1 was baked that way)."""
import json, os, re, struct, sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

sys.path.insert(0, os.path.dirname(__file__))
from frame import to_game
from kgeo import KGeo

# keys with seawalls, lawns and canals: only what OSM maps as wetland or wood counts there
BUILT = {'Marathon (Vaca Key)', 'Key Colony Beach', 'Duck Key', 'Walkers Island', 'Pigeon Key', 'Ohio Key', 'Grassy Key'}


CORRIDOR = 34
BUILT_REACH = 40       # game metres (the map is compressed 1:3, so about a real block)
CREEK_SRC = os.path.join(os.path.dirname(__file__), '..', '..', 'Source', 'KeysRun', 'KeysMangroveLife.cpp')


def read_creeks(path=CREEK_SRC):
    """[(name, width, [(x, z), ...])] as written between CREEKS-BEGIN and CREEKS-END (the corners are not rounded here)."""
    if not os.path.exists(path):
        return []
    src = open(path).read()
    if 'CREEKS-BEGIN' not in src:
        return []
    block = src.split('CREEKS-BEGIN')[1].split('CREEKS-END')[0]
    out = []
    for name, w, pts in re.findall(r'Add\(TEXT\("([^"]+)"\), ([-\d.e]+), [-\d.e]+, [-\d.e]+, \{(.*)\}\);', block):
        out.append((name, float(w), [(float(a), float(b)) for a, b in re.findall(r'\{(-?[\d.]+), (-?[\d.]+)\}', pts)]))
    return out


def creek_edge_dist(g, creeks):
    """metres outside the nearest creek's edge for every cell (negative in the channel), 1e9 far away"""
    best = np.full((g.nz, g.nx), 1e9)
    for _, w, pts in creeks:
        xs, zs = [p[0] for p in pts], [p[1] for p in pts]
        pad = w + CORRIDOR + 20
        i0, i1 = max(0, int((min(xs) - pad - g.x0) / g.cell)), min(g.nx, int((max(xs) + pad - g.x0) / g.cell) + 1)
        j0, j1 = max(0, int((min(zs) - pad - g.z0) / g.cell)), min(g.nz, int((max(zs) + pad - g.z0) / g.cell) + 1)
        if i1 <= i0 or j1 <= j0:
            continue
        X, Z = np.meshgrid(g.x0 + np.arange(i0, i1) * g.cell, g.z0 + np.arange(j0, j1) * g.cell)
        for (ax, az), (bx, bz) in zip(pts[:-1], pts[1:]):
            vx, vz = bx - ax, bz - az
            t = np.clip(((X - ax) * vx + (Z - az) * vz) / max(1e-9, vx * vx + vz * vz), 0, 1)
            d = np.hypot(X - ax - vx * t, Z - az - vz * t) - w / 2
            best[j0:j1, i0:i1] = np.minimum(best[j0:j1, i0:i1], d)
    return best


def lot_mask(g, path):
    """cells on (or within a cell of) an estate lot"""
    im = Image.new('L', (g.nx, g.nz), 0)
    if os.path.exists(path):
        d = ImageDraw.Draw(im)
        for ln in open(path):
            if ln.startswith('#') or ln.count('|') < 9:
                continue
            v = [float(t) for t in ln.strip().split('|')[9].split()]
            d.polygon([((v[k] - g.x0) / g.cell, (v[k + 1] - g.z0) / g.cell) for k in range(0, len(v), 2)], fill=255)
    return ndimage.maximum_filter(np.asarray(im), size=3) > 0


def built_mask(g, path):
    """cells within BUILT_REACH of a residential street or of residential/commercial land"""
    im = Image.new('L', (g.nx, g.nz), 0)
    d = ImageDraw.Draw(im)
    n = [0, 0]
    for e in json.load(open(path))['elements']:
        rings = []
        if e['type'] == 'way' and 'geometry' in e:
            rings = [e['geometry']]
        elif e['type'] == 'relation':
            rings = [m['geometry'] for m in e.get('members', []) if m.get('role') == 'outer' and 'geometry' in m]
        area = 'landuse' in e.get('tags', {}) or e['type'] == 'relation'
        for r in rings:
            px = []
            for p in r:
                x, z = to_game(p['lon'], p['lat'])
                px.append(((x - g.x0) / g.cell, (z - g.z0) / g.cell))
            if (area or ('tags' not in e and px[0] == px[-1])) and len(px) >= 3:     # (a download without tags: a closed way is an area)
                d.polygon(px, fill=255); n[1] += 1
            elif len(px) >= 2:
                d.line(px, fill=255, width=1); n[0] += 1
    near = ndimage.distance_transform_edt(np.asarray(im) == 0) * g.cell < BUILT_REACH
    return near, n


def main():
    phase, osm, geodir = sys.argv[1:4]
    streets = sys.argv[sys.argv.index('--built') + 1] if '--built' in sys.argv else None
    preview = sys.argv[sys.argv.index('--preview') + 1] if '--preview' in sys.argv else None
    g = KGeo(os.path.join(geodir, phase + '.kgeo'))
    nx, nz = g.nx, g.nz
    wet = Image.new('L', (nx, nz), 0)
    wood = Image.new('L', (nx, nz), 0)
    dw, dd = ImageDraw.Draw(wet), ImageDraw.Draw(wood)
    n = [0, 0]
    for e in json.load(open(osm))['elements']:
        t = e.get('tags', {})
        rings = []
        if e['type'] == 'way' and 'geometry' in e:
            rings = [e['geometry']]
        elif e['type'] == 'relation':
            rings = [m['geometry'] for m in e.get('members', []) if m.get('role') == 'outer' and 'geometry' in m]
        for r in rings:
            px = []
            for p in r:
                x, z = to_game(p['lon'], p['lat'])
                px.append(((x - g.x0) / g.cell, (z - g.z0) / g.cell))
            if len(px) < 3:
                continue
            if t.get('natural') == 'wetland':
                dw.polygon(px, fill=255); n[0] += 1
            else:
                dd.polygon(px, fill=255); n[1] += 1
    wet = np.asarray(wet) > 0
    wood = np.asarray(wood) > 0
    land = g.shore < 0
    S, H = g.shore, g.height
    small = np.zeros(len(g.islands), bool)      # little unnamed keys: mangrove from shore to shore
    built = np.zeros(len(g.islands), bool)
    for i, isl in enumerate(g.islands):
        small[i] = (not isl['name'] and isl['area'] < 30000) or (isl['area'] < 9000 and isl['name'] not in BUILT)
        built[i] = isl['name'] in BUILT
    idx = np.minimum(g.island, len(g.islands) - 1)
    m = np.zeros((nz, nx), np.uint8)
    m[land & wet] = 255
    m[land & wood & (S > -60)] = np.maximum(m[land & wood & (S > -60)], 210)
    m[land & small[idx]] = 255
    low = land & (S > -28) & (H < .75) & ~built[idx]            # low unbuilt shore
    nbuilt = [0, 0, 0]
    if streets:
        bm, nb = built_mask(g, streets)
        nbuilt = [nb[0], nb[1], int((low & bm & (m < 200)).sum())]
        low &= ~bm
    m[low] = np.maximum(m[low], 230)
    # the creeks: mangrove down both banks (and over the line itself, which is ground until the game cuts it)
    creeks = read_creeks()
    ncorr = 0
    if creeks:
        cd = creek_edge_dist(g, creeks)
        corr = land & (cd < CORRIDOR) & ~lot_mask(g, os.path.join(geodir, phase + '_plat.txt'))
        ncorr = int((corr & (m < 200)).sum())
        m[corr] = 255
    # the fringe: mangrove wading out over the shallows next to mangrove shore
    near = ndimage.maximum_filter((m >= 200).astype(np.uint8), size=3) > 0
    fringe = (~land) & near & (S < 14) & (-H < .9)
    m[fringe] = 110
    path = os.path.join(geodir, phase + '_mangrove.bin')
    with open(path, 'wb') as f:
        f.write(b'KMGV' + struct.pack('<ii', nx, nz) + m.tobytes())
    print('OSM wetlands %d, woods %d; mangrove land %.2f km2 (of %.2f km2 land), fringe cells %d, %d creeks with %d new bank cells, %d streets and %d built areas keep %d low shore cells clear -> %s' % (
        n[0], n[1], (m >= 200).sum() * g.cell ** 2 / 1e6, land.sum() * g.cell ** 2 / 1e6, int(fringe.sum()), len(creeks), ncorr, nbuilt[0], nbuilt[1], nbuilt[2], path))
    if preview:
        rgb = np.zeros((nz, nx, 3), np.uint8)
        rgb[...] = (150, 200, 225)
        rgb[land] = (222, 208, 160)
        rgb[m >= 200] = (40, 110, 50)
        rgb[(m > 0) & (m < 200)] = (90, 150, 110)
        if creeks:
            rgb[cd < 0] = (70, 60, 40)       # the channels, tea-dark
        Image.fromarray(rgb).save(preview)


if __name__ == '__main__':
    main()
