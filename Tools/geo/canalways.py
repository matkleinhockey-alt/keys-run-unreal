"""canalways.py: a way out to sea for every residential canal (called at the end of canals.cut()).

canals.cut() dredges from each canal mouth to charted deep water, working on the bake's own rasters. That leaves
canals a boat still cannot leave in the game: the 10 m grid closed the canal's own mouth (a pond on the grid), or
pinched it so the shore rule shoals it, or the flat outside plays shallower than charted, or the nearest deep water is
a pocket that is itself cut off. This step finds them the way the game's own test does (`fish:canals`,
KeysSea::LogCanalWays: a flood through the water the hull reads, see canalways_model.py) and opens each one:

- the route follows the real water (the coastline at 2.5 m, which still has the canal's real mouth) from the canal to
  the nearest water that is joined to the open sea, along a marked channel where one is near;
- where the route lies on the grid's land it becomes a canal line (WIDTH across, seawalled by the game): the real
  mouth, cut back open. Real land is only ever crossed for a few metres (LAND_MAX), never by a street, never through
  mangrove, never across US-1; a canal with no such route is left shut and reported as not a canal;
- where it lies on the grid's water it becomes a way ("w" line, DREDGE_W across), straightened wherever a straight
  line stays in clear water.

It reads the map as baked (the six .kgeo files and their marks in GEO), so a phase must have been baked once before;
the lines it adds change no .kgeo (the game cuts canals and ways from the lines), so baking again gives the same
canals file. Nothing here is random."""
import os

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage
from skimage.graph import MCP_Geometric

import canalways_model as M

GEO = os.environ.get('CANALWAYS_GEO') or os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Content', 'KeysRun', 'Geo'))
MIN_D = 1.0          # the water a boat needs (the game's test)
SEA_D = 1.5          # a way ends in water this deep that is joined to the open sea
REACH = 1400.0       # how far from a canal the game's test looks
LAND_C, FILL_C, BANK_C, SHORE_C, NAV_C = 40.0, 6.0, 4.0, 2.0, .4   # cost of a 2.5 m step: real land, a filled-in canal, water within 10 m of land, within 25 m, a marked channel
LAND_MAX = 15.0      # metres of real land a route may cross in all
ROAD_OFF = 5.0       # land this near a street is never cut
US1_OFF = 14.0       # nor the roadbed of US-1
CUT_S = 3.0          # the route is a canal line where the grid's shore distance is under this, a way elsewhere
CUT_PAD = 8.0
DEBUG_DIR = os.environ.get('CANALWAYS_DEBUG')


def _streets(path, to_game, x0, z0, w, h, res):
    """the streets of the built file as a raster (ways tagged highway; where the file has no tags, the open ways)"""
    import json
    im = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(im)
    for e in json.load(open(path))['elements']:
        if e['type'] != 'way' or 'geometry' not in e:
            continue
        g = e['geometry']
        t = e.get('tags')
        if t is not None and 'highway' not in t:
            continue
        if t is None and len(g) > 2 and g[0] == g[-1]:
            continue
        px = [((x - x0) / res, (z - z0) / res) for x, z in (to_game(p['lon'], p['lat']) for p in g)]
        if len(px) >= 2:
            d.line(px, fill=1, width=1)
    return np.array(im, dtype=bool)


def _sample(A, W, X, Z):
    """a node grid of the world read bilinearly at game points"""
    fx = np.clip((X - W.x0) / W.cell, 0, W.nx - 1.001)
    fz = np.clip((Z - W.z0) / W.cell, 0, W.nz - 1.001)
    i = fx.astype(int); j = fz.astype(int); u = fx - i; v = fz - j
    return (A[j, i] * (1 - u) + A[j, i + 1] * u) * (1 - v) + (A[j + 1, i] * (1 - u) + A[j + 1, i + 1] * u) * v


def _simplify(pts, tol):
    from shapely.geometry import LineString
    if len(pts) < 3:
        return pts
    return [(float(x), float(z)) for x, z in LineString(pts).simplify(tol).coords]


def _length(pts):
    return float(sum(np.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:])))


