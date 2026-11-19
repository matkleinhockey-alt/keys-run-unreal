"""plat.py <phase> <geo dir> [--preview png]: survey the Keys Run Estates plat on the real islands of a baked phase.

Waterfront lots are cut from a 38 m band round the shore of the keys people actually build on, one every so often along
the shoreline; small named keys are sold whole as private islands. Parks and refuges (Bahia Honda, No Name, the Pine
keys) are left alone, and so are the highway, the runway and the town. Writes <geo dir>/<phase>_plat.txt:
    id|name|island|tier|side|cx|cz|area|frontage|x0 z0 x1 z1 ...
tier: 1 private island, 2 point, 3 oceanfront, 4 bayfront (AKeysEstates::ETier)."""
import math, os, re, sys

sys.path.insert(0, os.path.dirname(__file__))
from kgeo import KGeo
from shapely.geometry import Polygon, LineString, Point, box
from shapely.ops import substring

DEPTH = 38.0
# island -> how many shore lots to keep
BUILD = {'Marathon (Vaca Key)': 78, 'Grassy Key': 40, 'Boot Key': 30, 'Key Colony Beach': 16, 'Duck Key': 9, 'Ohio Key': 5, 'Walkers Island': 5}
WHOLE = ['Deer Key', 'Toms Harbor Keys', 'Channel Key', 'Shands Key', 'Russel Key', 'Bamboo Key', 'Pigeon Key', 'Cocoanut Key', 'Teakettle Key', 'Hardup Key', 'Friend Key']
RUNWAY = LineString([(1016.7, -21.4), (1524.4, -55.1)]).buffer(40)
TOWN = box(-500 - 135, -70, -500 + 135, 175)        # the town pad and its marina (KeysGeo: the City Marina at x -500)
MARINAS = [Point(-330, 100), Point(-625, -70), Point(-300, -100)]


def slug(s):
    return re.sub(r'[^a-z0-9]+', '-', s.lower().replace(' (vaca key)', '')).strip('-')


def short(s):
    return s.replace(' (Vaca Key)', '')


