"""canalways.py: a way out to sea for every residential canal (called at the end of canals.cut()).

canals.cut() dredges from each canal mouth to charted deep water, working on the bake's own rasters. That leaves
canals a boat still cannot leave in the game: the 10 m grid closed the canal's own mouth (a pond on the grid), or
pinched it so the shore rule shoals it, or the flat outside plays shallower than charted, or the nearest deep water is
a pocket that is itself cut off. This step finds them the way the game's own test does (`fish:canals`,
KeysSea::LogCanalWays: a flood through the water the hull reads, see canalways_model.py) and opens each one:

- the route follows the real water (the coastline at 2.5 m, which still has the canal's real mouth) from the canal to
  the nearest water that is joined to the open sea, along a marked channel where one is near;
- where the route lies on the grid's land it becomes a canal line (WIDTH across, seawalled by the game): the real
  mouth, cut back open. Land that is land in life is never cut: the route keeps to real water and, where an inlet is
  narrower than the raster, to the shoreline's own cells, and such a stretch is checked against the coastline drawn
  at 0.5 m (water must join its two ends); US-1's roadbed is never crossed. A canal with no such route has no
  opening in life and is left shut, and reported;
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
FILL_C, BANK_C, SHORE_C, NAV_C = 6.0, 4.0, 2.0, .4   # cost of a 2.5 m step: a filled-in canal or the shoreline's own cells, water within 10 m of land, within 25 m, a marked channel
FINE = .5            # metres a cell when the coastline is drawn again to see whether a narrow inlet is water in life
NEAR_RUN = 6.0       # ... and how far from the route that water may lie
US1_OFF = 14.0       # nor the roadbed of US-1
CUT_S = 0.0          # the route is a canal line (seawalled) where the grid has land, a way elsewhere
CUT_MIN = 10.0       # ... if there is this much of it
STUB = 30.0          # ... or leaves a canal within this of its dead end
MERGE = 14.0         # a new canal line that begins or ends this near the end of a centre line is made part of that line
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
        # real land is never cut; nor a stretch of shoreline cells found to be a neck of land, not an inlet (below)
        cost = np.where(ld | blocked[b0:b1, a0:a1], -1.0, cost)
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
            return None, 'it has no opening in the real coastline (no water leads from it to the sea)'
        path = np.array(mcp.traceback(e))
        # where the route runs on the shoreline's own cells, look closer: is there water there in life?
        pl = land[b0:b1, a0:a1][path[:, 0], path[:, 1]]
        i = 0
        while i < len(path):
            if not pl[i]:
                i += 1
                continue
            j = i
            while j + 1 < len(path) and pl[j + 1]:
                j += 1
            if not water_in_life(path[max(0, i - 1):j + 2] + np.array([b0, a0])):
                blocked[path[i:j + 1, 0] + b0, path[i:j + 1, 1] + a0] = True
                return 'again', 0.0
            i = j + 1
        return [(float(x0 + (a0 + q) * res), float(z0 + (b0 + p) * res)) for p, q in path], 0.0

    blocked = np.zeros((h, w), bool)
    widths = []
    pb = np.array([p.bounds for p in polys]) if len(polys) else np.zeros((0, 4))

    def water_in_life(cells):
        """cells: a run of raster cells (row, col), water at both ends and the shoreline's own cells between. True if the
        real coastline, drawn at FINE metres, has water joining the two ends within a few metres of the run: an inlet
        narrower than the raster. False for a neck of land the two shores merely come close across."""
        X = x0 + cells[:, 1] * res; Z = z0 + cells[:, 0] * res
        m = 10.0
        fx0, fz0, fx1, fz1 = X.min() - m, Z.min() - m, X.max() + m, Z.max() + m
        fw, fh = int((fx1 - fx0) / FINE) + 1, int((fz1 - fz0) / FINE) + 1
        im = Image.new('L', (fw, fh), 0)
        d = ImageDraw.Draw(im)
        for n in np.nonzero((pb[:, 0] <= fx1) & (pb[:, 2] >= fx0) & (pb[:, 1] <= fz1) & (pb[:, 3] >= fz0))[0]:
            p = polys[n]
            d.polygon([((x - fx0) / FINE, (z - fz0) / FINE) for x, z in p.exterior.coords], fill=1)
            for hole in p.interiors:
                d.polygon([((x - fx0) / FINE, (z - fz0) / FINE) for x, z in hole.coords], fill=0)
        wat = ndimage.binary_dilation(~np.array(im, dtype=bool), structure=np.ones((3, 3)))
        cor = Image.new('L', (fw, fh), 0)
        ImageDraw.Draw(cor).line([((x - fx0) / FINE, (z - fz0) / FINE) for x, z in zip(X, Z)], fill=1, width=int(2 * NEAR_RUN / FINE))
        lab_f, _ = ndimage.label(wat & np.array(cor, dtype=bool), structure=np.ones((3, 3)))
        r = int(res / FINE)

        def at_end(q):
            a, b = int((X[q] - fx0) / FINE), int((Z[q] - fz0) / FINE)
            return set(np.unique(lab_f[max(0, b - r):b + r + 1, max(0, a - r):a + r + 1])) - {0}
        ok = bool(at_end(0) & at_end(len(cells) - 1))
        # how wide the water is along the run, in life: twice the distance from the nearest bank, at its widest within 3 m
        ed = ndimage.distance_transform_edt(~np.array(im, dtype=bool)) * FINE
        mx = ndimage.maximum_filter(ed, size=int(6 / FINE) + 1)
        wid = [2 * float(mx[min(fh - 1, int((z - fz0) / FINE)), min(fw - 1, int((x - fx0) / FINE))]) for x, z in zip(X, Z)]
        widths.append((float(X[len(X) // 2]), float(Z[len(Z) // 2]), len(cells) * res, float(np.median(wid)), float(np.min(wid)), ok))
        return ok

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
        # (a sliver of the grid's land under CUT_MIN long is not walled: the way's own dredging takes it)
        i = 0
        while i < len(P):
            j = i
            while j + 1 < len(P) and cutm[j + 1] == cutm[i]:
                j += 1
            if cutm[i] and d[j] - d[i] < CUT_MIN:
                cutm[i:j + 1] = False
            i = j + 1
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

    def seg_d(p, a, b):
        vx, vz = b[0] - a[0], b[1] - a[1]
        l2 = vx * vx + vz * vz
        t = min(1.0, max(0.0, ((p[0] - a[0]) * vx + (p[1] - a[1]) * vz) / l2)) if l2 > 1e-9 else 0.0
        return float(np.hypot(p[0] - a[0] - vx * t, p[1] - a[1] - vz * t))

    def line_d(p, pts):
        return min(seg_d(p, a, b) for a, b in zip(pts, pts[1:]))

    def describe(k, path, new_c, new_w, own, lab, org5):
        """a line of text for each line added for canal k: where the route leads and what in the real data it follows"""
        out = []
        e = path[-1]
        # where it leads
        into = [q for q, (cw, cp) in enumerate(canals) if q != k and line_d(e, cp) < cw / 2 + 6]
        inway = [q for q, (ww, wp) in enumerate(ways) if line_d(e, wp) < ww / 2]
        nd = float(_sample(W.navdist, W, np.array([e[0]]), np.array([e[1]]))[0])
        dep = float(_sample(Dn, W, np.array([e[0]]), np.array([e[1]]))[0])
        sh = float(_sample(W.S, W, np.array([e[0]]), np.array([e[1]]))[0])
        if into:
            cp = canals[into[0]][1]
            leads = 'into canal %d (middle %.0f, %.0f), which is open: the two are joined in life and share its mouth' % (into[0], cp[len(cp) // 2][0], cp[len(cp) // 2][1])
        elif nd <= M.NAV_HALF_W:
            n = int(np.argmin([seg_d(e, (a, b), (cc, dd)) for a, b, cc, dd in W.nav]))
            leads = 'into the marked channel %s, %.1f m deep there' % (W.nav_names[n], dep)
        elif inway:
            leads = 'into a way already dredged (way %d), which runs on to the open sea' % inway[0]
        else:
            leads = 'to open water %.1f m deep, %.0f m off the shore, joined to the open sea by %.1f m or more' % (dep, sh, MIN_D)
        # the canal centre lines this canal already reaches: land near those is its own bank
        ownc = [cp for q, (cw, cp) in enumerate(canals) if q == k or lab[min(lab.shape[0] - 1, max(0, int(round((cp[len(cp) // 2][1] - org5[1]) / (c / 2))))),
                                                                         min(lab.shape[1] - 1, max(0, int(round((cp[len(cp) // 2][0] - org5[0]) / (c / 2)))))] in own] + list(new_c)
        for ln in new_c:
            out.append('  line (canal line, %.0f m, %.0f, %.0f to %.0f, %.0f): the real mouth - water in life in the coastline, which the 10 m grid has as land; leads %s'
                       % (_length(ln), ln[0][0], ln[0][1], ln[-1][0], ln[-1][1], leads))
        for ln in new_w:
            n = max(2, int(_length(ln) / 2.5))
            sa = np.concatenate([[0], np.cumsum([np.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(ln, ln[1:])])])
            sp = np.linspace(0, sa[-1], n)
            X = np.interp(sp, sa, [q[0] for q in ln]); Z = np.interp(sp, sa, [q[1] for q in ln])
            in_nav = float((_sample(W.navdist, W, X, Z) <= M.NAV_HALF_W).mean())
            tight = float((_sample(W.S, W, X, Z) < 12.5).mean())
            what = ('a charted channel: it runs along the marked channel for %.0f%% of its length' % (in_nav * 100) if in_nav > .5
                    else 'a grid pinch: water in life, so narrow on the 10 m grid that it plays too shallow' if tight > .5
                    else 'the shortest clear line across the flat (no channel is charted there)')
            ii = np.clip(np.round((X - x0) / res).astype(int), 0, w - 1); jj = np.clip(np.round((Z - z0) / res).astype(int), 0, h - 1)
            ol = off_land[jj, ii].astype(float)
            away = np.array([min(line_d((x, z), cp) for cp in ownc) > 40 for x, z in zip(X, Z)]) if ownc else np.ones(n, bool)
            near = ''
            if away.any() and ol[away].min() < 30:
                q = int(np.argmin(np.where(away, ol, 1e9)))
                near = '; PASSES %.0f m FROM LAND that is not its canal\'s bank, at %.0f, %.0f' % (ol[q], X[q], Z[q])
            out.append('  line (way, %.0f m, %.0f, %.0f to %.0f, %.0f): %s; leads %s%s'
                       % (_length(ln), ln[0][0], ln[0][1], ln[-1][0], ln[-1][1], what, leads, near))
        return out

    parent, way_owner, cline_owner, group = {}, {}, {}, {}
    n_open = n_left = 0
    shut0 = [k for k in range(n_base) if not flood(k)[0]]
    done = set()
    for k in shut0:
        if k in done:
            continue
        is_open, sub, lab, sea, own, org5, D5 = flood(k)
        pts = canals[k][1]
        mid = pts[len(pts) // 2]
        widths.clear()
        path, land_m = plan(k, sub, lab, sea, own, org5, D5)
        tries = 0
        while path == 'again' and tries < 40:
            widths.clear()
            path, land_m = plan(k, sub, lab, sea, own, org5, D5)
            tries += 1
        if path is None or path == 'again':
            notes.append('canal %d (middle %.0f, %.0f) left shut: %s' % (k, mid[0], mid[1], land_m if path is None else 'no water leads from it to the sea in the real coastline'))
            if DEBUG_DIR:
                _preview(f'{DEBUG_DIR}/c{k:03d}_{mid[0]:.0f}_{mid[1]:.0f}_none.png', W, Dn, land, fill, street, x0, z0, res, sub, canals, ways, n_base, n_ways0, pts, edge)
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
        described = describe(k, path, new_c, new_w, own, lab, org5)
        # whose new water it runs into, if anyone's: that way then serves this canal too
        for q in range(n_ways0, len(ways)):
            if line_d(path[-1], ways[q][1]) < ways[q][0] / 2 + 2:
                parent[k] = way_owner[q]
        for q in range(n_base, len(canals)):
            if line_d(path[-1], canals[q][1]) < canals[q][0] / 2 + 2:
                parent[k] = cline_owner[q]
        for q in range(len(new_c)):
            cline_owner[len(canals) + q] = k
        for q in range(len(new_w)):
            way_owner[len(ways) + q] = k
        group[k] = dict(serves=[k], c_m=sum(_length(p) for p in new_c), w_m=sum(_length(p) for p in new_w), n=len(new_c) + len(new_w))
        canals += [(width, p) for p in new_c]
        ways += [(way_w, p) for p in new_w]
        rsub = window(mid[0], mid[1], REACH + 100)
        Dn[rsub[2]:rsub[3], rsub[0]:rsub[1]] = W.with_lines(D0[rsub[2]:rsub[3], rsub[0]:rsub[1]], canals, ways, rsub)
        ok = flood(k)[0]
        end = path[-1]
        notes.append('canal %d (middle %.0f, %.0f) was shut: %s; the nearest %.1f m water joined to the sea was %.0f m off. Opened%s: %.0f m to %.0f, %.0f; %.0f m of it over real land; %d canal line%s (%.0f m), %d way%s (%.0f m)'
                     % (k, mid[0], mid[1], why, MIN_D, _length([tuple(q) for q in gap]), '' if ok else ' BUT STILL SHUT IN THE MODEL', _length(path[first:]), end[0], end[1], land_m,
                        len(new_c), '' if len(new_c) == 1 else 's', sum(_length(p) for p in new_c), len(new_w), '' if len(new_w) == 1 else 's', sum(_length(p) for p in new_w)))
        notes += described
        for wx, wz, wl, wmed, wmin, wok in widths:
            notes.append('  inlet at %.0f, %.0f: %.0f m of route on the shoreline\'s cells; water there in life is %.1f m wide (median), %.1f m at its narrowest%s' % (wx, wz, wl, wmed, wmin, '' if wok else '; NOT JOINED'))
        n_open += 1
        done.add(k)
        for q in shut0:
            if q not in done and flood(q)[0]:
                qm = canals[q][1][len(canals[q][1]) // 2]
                notes.append('canal %d (middle %.0f, %.0f) was shut: joined to canal %d, and opened with it' % (q, qm[0], qm[1], k))
                done.add(q)
                n_open += 1
                group[k]['serves'].append(q)
        if DEBUG_DIR:
            _preview(f'{DEBUG_DIR}/c{k:03d}_{mid[0]:.0f}_{mid[1]:.0f}.png', W, Dn, land, fill, street, x0, z0, res, sub, canals, ways, n_base, n_ways0, path, edge)
            import json
            json.dump(dict(canal=k, centre=pts, route=_simplify(path, 1.0), new_canal=new_c, new_way=new_w),
                      open(f'{DEBUG_DIR}/c{k:03d}_route.json', 'w'))
    # ---- how many canals each piece of new water serves (its own, those joined to it, and those whose way runs into it)
    def all_served(g):
        out = list(group[g]['serves'])
        for q in group:
            if parent.get(q) == g:
                out += all_served(q)
        return out
    for g in group:
        sv = all_served(g)
        notes.append('new water for canal %d: %d line%s, %.0f m of canal line and %.0f m of way; it serves %d canal%s (%s)%s'
                     % (g, group[g]['n'], '' if group[g]['n'] == 1 else 's', group[g]['c_m'], group[g]['w_m'], len(sv), '' if len(sv) == 1 else 's',
                        ', '.join(str(q) for q in sv), '' if g not in parent else '; it runs into the water made for canal %d' % parent[g]))
    notes.append('new dredged water: %.0f m of canal line (real mouths and links) and %.0f m of way, %.0f m in all, for %d canals'
                 % (sum(v['c_m'] for v in group.values()), sum(v['w_m'] for v in group.values()), sum(v['c_m'] + v['w_m'] for v in group.values()), n_open))
    # ---- no more centre lines than need be: a new canal line that starts or ends at the end of a canal's own centre line
    # is the same canal, so it is written as part of that line (and two canals it joins end to end become one line)
    lines = [list(p) for _, p in canals]
    alive = [True] * len(lines)
    n_merge = n_fuse = 0

    def end_near(pt, skip):
        best = None
        for q, ln in enumerate(lines):
            if not alive[q] or q in skip:
                continue
            for at_end in (0, -1):
                dd = float(np.hypot(ln[at_end][0] - pt[0], ln[at_end][1] - pt[1]))
                if dd <= MERGE and (best is None or dd < best[0]):
                    best = (dd, q, at_end)
        return best

    def cat(a, b):
        return a + (b[1:] if np.hypot(a[-1][0] - b[0][0], a[-1][1] - b[0][1]) < 1.0 else b)
    for n in range(n_base, len(lines)):
        N = lines[n]
        A = end_near(N[0], {n})
        B = end_near(N[-1], {n} | ({A[1]} if A else set()))
        if A:
            a = lines[A[1]] if A[2] == -1 else lines[A[1]][::-1]          # ... ends where N begins
            merged = cat(a, N)
            if B:
                b = lines[B[1]] if B[2] == 0 else lines[B[1]][::-1]       # begins where N ends
                merged = cat(merged, b)
                alive[B[1]] = False
                n_fuse += 1
            lines[A[1]] = merged
            alive[n] = False
            n_merge += 1
        elif B:
            b = lines[B[1]] if B[2] == 0 else lines[B[1]][::-1]
            lines[B[1]] = cat(N, b)
            alive[n] = False
            n_merge += 1
        else:
            # it leaves a canal a little short of that canal's dead end: the centre line runs to the dead end and back
            # to the turning, then on down the new stretch (the same water, one line)
            for q in range(n_base):
                if not alive[q]:
                    continue
                for ln in (lines[q], lines[q][::-1]):
                    j = len(ln) - 1
                    stub = 0.0
                    hit = None
                    while j > 0 and stub <= STUB:
                        dd = seg_d(N[0], ln[j - 1], ln[j])
                        if dd < 3.0:
                            hit = j
                            break
                        stub += float(np.hypot(ln[j][0] - ln[j - 1][0], ln[j][1] - ln[j - 1][1]))
                        j -= 1
                    if hit is None:
                        continue
                    e, pv = ln[-1], ln[-2]
                    r = float(np.hypot(e[0] - pv[0], e[1] - pv[1]))
                    bx, bz = e[0] + (e[0] - pv[0]) / r * (width / 2 + 10), e[1] + (e[1] - pv[1]) / r * (width / 2 + 10)
                    if float(_sample(W.S, W, np.array([bx]), np.array([bz]))[0]) >= 0:
                        continue                                # (that end is a mouth, not a dead end: left an end)
                    lines[q] = cat(ln + ln[hit:-1][::-1], N)
                    alive[n] = False
                    n_merge += 1
                    break
                if not alive[n]:
                    break
    n_lines_before = n_base
    canals = [(width, ln) for q, ln in enumerate(lines) if alive[q]]
    notes.append('centre lines: %d from the canals themselves, %d canal lines added for real mouths and links, %d of those made part of the line they continue (%d joining two lines into one): %d centre lines written'
                 % (n_lines_before, len(lines) - n_base, n_merge, n_fuse, len(canals)))
    rpt['out_lines'] = [p for _, p in canals]
    rpt['ways'] = [p for _, p in ways]
    rpt['opened_n'], rpt['left_shut'] = n_open, n_left
    notes.append('ways out: %d canals had no %.1f m of water to the open sea; %d opened (%d canal lines and %d ways added), %d left shut'
                 % (n_open + n_left, MIN_D, n_open, len(lines) - n_base, len(ways) - n_ways0, n_left))


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


def beacons(geo=None):
    """where a pair of daybeacons belongs: the outer end of every way that crosses a flat. The same rule as the game's
    (request R1, KeysGeoReal MarkWays): a way 60 m long or more that ends 30 m or more off the shore, not in another
    way, with no mark within 45 m already -> [(way index over the whole map, length, (green x, z), (red x, z))]"""
    W = M.World(geo or GEO)
    _, ways = M.load_all(geo or GEO)
    marks = [(x, z) for x, z, _, _, _ in W.marks]
    S = lambda x, z: float(_sample(W.S, W, np.array([x]), np.array([z]))[0])

    def seg_d(p, a, b):
        v = (b[0] - a[0], b[1] - a[1])
        l2 = v[0] * v[0] + v[1] * v[1]
        t = min(1.0, max(0.0, ((p[0] - a[0]) * v[0] + (p[1] - a[1]) * v[1]) / l2)) if l2 > 1e-9 else 0.0
        return float(np.hypot(p[0] - a[0] - v[0] * t, p[1] - a[1] - v[1] * t))
    out = []
    for n, (_, wd, pts) in enumerate(ways):
        e = pts[-1]
        ln = _length(pts)
        if ln < 60 or S(*e) < 30:
            continue
        d = None
        for q in pts[-2::-1]:
            r = np.hypot(e[0] - q[0], e[1] - q[1])
            if r > 4:
                d = ((e[0] - q[0]) / r, (e[1] - q[1]) / r)
                break
        if d is None:
            continue
        skip = False
        for o, (_, owd, op) in enumerate(ways):
            if o == n or np.hypot(op[-1][0] - e[0], op[-1][1] - e[1]) < 20:
                continue
            if any(seg_d(e, a, b) < owd / 2 for a, b in zip(op, op[1:])):
                skip = True
                break
        if skip or any((mx - e[0]) ** 2 + (mz - e[1]) ** 2 < 45.0 ** 2 for mx, mz in marks):
            continue
        sb = (d[1], -d[0])
        red = (e[0] + sb[0] * (wd / 2 + 2), e[1] + sb[1] * (wd / 2 + 2))
        grn = (e[0] - sb[0] * (wd / 2 + 2), e[1] - sb[1] * (wd / 2 + 2))
        if S(*red) < 10 or S(*grn) < 10:
            continue
        marks += [grn, red]
        out.append((n, ln, grn, red))
    return out


if __name__ == '__main__':
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == 'beacons':
        B = beacons(sys.argv[2] if len(sys.argv) > 2 else None)
        for n, ln, g, r in B:
            print('way %d (%.0f m): green %.1f, %.1f red %.1f, %.1f' % (n, ln, g[0], g[1], r[0], r[1]))
        print('%d pairs' % len(B))
    else:
        print('canalways.py beacons [geo dir]: the daybeacon pairs at the outer ends of the ways (the opening itself is run by canals.py)')
