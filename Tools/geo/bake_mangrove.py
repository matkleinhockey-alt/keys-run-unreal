"""bake_mangrove.py <phase> <osm wetland json> <geo dir> [--preview png]: where the mangroves stand on a baked phase.

OpenStreetMap has no wetland=mangrove here, but its coastal natural=wetland and shore-side natural=wood areas in the
Keys are mangrove, and so are the small unnamed keys and the low, unbuilt shores. Writes <geo dir>/<phase>_mangrove.bin:
'KMGV', int32 nx, int32 nz, then nx*nz bytes on the .kgeo grid: 0 none .. 255 solid mangrove; values 1..127 are the
fringe standing in the water (prop roots, no ground under them)."""
import json, os, struct, sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

sys.path.insert(0, os.path.dirname(__file__))
from frame import to_game
from kgeo import KGeo

# keys with seawalls, lawns and canals: only what OSM maps as wetland or wood counts there
BUILT = {'Marathon (Vaca Key)', 'Key Colony Beach', 'Duck Key', 'Walkers Island', 'Pigeon Key', 'Ohio Key', 'Grassy Key'}


def main():
    phase, osm, geodir = sys.argv[1:4]
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
    m[low] = np.maximum(m[low], 230)
    # the fringe: mangrove wading out over the shallows next to mangrove shore
    near = ndimage.maximum_filter((m >= 200).astype(np.uint8), size=3) > 0
    fringe = (~land) & near & (S < 14) & (-H < .9)
    m[fringe] = 110
    path = os.path.join(geodir, phase + '_mangrove.bin')
    with open(path, 'wb') as f:
        f.write(b'KMGV' + struct.pack('<ii', nx, nz) + m.tobytes())
    print('OSM wetlands %d, woods %d; mangrove land %.2f km2 (of %.2f km2 land), fringe cells %d -> %s' % (
        n[0], n[1], (m >= 200).sum() * g.cell ** 2 / 1e6, land.sum() * g.cell ** 2 / 1e6, int(fringe.sum()), path))
    if preview:
        rgb = np.zeros((nz, nx, 3), np.uint8)
        rgb[...] = (150, 200, 225)
        rgb[land] = (222, 208, 160)
        rgb[m >= 200] = (40, 110, 50)
        rgb[(m > 0) & (m < 200)] = (90, 150, 110)
        Image.fromarray(rgb).save(preview)


if __name__ == '__main__':
    main()