def open_shut(rpt, polys, land, fill, built_json, to_game, x0, z0, res, width, way_w):
    """adds to rpt['out_lines'] and rpt['ways'] what opens every canal of this phase the game's test finds shut;
    rpt['opened'] gets a line of text for each. land, fill: the bake's rasters at res metres from (x0, z0)."""
    notes = rpt.setdefault('opened', [])
    if not os.path.exists(f'{GEO}/p1.kgeo'):
        notes.append('ways out not checked: no baked map in ' + GEO)
        return
    W = M.World(GEO)
    D0 = W.base_depth()
    h, w = land.shape
    canals = [(width, pts) for pts in rpt['out_lines']]
    ways = [(way_w, pts) for pts in rpt['ways']]
    n_base, n_ways0 = len(canals), len(ways)
    Dn = W.with_lines(D0, canals, ways)
    street = _streets(built_json, to_game, x0, z0, w, h, res)
    # the shoreline itself, drawn as lines: an inlet narrower than a cell of the raster (most canal mouths are, at 1:3)
    # is lost from `land` but its two banks still trace it, so a route may follow the shoreline's own cells
    em = Image.new('L', (w, h), 0)
    ed = ImageDraw.Draw(em)
    for p in polys:
        for ring in [p.exterior] + list(p.interiors):
            ed.line([((x - x0) / res, (z - z0) / res) for x, z in ring.coords], fill=1, width=1)
    edge = np.array(em, dtype=bool)
    del em, ed
    c = W.cell

    def window(cx, cz, r):
        """node window (i0, i1, j0, j1) round a point"""
        i0 = max(0, int(np.floor((cx - r - W.x0) / c))); i1 = min(W.nx, int(np.ceil((cx + r - W.x0) / c)) + 1)
        j0 = max(0, int(np.floor((cz - r - W.z0) / c))); j1 = min(W.nz, int(np.ceil((cz + r - W.z0) / c)) + 1)
        return i0, i1, j0, j1

    def flood(k):
        """the game's test for canal k -> (open?, window, labels on the window's 5 m grid, open-sea labels, own labels)"""
        pts = canals[k][1]
        sx, sz = pts[len(pts) // 2]
        i0, i1, j0, j1 = sub = window(sx, sz, REACH)
        D5 = M.fine(Dn[j0:j1, i0:i1].astype(np.float32))
        S5 = M.fine(W.S[j0:j1, i0:i1])
        C5 = M.fine(W.seg_dist(W.line_segs(canals), 70.0, sub))
        X5 = W.x0 + i0 * c + np.arange(D5.shape[1]) * (c / 2)
        Z5 = W.z0 + j0 * c + np.arange(D5.shape[0]) * (c / 2)
        within = (X5[None, :] - sx) ** 2 + (Z5[:, None] - sz) ** 2 <= REACH * REACH
        lab, n = ndimage.label((D5 >= MIN_D) & within)
        sea = np.zeros(n + 1, bool)
        sea[np.unique(lab[(D5 >= SEA_D) & (S5 >= 60) & (C5 > 60) & within])] = True
        sea[0] = False
        a, b = int(round((sx - X5[0]) / (c / 2))), int(round((sz - Z5[0]) / (c / 2)))
        own = set()
        for da, db in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
            if 0 <= b + db < lab.shape[0] and 0 <= a + da < lab.shape[1] and lab[b + db, a + da]:
                own.add(int(lab[b + db, a + da]))
        return any(sea[q] for q in own), sub, lab, sea, own, (X5[0], Z5[0]), D5

    def plan(k, sub, lab, sea, own, org5, D5):
        """the route for canal k -> (path [(x, z)], metres of real land crossed) or (None, why)"""
        i0, i1, j0, j1 = sub
        # the window on the bake's raster
        wx0, wz0 = W.x0 + i0 * c, W.z0 + j0 * c
        a0 = max(0, int(round((wx0 - x0) / res))); a1 = min(w, int(round((W.x0 + (i1 - 1) * c - x0) / res)) + 1)
        b0 = max(0, int(round((wz0 - z0) / res))); b1 = min(h, int(round((W.z0 + (j1 - 1) * c - z0) / res)) + 1)
        if a1 - a0 < 8 or b1 - b0 < 8:
            return None, 'off the raster'
        X, Z = np.meshgrid(x0 + np.arange(a0, a1) * res, z0 + np.arange(b0, b1) * res)
        ld = land[b0:b1, a0:a1] & ~edge[b0:b1, a0:a1]          # (real land, less the shoreline's own cells)
        fl = fill[b0:b1, a0:a1]
        eg = edge[b0:b1, a0:a1] & land[b0:b1, a0:a1]
        gS = _sample(W.S, W, X, Z)
        # the 5 m flood read at each raster cell
        fi = np.clip(np.round((X - org5[0]) / (c / 2)).astype(int), 0, lab.shape[1] - 1)
        fj = np.clip(np.round((Z - org5[1]) / (c / 2)).astype(int), 0, lab.shape[0] - 1)
        L = lab[fj, fi]
        target = sea[L] & (D5[fj, fi] >= SEA_D) & ~ld
        if not target.any():
            return None, 'no open sea within %.0f m' % REACH
        ol = off_land[b0:b1, a0:a1]
        cost = np.where(ol < 10, BANK_C, np.where(ol < 25, SHORE_C, 1.0))
        cost = np.where(_sample(W.navdist, W, X, Z) <= M.NAV_HALF_W, cost * NAV_C, cost)
        cost = np.where((fl & ~ld) | eg, FILL_C, cost)
        # real land: dear, and never by a street, in mangrove or on US-1's roadbed
        mi = np.clip(np.round((X - W.x0) / c).astype(int), 0, W.nx - 1); mj = np.clip(np.round((Z - W.z0) / c).astype(int), 0, W.nz - 1)
        by_road = ndimage.distance_transform_edt(~street[b0:b1, a0:a1]) * res < ROAD_OFF
        no = ld & (by_road | (W.mang[mj, mi] >= 200))
        cost = np.where(ld, LAND_C, cost)
        cost = np.where(no, -1.0, cost)
        us1 = (np.abs(Z - W.chain_z(X)) <= US1_OFF) & (gS < 6)
        cost = np.where(us1, -1.0, cost)
        # from: the centre lines of the canals this one is joined to already
        src = Image.new('L', (a1 - a0, b1 - b0), 0)
        sd = ImageDraw.Draw(src)
        for q, (_, pts) in enumerate(canals):
            mx, mz = pts[len(pts) // 2]
            fa, fb = int(round((mx - org5[0]) / (c / 2))), int(round((mz - org5[1]) / (c / 2)))
            if q == k or (0 <= fb < lab.shape[0] and 0 <= fa < lab.shape[1] and lab[fb, fa] in own):
                sd.line([((x - x0) / res - a0, (z - z0) / res - b0) for x, z in pts], fill=1, width=1)
        starts = np.argwhere(np.array(src, dtype=bool) & (cost > 0))
        if not len(starts):
            return None, 'centre line off the raster'
        mcp = MCP_Geometric(cost, fully_connected=True)
        cum, _ = mcp.find_costs([tuple(s) for s in starts])
        cand = np.where(target, cum, np.inf)
        e = np.unravel_index(int(np.argmin(cand)), cand.shape)
        if not np.isfinite(cand[e]):
            return None, 'no route that keeps off streets, mangrove and US-1'
        path = np.array(mcp.traceback(e))
        on_land = ld[path[:, 0], path[:, 1]]
        land_m = float(on_land.sum() * res)
        return [(float(x0 + (a0 + q) * res), float(z0 + (b0 + p) * res)) for p, q in path], land_m

    off_land = ndimage.distance_transform_edt(~land).astype(np.float32) * res      # metres from real land

    def measure(X, Z):
        """along points: the least distance from real land and from the grid's shore"""
        ii = np.clip(np.round((X - x0) / res).astype(int), 0, w - 1); jj = np.clip(np.round((Z - z0) / res).astype(int), 0, h - 1)
        return float(off_land[jj, ii].min()), float(_sample(W.S, W, X, Z).min())

    def straighten(pts):
        """the way with its corners cut wherever the straight line keeps as far off the land as the route did
        (or 25 m off, if the route kept further)"""
        P = np.array(pts)
        out = [pts[0]]
        i = 0
        while i < len(pts) - 1:
            j = len(pts) - 1
            while j > i + 1:
                l0, s0 = measure(P[i:j + 1, 0], P[i:j + 1, 1])
                t = np.linspace(0, 1, int(np.hypot(*(P[j] - P[i])) / 2.5) + 2)
                l1, s1 = measure(P[i, 0] + (P[j, 0] - P[i, 0]) * t, P[i, 1] + (P[j, 1] - P[i, 1]) * t)
                if l1 >= min(l0, 25.0) - .5 and s1 >= min(s0, 25.0) - .5:
                    break
                j -= max(1, (j - i) // 6)
            out.append(pts[j])
            i = j
        return out

    def lines_of(path):
        """the route as canal lines (on the grid's land) and ways (on its water), each overlapping the next"""
        P = np.array(path)
        s = _sample(W.S, W, P[:, 0], P[:, 1])
        d = np.concatenate([[0], np.cumsum(np.hypot(np.diff(P[:, 0]), np.diff(P[:, 1])))])
        cutm = s < CUT_S
        # (a cut reaches CUT_PAD past the land at both ends, so its free end lies in the grid's water: a mouth)
        if cutm.any():
            dc = d[cutm]
            cutm = np.array([np.abs(dc - q).min() <= CUT_PAD for q in d]) | cutm
        new_c, new_w = [], []
        i = 0
        while i < len(P):
            j = i
            while j + 1 < len(P) and cutm[j + 1] == cutm[i]:
                j += 1
            a, b = max(0, i - 1), min(len(P) - 1, j + 1)        # (overlap its neighbours by a step)
            run = [tuple(q) for q in P[a:b + 1]]
            if len(run) >= 2 and _length(run) >= 5:
                if cutm[i]:
                    new_c.append(_simplify(run, 4.0))
                else:
                    new_w.append(straighten(_simplify(run, 1.0)))
            i = j + 1
        return new_c, new_w

    n_open = n_left = 0
    shut0 = [k for k in range(n_base) if not flood(k)[0]]
    done = set()
    for k in shut0:
        if k in done:
            continue
        is_open, sub, lab, sea, own, org5, D5 = flood(k)
        pts = canals[k][1]
        mid = pts[len(pts) // 2]
        path, land_m = plan(k, sub, lab, sea, own, org5, D5)
        if path is None:
            notes.append('canal %d (middle %.0f, %.0f) left shut: %s' % (k, mid[0], mid[1], land_m))
            if DEBUG_DIR:
                _preview(f'{DEBUG_DIR}/c{k:03d}_{mid[0]:.0f}_{mid[1]:.0f}_none.png', W, Dn, land, fill, street, x0, z0, res, sub, canals, ways, n_base, n_ways0, pts, edge)
            n_left += 1
            continue
        if land_m > LAND_MAX:
            if DEBUG_DIR:
                _preview(f'{DEBUG_DIR}/c{k:03d}_{mid[0]:.0f}_{mid[1]:.0f}_shut.png', W, Dn, land, fill, street, x0, z0, res, sub, canals, ways, n_base, n_ways0, path, edge)
            notes.append('canal %d (middle %.0f, %.0f) left shut: its nearest way to the sea crosses %.0f m of real land (not a canal with a mouth)' % (k, mid[0], mid[1], land_m))
            n_left += 1
            continue
        # the part of the route still inside the water the canal reaches is not dredged again
        P = np.array(path)
        fi = np.clip(np.round((P[:, 0] - org5[0]) / (c / 2)).astype(int), 0, lab.shape[1] - 1)
        fj = np.clip(np.round((P[:, 1] - org5[1]) / (c / 2)).astype(int), 0, lab.shape[0] - 1)
        inside = np.isin(lab[fj, fi], list(own)) if own else np.zeros(len(path), bool)
        last_in = int(np.nonzero(inside)[0].max()) if inside.any() else 0
        first = max(0, last_in - 4)                             # (10 m of overlap with the water it has)
        # why it was shut, and how far the nearest metre of water joined to the sea was (along the route)
        at_sea = np.nonzero(sea[lab[fj, fi]] & (np.arange(len(path)) > last_in))[0]
        to_sea = int(at_sea.min()) if len(at_sea) else len(path) - 1
        gap = P[last_in:to_sea + 1]
        gS = _sample(W.S, W, gap[:, 0], gap[:, 1])
        why = ('a pond on the grid (its mouth is land on the 10 m grid)' if (gS < 0).any()
               else 'a pinched mouth (so narrow on the grid that the shore rule shoals it)' if (gS < 12.5).mean() > .5
               else 'the flat outside is too shallow')
        new_c, new_w = lines_of(path[first:])
        canals += [(width, p) for p in new_c]
        ways += [(way_w, p) for p in new_w]
        rsub = window(mid[0], mid[1], REACH + 100)
        Dn[rsub[2]:rsub[3], rsub[0]:rsub[1]] = W.with_lines(D0[rsub[2]:rsub[3], rsub[0]:rsub[1]], canals, ways, rsub)
        ok = flood(k)[0]
        end = path[-1]
        notes.append('canal %d (middle %.0f, %.0f) was shut: %s; the nearest %.1f m water joined to the sea was %.0f m off. Opened%s: %.0f m to %.0f, %.0f; %.0f m of it over real land; %d canal line%s (%.0f m), %d way%s (%.0f m)'
                     % (k, mid[0], mid[1], why, MIN_D, _length([tuple(q) for q in gap]), '' if ok else ' BUT STILL SHUT IN THE MODEL', _length(path[first:]), end[0], end[1], land_m,
                        len(new_c), '' if len(new_c) == 1 else 's', sum(_length(p) for p in new_c), len(new_w), '' if len(new_w) == 1 else 's', sum(_length(p) for p in new_w)))
        n_open += 1
        done.add(k)
        for q in shut0:
            if q not in done and flood(q)[0]:
                qm = canals[q][1][len(canals[q][1]) // 2]
                notes.append('canal %d (middle %.0f, %.0f) was shut: joined to canal %d, and opened with it' % (q, qm[0], qm[1], k))
                done.add(q)
                n_open += 1
        if DEBUG_DIR:
            _preview(f'{DEBUG_DIR}/c{k:03d}_{mid[0]:.0f}_{mid[1]:.0f}.png', W, Dn, land, fill, street, x0, z0, res, sub, canals, ways, n_base, n_ways0, path, edge)
    rpt['out_lines'] = [p for _, p in canals]
    rpt['ways'] = [p for _, p in ways]
    rpt['opened_n'], rpt['left_shut'] = n_open, n_left
    notes.append('ways out: %d canals had no %.1f m of water to the open sea; %d opened (%d canal lines and %d ways added), %d left shut'
                 % (n_open + n_left, MIN_D, n_open, len(canals) - n_base, len(ways) - n_ways0, n_left))


def _preview(path, W, Dn, land, fill, street, x0, z0, res, sub, canals, ways, n_base, n_ways, route, edge=None):
    """a picture of one opened canal: real land, the grid's shore, the depth the hull reads, old and new lines"""
    xs = [p[0] for p in route]; zs = [p[1] for p in route]
    ax0, ax1, az0, az1 = min(xs) - 110, max(xs) + 110, min(zs) - 110, max(zs) + 110
    sc = 2.0                                                    # pixels a metre
    wd, ht = int((ax1 - ax0) * sc), int((az1 - az0) * sc)
    X, Z = np.meshgrid(ax0 + np.arange(wd) / sc, az0 + np.arange(ht) / sc)
    D = _sample(Dn, W, X, Z)
    S = _sample(W.S, W, X, Z)
    ii = np.clip(np.round((X - x0) / res).astype(int), 0, land.shape[1] - 1); jj = np.clip(np.round((Z - z0) / res).astype(int), 0, land.shape[0] - 1)
    rgb = np.zeros((ht, wd, 3), np.uint8)
    t = np.clip(D / 3.0, 0, 1)[..., None]
    rgb[:] = (np.array([200, 225, 235]) * (1 - t) + np.array([20, 70, 150]) * t).astype(np.uint8)
    rgb[D < 1.0] = (235, 215, 160)                              # water under a metre: sand colour
    rgb[S < 0] = (150, 170, 120)                                # the grid's land
    ld = land[jj, ii]
    rgb[ld] = (rgb[ld] * .6).astype(np.uint8)                   # real land: darker
    rgb[~ld & (S < 0)] = (0, 230, 230)                          # real water the grid has as land
    rgb[fill[jj, ii] & ~ld] = (190, 150, 190)
    rgb[street[jj, ii]] = (60, 60, 60)
    if edge is not None:
        e = edge[jj, ii] & ld & (S < -2)
        rgb[e] = (0, 150, 200)                                  # the shoreline's own cells inside the grid's land
    mi = np.clip(np.round((X - W.x0) / W.cell).astype(int), 0, W.nx - 1); mj = np.clip(np.round((Z - W.z0) / W.cell).astype(int), 0, W.nz - 1)
    mg = W.mang[mj, mi] >= 200
    rgb[mg] = (rgb[mg] * .5 + np.array([0, 90, 0]) * .5).astype(np.uint8)
    im = Image.fromarray(rgb)
    d = ImageDraw.Draw(im)
    px = lambda pts: [((x - ax0) * sc, (z - az0) * sc) for x, z in pts]
    for n, (_, pts) in enumerate(canals):
        d.line(px(pts), fill=(255, 255, 255) if n < n_base else (255, 0, 0), width=2)
    for n, (_, pts) in enumerate(ways):
        d.line(px(pts), fill=(255, 255, 0) if n < n_ways else (255, 120, 0), width=2)
    d.line(px(route), fill=(0, 0, 0), width=1)
    for a, b, cc, dd in W.nav:
        d.line(px([(a, b), (cc, dd)]), fill=(255, 0, 255), width=1)
    im.save(path)
