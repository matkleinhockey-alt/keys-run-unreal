"""canals.py: the residential canals of a phase, made wide enough for the 10 m grid and for a boat.

The real canals are 25 to 30 m wide, which the map's 1:3 compression turns into 8 to 10 game metres: less than a
grid cell, so they bake as a scatter of wet cells. bake.py calls cut() (PHASES 'canals') with the coastline polygons
and gets back, on its fine grid, the water to cut and the land to fill:

- narrow water (under NARROW across) within BUILT_NEAR of a residential street is a canal;
- its centre lines are followed longest first; a line is kept unless most of it runs within SPACING of one already
  kept (the comb of canals 38 m apart keeps every second tooth, so the land between still holds a row of houses);
- kept lines become water WIDTH across and DEPTH deep; teeth dropped for crowding are filled in as land;
- from each canal's mouth a cut DREDGE_W wide follows the water to the nearest water DEEP deep, if that is within
  DREDGE_MAX; dropped dead ends and short blind stubs are filled too.

Nothing here is random: the same inputs give the same canals."""
import heapq, json

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage, sparse
from scipy.sparse import csgraph
from scipy.spatial import cKDTree
from skimage.graph import MCP_Geometric
from skimage.morphology import skeletonize

RES = 2.5            # metres per cell of the raster the canals are found on
NARROW = 30.0        # water narrower than this is a canal
BUILT_NEAR = 70.0    # ... if a residential street is this near
SPACING = 55.0       # kept centre lines stay this far apart
MIN_FIRST, MIN_SIDE = 70.0, 60.0   # shortest canal kept: on its own, and as a branch (beyond SPACING from its parent)
CROWD = .6           # a line is dropped when more than this share of it is within SPACING of a kept one
WIDTH, DEPTH = 24.0, 2.0
BRIDGE = 60.0
DEEP, DREDGE_W, DREDGE_MAX, DREDGE_D = 2.2, 16.0, 700.0, 2.2   # (the game plays charted water under 2.4 m shallower than charted)
DEBUG = None        # a dict to receive the working rasters (preview_canals.py)


def _raster(polys, x0, z0, w, h, res):
    im = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(im)
    for p in polys:
        d.polygon([((x - x0) / res, (z - z0) / res) for x, z in p.exterior.coords], fill=1)
        for hole in p.interiors:
            d.polygon([((x - x0) / res, (z - z0) / res) for x, z in hole.coords], fill=0)
    return np.array(im, dtype=bool)


def _built(path, to_game, x0, z0, w, h, res):
    im = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(im)
    for e in json.load(open(path))['elements']:
        rings = []
        if e['type'] == 'way' and 'geometry' in e:
            rings = [e['geometry']]
        elif e['type'] == 'relation':
            rings = [m['geometry'] for m in e.get('members', []) if m.get('role') == 'outer' and 'geometry' in m]
        for r in rings:
            px = [((x - x0) / res, (z - z0) / res) for x, z in (to_game(p['lon'], p['lat']) for p in r)]
            if len(px) >= 2:
                d.line(px, fill=1, width=1)
    return np.array(im, dtype=bool)


def _graph(pts):
    """8-connected graph over skeleton pixels (N x 2 array of row, col); edge weights in cells"""
    index = {(int(a), int(b)): k for k, (a, b) in enumerate(pts)}
    I, J, W = [], [], []
    for k, (a, b) in enumerate(pts):
        for da, db, wt in ((0, 1, 1.0), (1, 0, 1.0), (1, 1, 1.4142), (1, -1, 1.4142)):
            q = index.get((int(a) + da, int(b) + db))
            if q is not None:
                I += [k, q]; J += [q, k]; W += [wt, wt]
    return sparse.csr_matrix((W, (I, J)), shape=(len(pts), len(pts)))


def _path(pred, end):
    out = [end]
    while pred[out[-1]] >= 0:
        out.append(pred[out[-1]])
    return out[::-1]


