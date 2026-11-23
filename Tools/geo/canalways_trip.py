"""canalways_trip.py: scripted boat trips out of opened canals and back in, and a reader for the log they leave.

The game's test scripts cannot steer by what the boat sees: they set a heading and a throttle at given seconds. So a
trip is laid out ahead of time along the lines the bake made, from a point inside the canal to open water, and the
heading is set again twice a second for where the boat should be by then (speeds measured in the game: throttle 0.05
is idle, 2.5 m/s; 0.165 is 15 kn).

  python canalways_trip.py script <route.json> [<route.json> ...]    -> the script for shoot.sh (each canal: out at
                                                                        idle, out at 15 kn, back in at idle)
  python canalways_trip.py read <game log> <route.json> [...]        -> each leg: seconds, aground or not, the least
                                                                        water under the boat, how far she strayed

route.json is what canals.py --preview leaves for each opened canal. Low tide is held for the whole run."""
import json, math, os, re, sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import canalways as CW
import canalways_model as M

IDLE = (0.05, 2.5, 2.6)      # throttle, metres a second once she is moving, seconds lost getting under way
FAST = (0.165, 8.0, 3.0)     # 15 kn
BACK = 45.0                  # the trip starts this far inside the canal
OUT = 90.0                   # and ends this far past the end of the way, if the water allows


def arc(pts):
    return np.concatenate([[0], np.cumsum([math.dist(a, b) for a, b in zip(pts, pts[1:])])])


def at(pts, s_arr, s):
    s = min(max(s, 0.0), float(s_arr[-1]))
    i = int(min(np.searchsorted(s_arr, s, side='right') - 1, len(pts) - 2))
    t = (s - s_arr[i]) / max(1e-9, s_arr[i + 1] - s_arr[i])
    return (pts[i][0] + (pts[i + 1][0] - pts[i][0]) * t, pts[i][1] + (pts[i + 1][1] - pts[i][1]) * t)


def nearest_s(pts, s_arr, p):
    best = (1e18, 0.0)
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        vx, vz = b[0] - a[0], b[1] - a[1]
        l2 = vx * vx + vz * vz
        t = min(1.0, max(0.0, ((p[0] - a[0]) * vx + (p[1] - a[1]) * vz) / l2)) if l2 > 1e-9 else 0.0
        d = math.hypot(p[0] - a[0] - vx * t, p[1] - a[1] - vz * t)
        if d < best[0]:
            best = (d, float(s_arr[i] + t * math.sqrt(l2)))
    return best


_world = {}


def depth_fn():
    """the model's depth at points, with every canal and way of the map dredged"""
    if not _world:
        W = M.World(CW.GEO)
        canals, ways = M.load_all(CW.GEO)
        _world['W'] = W
        _world['D'] = W.with_lines(W.base_depth(), [(w, p) for _, w, p in canals], [(w, p) for _, w, p in ways])
    W, D = _world['W'], _world['D']
    return lambda x, z: float(CW._sample(D, W, np.array([x]), np.array([z]))[0]), lambda x, z: float(CW._sample(W.S, W, np.array([x]), np.array([z]))[0])


def trip(route_json):
    """the line the boat is to follow: from inside the canal, along the lines the bake added, out into open water"""
    R = json.load(open(route_json))
    centre = [tuple(p) for p in R['centre']]
    route = [tuple(p) for p in R['route']]
    rs = arc(route)
    lines = sorted([[tuple(p) for p in ln] for ln in R['new_canal'] + R['new_way']], key=lambda ln: nearest_s(route, rs, ln[0])[1])
    depth, shore = depth_fn()
    # the route as far as the first new line, then the new lines end to end
    first_s = nearest_s(route, rs, lines[0][0])[1] if lines else float(rs[-1])
    chain = [p for p, s in zip(route, rs) if s < first_s]
    for ln in lines:
        # (each line overlaps the one before: take it up from the point nearest where the boat is by then)
        if not chain:
            chain += ln
            continue
        ls = arc(ln)
        s_on = nearest_s(ln, ls, chain[-1])[1]
        chain += [p for p, s in zip(ln, ls) if s > s_on + 2.5]
    # inside the canal: straight back from where the route starts, as far as the canal's own water goes that way (a
    # script cannot feel its way round a bend); failing that, back along the canal's centre line
    cs_ = arc(chain)
    a0, b0 = chain[0], at(chain, cs_, 12)
    d0 = ((b0[0] - a0[0]) / math.dist(a0, b0), (b0[1] - a0[1]) / math.dist(a0, b0))
    back = 0.0
    while back < BACK and depth(a0[0] - d0[0] * (back + 12), a0[1] - d0[1] * (back + 12)) >= 1.7:
        back += 5
    if back >= 20:
        pts = [(a0[0] - d0[0] * back, a0[1] - d0[1] * back)]
    else:
        cs = arc(centre)
        s0 = nearest_s(centre, cs, route[0])[1]
        sb = s0 - BACK if s0 - BACK >= 0 and (s0 >= cs[-1] - s0 or s0 + BACK > cs[-1]) else min(float(cs[-1]), s0 + BACK)
        n = max(2, int(abs(sb - s0) / 5))
        pts = [at(centre, cs, sb + (s0 - sb) * q / n) for q in range(n + 1)]
    pts += chain
    # on into open water, straight, as far as it is deep
    a, b = pts[-2], pts[-1]
    q = len(pts) - 2
    while math.dist(a, b) < 12 and q > 0:
        q -= 1
        a = pts[q]
    d = ((b[0] - a[0]) / math.dist(a, b), (b[1] - a[1]) / math.dist(a, b))
    ext = 0.0
    while ext < OUT and depth(b[0] + d[0] * (ext + 10), b[1] + d[1] * (ext + 10)) >= 1.3:
        ext += 5
    if ext >= 10:
        pts.append((b[0] + d[0] * ext, b[1] + d[1] * ext))
    from shapely.geometry import LineString
    pts = [(float(x), float(z)) for x, z in LineString(pts).simplify(2.5).coords]
    return dict(canal=R['canal'], pts=pts, ext=ext, end_depth=depth(*pts[-1]), end_shore=shore(*pts[-1]))


