"""bake_marks.py <phase> <enc dir> <geo dir>: the real red and green marks, from NOAA ENC Direct (fetch_enc.sh), into the
game frame of an already-baked <geo dir>/<phase>.kgeo. Writes <geo dir>/<phase>_marks.txt, one mark a line:
    x z kind number lit name
kind: R red daybeacon (triangle, even), G green daybeacon (square, odd), r red nun buoy, g green can buoy.
Marks are fixed to the nearest water if the 10 m grid has them ashore, and thinned where the 1:3 map crowds them."""
import glob, json, math, os, re, sys

sys.path.insert(0, os.path.dirname(__file__))
from frame import to_game
from kgeo import KGeo


def load(encdir, name):
    out = []
    for scale in ('harbour', 'approach'):   # the larger-scale chart first: it wins where both have the mark
        f = os.path.join(encdir, '%s_%s.geojson' % (scale, name))
        if os.path.exists(f):
            out += json.load(open(f))['features']
    return out


def main():
    phase, encdir, geodir = sys.argv[1:4]
    g = KGeo(os.path.join(geodir, phase + '.kgeo'))
    x1, z1 = g.x0 + (g.nx - 1) * g.cell, g.z0 + (g.nz - 1) * g.cell
    lights = []
    for f in load(encdir, 'light'):
        lon, lat = f['geometry']['coordinates'][:2]
        lights.append(to_game(lon, lat))
    marks = []
    for layer, kinds in (('beacon_lateral', 'GR'), ('buoy_lateral', 'gr')):
        for f in load(encdir, layer):
            p = f['properties']
            lon, lat = f['geometry']['coordinates'][:2]
            x, z = to_game(lon, lat)
            if not (g.x0 + 160 < x < x1 - 160 and g.z0 + 160 < z < z1 - 160):
                continue
            cat = p.get('CATLAM')
            if cat not in (1, 2):
                continue
            name = (p.get('OBJNAM') or '').strip()
            m = re.search(r'(\d+)[A-Z]?\s*$', name)
            num = int(m.group(1)) if m else 0
            # ashore on the grid: step out to the nearest water
            if g.S(x, z) < 2:
                best = None
                for r in range(5, 45, 5):
                    for k in range(16):
                        a = k * math.pi / 8
                        px, pz = x + r * math.cos(a), z + r * math.sin(a)
                        if g.S(px, pz) >= 2.5 and -g.H(px, pz) > .4:
                            best = (px, pz); break
                    if best: break
                if not best:
                    continue
                x, z = best
            lit = any((x - lx) ** 2 + (z - lz) ** 2 < 8 * 8 for lx, lz in lights)
            marks.append([x, z, kinds[cat - 1], num, lit, name])
    # the same mark from both chart scales, and marks the compression has pushed on top of each other
    out = []
    for m in marks:
        dup = False
        for o in out:
            d2 = (m[0] - o[0]) ** 2 + (m[1] - o[1]) ** 2
            if d2 < 3 * 3 or (d2 < 9 * 9 and m[2] == o[2]) or (m[5] and m[5] == o[5]):
                dup = True
                o[4] = o[4] or m[4]
                break
        if not dup:
            out.append(m)
    path = os.path.join(geodir, phase + '_marks.txt')
    with open(path, 'w') as f:
        f.write('# aids to navigation: NOAA ENC Direct to GIS (public domain). x z kind number lit name\n')
        for x, z, kind, num, lit, name in out:
            f.write('%.1f %.1f %s %d %d %s\n' % (x, z, kind, num, 1 if lit else 0, name))
    n = {k: sum(1 for m in out if m[2] == k) for k in 'RGrg'}
    print('%d marks -> %s  (%s), %d lit, from %d charted' % (len(out), path, n, sum(1 for m in out if m[4]), len(marks)))


if __name__ == '__main__':
    main()