def cut(polys, built_json, to_game, x0, z0, fnx, fnz, fc, fine_h):
    """-> (water mask, fill mask, depth to cut to (positive metres, 0 = none), report dict), all on bake.py's fine grid
    (fnz x fnx cells of fc metres from x0, z0). fine_h is the fine grid of DEM heights (for the way out to deep water)."""
    k = int(round(fc / RES))
    assert abs(fc / RES - k) < 1e-9
    w, h = (fnx - 1) * k + 1, (fnz - 1) * k + 1
    land = _raster(polys, x0, z0, w, h, RES)
    water = ~land
    # narrow water: what a disc NARROW across cannot reach
    half = NARROW / 2
    core = ndimage.distance_transform_edt(water) * RES > half
    reach = ndimage.distance_transform_edt(~core) * RES <= half + RES
    near = ndimage.distance_transform_edt(~_built(built_json, to_game, x0, z0, w, h, RES)) * RES < BUILT_NEAR
    narrow = water & ~reach
    lab, cnt = ndimage.label(narrow, structure=np.ones((3, 3)))
    if cnt:
        # a stretch of narrow water is a canal when enough of it has a street near (the far end of a canal still counts)
        ix = np.arange(1, cnt + 1)
        sizes = ndimage.sum(narrow, lab, index=ix) * RES * RES
        share = ndimage.sum(near, lab, index=ix) * RES * RES / np.maximum(sizes, 1)
        narrow &= np.concatenate([[False], (sizes > 250) & (share >= .3)])[lab]
    skel = skeletonize(narrow)
    slab, scnt = ndimage.label(skel, structure=np.ones((3, 3)))

    # ---- follow the centre lines, longest first
    kept_pts = []                      # arrays of (row, col)
    tree = [None]

    def crowded(pp):
        if not kept_pts:
            return np.zeros(len(pp), bool)
        if tree[0] is None:
            tree[0] = cKDTree(np.concatenate(kept_pts))
        d, _ = tree[0].query(pp, distance_upper_bound=SPACING / RES)
        return np.isfinite(d)

    heap, serial = [], 0
    pieces = {}

    def push(pts, attach):
        """a piece of skeleton: its longest line (from the attachment, if it hangs off a kept line) goes on the heap"""
        nonlocal serial
        if len(pts) < 4:
            return
        G = _graph(pts)
        if attach is None:
            d0 = csgraph.dijkstra(G, indices=0)
            a = int(np.argmax(np.where(np.isfinite(d0), d0, -1)))
        else:
            a = int(np.argmin(((pts - attach) ** 2).sum(1)))
        d, pred = csgraph.dijkstra(G, indices=a, return_predecessors=True)
        b = int(np.argmax(np.where(np.isfinite(d), d, -1)))
        line = _path(pred, b)
        serial += 1
        pieces[serial] = (pts, G, line, d[line] * RES, attach is not None)
        heapq.heappush(heap, (-float(d[b]) * RES, serial))

    objs = ndimage.find_objects(slab)
    for c in range(1, scnt + 1):
        sl = objs[c - 1]
        rr, cc = np.nonzero(slab[sl] == c)
        push(np.stack([rr + sl[0].start, cc + sl[1].start], 1), None)

    kept_lines, dropped_pts = [], []
    n_short = n_crowd = 0
    while heap:
        neg, s = heapq.heappop(heap)
        pts, G, line, along, hangs = pieces.pop(s)
        pp = pts[line]
        free = along >= SPACING if hangs else np.ones(len(line), bool)     # (a branch starts beside its parent)
        length = -neg - (SPACING if hangs else 0)
        if length < (MIN_SIDE if hangs else MIN_FIRST):
            n_short += 1
            dropped_pts.append((pts, 0 if hangs else 1))
            continue
        if crowded(pp[free]).mean() > CROWD:
            n_crowd += 1
            dropped_pts.append((pts, 9))
            continue
        kept_pts.append(pp)
        tree[0] = None
        kept_lines.append(pp)
        # what is left of the piece hangs off this line
        rest = np.ones(len(pts), bool)
        rest[line] = False
        if rest.sum() >= 4:
            idx = np.nonzero(rest)[0]
            ncomp, cl = csgraph.connected_components(G[idx][:, idx], directed=False)
            line_tree = cKDTree(pp)
            for q in range(ncomp):
                sub = pts[idx[cl == q]]
                dd, _ = line_tree.query(sub)
                push(sub, sub[int(np.argmin(dd))])

    # the lines the game is given (<phase>_canals.txt): straightened to within 2 m, in game metres
    from shapely.geometry import LineString
    out_lines = []
    for pp in kept_lines:
        ls = LineString([(x0 + c * RES, z0 + r * RES) for r, c in pp]).simplify(2.0)
        out_lines.append([(float(x), float(z)) for x, z in ls.coords])
    canal_im = Image.new('L', (w, h), 0)
    cd = ImageDraw.Draw(canal_im)

    def draw_line(pts):
        cd.line([((x - x0) / RES, (z - z0) / RES) for x, z in pts], fill=1, width=1)
    # a canal drawn thinner than a cell breaks the centre line: an end that stops short of open water is joined to the
    # nearest other kept line within BRIDGE, when the straight way there is mostly water
    n_bridge = 0
    if len(kept_lines) > 1:
        owner = np.concatenate([np.full(len(pp), n) for n, pp in enumerate(kept_lines)])
        allp = np.concatenate(kept_lines)
        kt = cKDTree(allp)
        open_d = ndimage.distance_transform_edt(~(water & reach)) * RES
        for n, pp in enumerate(kept_lines):
            for e in (pp[0], pp[-1]):
                if open_d[e[0], e[1]] < WIDTH:
                    continue
                best = None
                for q in kt.query_ball_point(e, BRIDGE / RES):
                    if owner[q] != n:
                        d2 = ((allp[q] - e) ** 2).sum()
                        if best is None or d2 < best[0]:
                            best = (d2, allp[q])
                if best is None or best[0] < 9:
                    continue
                t = np.linspace(0, 1, int(np.sqrt(best[0]) * 2) + 2)
                lr = np.round(e[0] + (best[1][0] - e[0]) * t).astype(int)
                lc = np.round(e[1] + (best[1][1] - e[1]) * t).astype(int)
                if water[lr, lc].mean() >= .45:
                    out_lines.append([(float(x0 + e[1] * RES), float(z0 + e[0] * RES)), (float(x0 + best[1][1] * RES), float(z0 + best[1][0] * RES))])
                    n_bridge += 1
    for pts in out_lines:
        draw_line(pts)
    canal = np.array(canal_im, dtype=bool)
    canal_w = ndimage.distance_transform_edt(~canal) * RES <= WIDTH / 2 if kept_lines else canal
    fill = np.zeros((h, w), bool)
    if dropped_pts:
        # the water of a dropped line: narrow cells whose nearest centre-line pixel is one of its own. A tooth dropped for
        # crowding is always filled; a short one only when it is a dead end (it may touch open water in `allow` places)
        own = np.zeros((h, w), np.int32)
        own[skel] = -1
        for n, (pts, _) in enumerate(dropped_pts):
            own[pts[:, 0], pts[:, 1]] = n + 1
        _, (ir, ic) = ndimage.distance_transform_edt(own == 0, return_indices=True)
        cell = np.where(narrow, own[ir, ic], 0)
        open_c = water & ~narrow
        for n, sl in enumerate(ndimage.find_objects(np.maximum(cell, 0))):
            if sl is None:
                continue
            allow = dropped_pts[n][1]
            sl = (slice(max(0, sl[0].start - 2), sl[0].stop + 2), slice(max(0, sl[1].start - 2), sl[1].stop + 2))
            mine = cell[sl] == n + 1
            if allow < 9:
                touch = ndimage.binary_dilation(mine, structure=np.ones((3, 3)), iterations=2) & open_c[sl]
                if ndimage.label(touch, structure=np.ones((3, 3)))[1] > allow:
                    continue
            fill[sl] |= mine
        fill &= ~canal_w
    canal_f, fill_f = canal_w[::k, ::k].copy(), fill[::k, ::k].copy()
    assert canal_f.shape == (fnz, fnx)
    depth = np.where(canal_f, DEPTH, 0.0)

    # ---- the way out: from each canal mouth a cut that follows the water to the nearest deep water
    land_f = (land[::k, ::k] | fill_f) & ~canal_f
    deep = (~land_f) & (fine_h < -DEEP) & ~canal_f
    n_dredge = n_far = 0
    ways = []
    if kept_lines and deep.any():
        open_w = (~land_f) & ~canal_f
        mouth = canal_f & ndimage.binary_dilation(open_w, structure=np.ones((3, 3)))
        mlab, mcnt = ndimage.label(mouth, structure=np.ones((3, 3)))
        R = int(DREDGE_MAX / fc)
        for m, sl in enumerate(ndimage.find_objects(mlab)):
            rr, cc = np.nonzero(mlab[sl] == m + 1)
            a = (int(rr[len(rr) // 2]) + sl[0].start, int(cc[len(cc) // 2]) + sl[1].start)
            j0, j1, i0, i1 = max(0, a[0] - R), min(fnz, a[0] + R + 1), max(0, a[1] - R), min(fnx, a[1] + R + 1)
            lw = land_f[j0:j1, i0:i1]
            if deep[max(0, a[0] - 3):a[0] + 4, max(0, a[1] - 3):a[1] + 4].any():
                continue                                           # it opens on deep water already
            # keep off the banks: water within 10 m of land costs four times as much
            cost = np.where(lw, -1.0, np.where(ndimage.distance_transform_edt(~lw) * fc < 10, 4.0, 1.0))
            mcp = MCP_Geometric(cost)
            cum, _ = mcp.find_costs([(a[0] - j0, a[1] - i0)])
            dw = deep[j0:j1, i0:i1]
            cand = np.where(dw, cum, np.inf)
            b = np.unravel_index(int(np.argmin(cand)), cand.shape)
            if not np.isfinite(cand[b]):
                n_far += 1
                continue
            path = np.array(mcp.traceback(b))
            if len(path) * fc > DREDGE_MAX * 1.3:
                n_far += 1
                continue
            on = np.zeros(lw.shape, bool)
            on[path[:, 0], path[:, 1]] = True
            nl = ndimage.distance_transform_edt(~on) * fc <= DREDGE_W / 2
            depth[j0:j1, i0:i1] = np.where(nl, np.maximum(depth[j0:j1, i0:i1], DREDGE_D), depth[j0:j1, i0:i1])
            n_dredge += 1
            wl = LineString([(x0 + (c + i0) * fc, z0 + (r + j0) * fc) for r, c in path]).simplify(3.0) if len(path) > 1 else None
            if wl is not None:
                ways.append([(float(x), float(z)) for x, z in wl.coords])
    if DEBUG is not None:
        DEBUG.update(res=RES, land=land, narrow=narrow, skel=skel, canal=canal, fill=fill)
    report = dict(lines=len(kept_lines), metres=float(sum(len(p) for p in kept_lines) * RES * 1.1), short=n_short, crowded=n_crowd,
                  cut_cells=int((canal_f & land[::k, ::k]).sum()), fill_cells=int(fill_f.sum()), dredged=n_dredge, no_way_out=n_far, bridged=n_bridge)
    report['near'] = near[::k, ::k].copy()
    report['out_lines'] = out_lines
    report['ways'] = ways
    # ---- the canals a boat still cannot leave in the game, opened (canalways.py)
    import canalways
    canalways.open_shut(report, polys, land, fill, built_json, to_game, x0, z0, RES, WIDTH, DREDGE_W)
    for note in report.get('opened', []):
        print('  ' + note)
    return canal_f, fill_f, depth, report