def heading(a, b):
    # the game's forward is (-sin H, -cos H)
    return math.atan2(-(b[0] - a[0]), -(b[1] - a[1]))


def leg(t, pts, thr, v, lag, tag):
    """script steps for one leg from pts[0] to pts[-1], starting at second t -> (steps, the second it ends)"""
    s_arr = arc(pts)
    total = float(s_arr[-1])
    h0 = heading(pts[0], at(pts, s_arr, 8))
    steps = [(t, 'tp=%.0f,%.0f,%.3f' % (pts[0][0], pts[0][1], h0)), (t + 2, 'throttle=%g' % thr)]
    dur = lag + total / v
    k = 0.0
    last = None
    while k <= dur:
        s = v * max(0.0, k - lag)
        a = at(pts, s_arr, s)
        b = at(pts, s_arr, s + 8)
        if math.dist(a, b) > .5:
            hd = heading(a, b)
            # (set again every 2 s on a straight: left alone the bow wanders; a long script is cut short by the game)
            if last is None or abs(hd - last[0]) > .01 or k - last[1] >= 2:
                steps.append((t + 2 + k, 'heading=%.3f' % hd))
                last = (hd, k)
        k += .5
    steps.append((t + 2 + dur, 'throttle=0'))
    return steps, t + 2 + dur + 6


def script(files):
    steps = [(3, 'play'), (5, 'tide=-1'), (6, 'fish:sound')]
    t = 8.0
    plan = []
    for f in files:
        T = trip(f)
        pts = T['pts']
        for name, p, (thr, v, lag) in (('out at idle', pts, IDLE), ('out at 15 kn', pts, FAST), ('back in at idle', pts[::-1], IDLE)):
            st, t1 = leg(t, p, thr, v, lag, name)
            steps += st
            if name == 'out at idle':
                steps.append((t + (t1 - t) * .5, 'shot'))
            plan.append((T['canal'], name, t, t1))
            t = t1
    steps.append((t + 1, 'quit'))
    steps.sort(key=lambda q: q[0])
    return ';'.join('%g:%s' % (round(a, 1), b) for a, b in steps), plan


def read(log, files):
    """the KRSound lines of a run against its plan"""
    _, plan = script(files)
    trips = {}
    for f in files:
        T = trip(f)
        trips[T['canal']] = T
    rows = []
    t_play = None
    pat = re.compile(r'\[(\d+)\.(\d+)\.(\d+)-(\d+)\.(\d+)\.(\d+):(\d+)\]\[ *\d+\]LogTemp: KRSound (-?\d+),(-?\d+) (.*?): ([\d.]+) m under her \(tide ([-+\d.]+)\), she draws ([\d.]+): (\w+), ([\d.]+) m/s')
    for ln in open(log, errors='ignore'):
        m = pat.search(ln)
        if m:
            g = m.groups()
            rows.append((int(g[3]) * 3600 + int(g[4]) * 60 + int(g[5]) + int(g[6]) / 1000, float(g[7]), float(g[8]), float(g[10]), g[13], float(g[14]), g[9]))
    if not rows:
        print('no KRSound lines')
        return 1
    t0 = rows[0][0] - 6.0          # (fish:sound is given at second 6)
    bad = 0
    for canal, name, a, b in plan:
        pts = trips[canal]['pts']
        s_arr = arc(pts)
        seg = [r for r in rows if a + 1 <= r[0] - t0 <= b]
        if not seg:
            print('canal %d %s: no lines' % (canal, name))
            bad += 1
            continue
        ag = sum(1 for r in seg if r[4] == 'aground')
        on = [nearest_s(pts, s_arr, (r[1], r[2])) for r in seg]
        stray = max([d for d, sv in on if 1 < sv < s_arr[-1] - 1] or [0.0])
        last = seg[-1]
        want = pts[-1] if name.startswith('out') else pts[0]
        vmax = max(r[5] for r in seg)
        print('canal %d %-16s %3.0f s  %s  least water %.2f m (tide %s held)  fastest %.1f m/s (%.1f kn)  strayed %.1f m at most  ended %.0f m from the end of the line, at %.0f, %.0f (%s)'
              % (canal, name + ':', b - a, 'AGROUND on %d lines' % ag if ag else 'no aground', min(r[3] for r in seg), 'low', vmax, vmax / .5144, stray,
                 math.dist((last[1], last[2]), want), last[1], last[2], last[6]))
        bad += 1 if ag else 0
    return 1 if bad else 0


if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[1] == 'script':
        s, plan = script(sys.argv[2:])
        print(s)
        for c, name, a, b in plan:
            print('# canal %d %s: seconds %.0f to %.0f' % (c, name, a, b), file=sys.stderr)
    elif len(sys.argv) > 3 and sys.argv[1] == 'read':
        sys.exit(read(sys.argv[2], sys.argv[3:]))
    elif len(sys.argv) > 2 and sys.argv[1] == 'trip':
        for f in sys.argv[2:]:
            T = trip(f)
            print(T['canal'], 'ext %.0f m, end depth %.2f m, shore %.0f m:' % (T['ext'], T['end_depth'], T['end_shore']), ' '.join('%.0f,%.0f' % p for p in T['pts']))
    else:
        print(__doc__)