def main():
    phase, geodir = sys.argv[1:3]
    preview = sys.argv[sys.argv.index('--preview') + 1] if '--preview' in sys.argv else None
    g = KGeo(os.path.join(geodir, phase + '.kgeo'))
    out = []
    seen = {}
    for isl in g.islands:
        name = isl['name']
        P = Polygon(isl['ring']).buffer(0)
        if P.geom_type != 'Polygon' or P.area < 1500:
            continue
        if name in WHOLE and name not in seen:
            seen[name] = 1
            S = P.simplify(2.0)
            out.append(dict(id=slug(name), name=name, island=name, tier=1, side='Whole island', poly=list(S.exterior.coords)[:-1], area=P.area, front=P.exterior.length))
            continue
        if name not in BUILD:
            continue
        ext = P.exterior
        if not ext.is_ccw:
            ext = LineString(list(ext.coords)[::-1])
        L = ext.length
        minx, minz, maxx, maxz = P.bounds
        cands = []
        n = int(L // 62)
        for k in range(n):
            s0, s1 = k * L / n, (k + .86) * L / n     # a gap between neighbours, like a side setback
            seg = substring(ext, s0, s1)
            pts = list(seg.coords)
            if len(pts) < 2:
                continue
            # the back line: the frontage's two ends pushed straight inland
            (ax, az), (bx, bz) = pts[0], pts[-1]
            dx, dz = bx - ax, bz - az
            d = math.hypot(dx, dz)
            if d < 30 or d > 80 or seg.length > d * 1.35:      # a straight enough piece of shore
                continue
            nx, nz = -dz / d, dx / d                             # left of travel = inland for a counter-clockwise ring
            lot = Polygon(pts + [(bx + nx * DEPTH, bz + nz * DEPTH), (ax + nx * DEPTH, az + nz * DEPTH)]).buffer(0)
            lot = lot.intersection(P.buffer(1.5))
            if lot.geom_type == 'MultiPolygon':
                lot = max(lot.geoms, key=lambda q: q.area)
            if lot.geom_type != 'Polygon' or lot.area < 1100 or lot.area > 3400:
                continue
            c = lot.centroid
            # all of it dry land, with open water off the frontage (not a canal 10 m wide), clear of the road, runway, town
            if g.S(c.x, c.y) > -8 or g.S(c.x + nx * 14, c.y + nz * 14) > -3:
                continue
            mx, mz = (ax + bx) / 2, (az + bz) / 2
            if g.S(mx - nx * 45, mz - nz * 45) < 25:
                continue
            cz = float(g.chain_z(c.x))
            if abs(c.y - cz) < DEPTH * .5 + 16 or lot.intersects(RUNWAY) or lot.intersects(TOWN) or any(lot.distance(m) < 70 for m in MARINAS):
                continue
            point = (c.x - minx < 140 or maxx - c.x < 140) and name != 'Marathon (Vaca Key)'
            tier = 2 if point else (3 if c.y > cz else 4)
            cands.append(dict(tier=tier, poly=list(lot.simplify(.8).exterior.coords)[:-1], area=lot.area, front=seg.length, cx=c.x, cz=c.y, geom=lot))
        # thin to the island's quota, spread evenly along the shore, never two overlapping
        want = BUILD[name] if seen.get(name, 0) == 0 else max(4, BUILD[name] // 2)
        seen[name] = seen.get(name, 0) + 1
        keep = []
        step = max(1.0, len(cands) / float(want))
        i = 0.0
        while int(i) < len(cands) and len(keep) < want:
            c = cands[int(i)]
            if not any(c['geom'].distance(o['geom']) < 6 for o in keep):
                keep.append(c)
            i += step
        counts = {}
        for c in sorted(keep, key=lambda q: q['cx']):
            side = {2: 'Point', 3: 'Oceanside', 4: 'Bayside'}[c['tier']]
            tag = {2: 'pt', 3: 's', 4: 'n'}[c['tier']]
            counts[tag] = counts.get(tag, 0) + 1
            suffix = '' if seen[name] == 1 else chr(ord('a') + seen[name] - 1)
            c.update(id='%s%s-%s%d' % (slug(name), suffix, tag, counts[tag]), name='%s - %s %d%s' % (short(name), side, counts[tag], suffix.upper()), island=name, side=side)
            out.append(c)
        print('%-22s shore %6.0f m, %3d candidates, %3d lots' % (name, L, len(cands), len(keep)))
    path = os.path.join(geodir, phase + '_plat.txt')
    with open(path, 'w') as f:
        f.write('# Keys Run Estates on the real islands (Tools/geo/plat.py). id|name|island|tier|side|cx|cz|area|frontage|polygon\n')
        for o in out:
            P = Polygon(o['poly'])
            c = P.centroid
            f.write('%s|%s|%s|%d|%s|%.1f|%.1f|%.0f|%.0f|%s\n' % (o['id'], o['name'], o['island'], o['tier'], o['side'], c.x, c.y, o['area'], o['front'],
                                                              ' '.join('%.1f %.1f' % (x, z) for x, z in o['poly'])))
    print('%d parcels -> %s' % (len(out), path))
    if preview:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots(figsize=(24, 9))
        for isl in g.islands:
            r = isl['ring']
            ax.fill(r[:, 0], r[:, 1], color='#d8cfa8', lw=0)
        col = {1: '#e46fa1', 2: '#b48cf0', 3: '#4fc3d9', 4: '#6fcf7f'}
        for o in out:
            xs = [p[0] for p in o['poly']]; zs = [p[1] for p in o['poly']]
            ax.fill(xs, zs, color=col[o['tier']], lw=.3, ec='k')
        ax.set_xlim(-6000, 7300); ax.set_ylim(1300, -1300); ax.set_aspect('equal')
        fig.savefig(preview, dpi=110, bbox_inches='tight')


if __name__ == '__main__':
    main()
