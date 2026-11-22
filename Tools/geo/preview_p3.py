"""preview_p3.py <geo dir> <out.png> [--route out.json]: the whole real map as the game will see it, every phase that is
baked side by side, and the checks Phase 3 has to pass on the data alone:

  - the seams: heights and shore distances along every edge two files share, the largest difference
  - Hawk Channel: a line down the ocean side from off Key West to the top of Key Largo (as far as the files go), the
    shoalest water on it, and its waypoints (the scripted run steers by them)
  - the marked channels of p3, p4 and p4_bay: the water at every mark and between consecutive marks of a channel

Reads only baked files (through kgeo.py). Prints one summary line per check."""
import json, os, sys

import numpy as np
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kgeo import KGeo

PHASES = ('p1', 'p2', 'p2_bay', 'p3', 'p4', 'p4_bay')     # the order the game loads them in
NEW = ('p3', 'p4', 'p4_bay')


def main():
    geo, out = sys.argv[1], sys.argv[2]
    route_out = sys.argv[sys.argv.index('--route') + 1] if '--route' in sys.argv else None
    G = {p: KGeo(os.path.join(geo, p + '.kgeo')) for p in PHASES if os.path.exists(os.path.join(geo, p + '.kgeo'))}
    cell = G['p1'].cell
    X1 = lambda g: g.x0 + (g.nx - 1) * cell
    Z1 = lambda g: g.z0 + (g.nz - 1) * cell
    x0 = min(g.x0 for g in G.values()); z0 = min(g.z0 for g in G.values())
    x1 = max(X1(g) for g in G.values()); z1 = max(Z1(g) for g in G.values())
    nx, nz = int(round((x1 - x0) / cell)) + 1, int(round((z1 - z0) / cell)) + 1
    H = np.full((nz, nx), np.nan, np.float32); S = np.full((nz, nx), np.nan, np.float32)
    for p in reversed(list(G)):     # p1 last: where two files share an edge the game reads the first loaded
        g = G[p]
        i, j = int(round((g.x0 - x0) / cell)), int(round((g.z0 - z0) / cell))
        H[j:j + g.nz, i:i + g.nx] = g.height; S[j:j + g.nz, i:i + g.nx] = g.shore
    print(f'map: {len(G)} grids ({", ".join(G)}), {nx} x {nz} cells, x {x0:.0f}..{x1:.0f}, z {z0:.0f}..{z1:.0f}; cells with no data {int(np.isnan(H).sum())}')

    # ---- seams: every pair of files that share an edge
    worst = 0.0
    names = list(G)
    for a in names:
        for b in names:
            if a == b: continue
            ga, gb = G[a], G[b]
            if abs(X1(ga) - gb.x0) < 1e-6:      # a's last column against b's first
                lo, hi = max(ga.z0, gb.z0), min(Z1(ga), Z1(gb))
                if hi - lo < cell: continue
                ja, jb, n = int(round((lo - ga.z0) / cell)), int(round((lo - gb.z0) / cell)), int(round((hi - lo) / cell)) + 1
                dh = np.abs(ga.height[ja:ja + n, -1] - gb.height[jb:jb + n, 0]); ds = np.abs(ga.shore[ja:ja + n, -1] - gb.shore[jb:jb + n, 0])
                # and the step from each side's edge to its own next cell, so a hidden cliff one cell in would show
                st = max(np.abs(np.diff(ga.height[ja:ja + n, -3:], axis=1)).max(), np.abs(np.diff(gb.height[jb:jb + n, :3], axis=1)).max())
                where = f'x = {gb.x0:.0f}, z {lo:.0f}..{hi:.0f}'
            elif abs(Z1(ga) - gb.z0) < 1e-6:    # a's last row against b's first
                lo, hi = max(ga.x0, gb.x0), min(X1(ga), X1(gb))
                if hi - lo < cell: continue
                ia, ib, n = int(round((lo - ga.x0) / cell)), int(round((lo - gb.x0) / cell)), int(round((hi - lo) / cell)) + 1
                dh = np.abs(ga.height[-1, ia:ia + n] - gb.height[0, ib:ib + n]); ds = np.abs(ga.shore[-1, ia:ia + n] - gb.shore[0, ib:ib + n])
                st = max(np.abs(np.diff(ga.height[-3:, ia:ia + n], axis=0)).max(), np.abs(np.diff(gb.height[:3, ib:ib + n], axis=0)).max())
                where = f'z = {gb.z0:.0f}, x {lo:.0f}..{hi:.0f}'
            else:
                continue
            if a in NEW or b in NEW: worst = max(worst, float(dh.max()))
            print(f'seam {a} | {b} ({where}): {n} cells, height differs by at most {dh.max():.3f} m, shore distance by at most {ds.max():.2f} m; '
                  f'steepest step within two cells of the edge {st:.2f} m')
    print(f'seams of the new phases: largest height difference {worst:.3f} m')

    def at(A, x, z):
        return A[np.clip(np.round((np.asarray(z) - z0) / cell).astype(int), 0, nz - 1), np.clip(np.round((np.asarray(x) - x0) / cell).astype(int), 0, nx - 1)]

    def chain(x):
        for g in G.values():
            if g.x0 - 1 <= x <= X1(g) + 1:
                return float(g.chain_z(x))
        return float(G['p1'].chain_z(x))

    # ---- Hawk Channel: the water between the keys and the reef. The line is the cheapest way along the map on a 100 m
    # by 20 m lattice south of US-1's line: water under 4 m costs (under 2.5 m a great deal), so does water within 300 m
    # of a shore, the open sea past the reef (over 25 m) a little, and every metre of sideways movement a little, at
    # most 60 m sideways per 100 m run. Then smoothed, and checked every 10 m.
    xs = np.arange(x0 + 900, x1 - 900, 100.0)
    zl = np.arange(z0 + 300, z1 - 300, 20.0)
    INF = 1e12
    best, back = None, []
    for x in xs:
        c = chain(x)
        d = -at(H, np.full_like(zl, x), zl); sh = at(S, np.full_like(zl, x), zl)
        d = np.nan_to_num(d, nan=-1.0); sh = np.nan_to_num(sh, nan=-1.0)
        w = np.where(d < 2.5, 2000.0, np.where(d < 4, 25.0 * (4 - d), 0.0)) + np.clip(300 - sh, 0, 300) / 30.0 + np.where(d > 25, 3.0, 0.0)
        w = np.where((zl < c + 300) | (sh <= 0), INF, w)
        if best is None:
            best = w
            continue
        cand = np.full((7, len(zl)), INF)
        for k, sft in enumerate(range(-3, 4)):
            src = np.roll(best, sft)
            if sft > 0: src[:sft] = INF
            if sft < 0: src[sft:] = INF
            cand[k] = src + abs(sft) * 20 * .02
        kbest = np.argmin(cand, axis=0)
        back.append(kbest - 3)
        best = cand[kbest, np.arange(len(zl))] + w
    j = int(np.argmin(best))
    idx = [j]
    for bk in reversed(back):
        j = j - int(bk[j])
        idx.append(j)
    zs = ndimage.gaussian_filter1d(zl[np.array(idx[::-1])], 4, mode='nearest')
    px = np.arange(xs[0], xs[-1], 10.0); pz = np.interp(px, xs, zs)
    pd, ps = -at(H, px, pz), at(S, px, pz)
    k = int(np.argmin(pd))
    way = [(float(px[i]), float(pz[i])) for i in range(0, len(px), 150)] + [(float(px[-1]), float(pz[-1]))]
    joins = sorted(set(g.x0 for p, g in G.items() if p != 'p1' and x0 < g.x0 < x1) | set(X1(g) for p, g in G.items() if x0 < X1(g) < x1))
    print(f'Hawk Channel line x {px[0]:.0f}..{px[-1]:.0f} ({(px[-1] - px[0]) / 1000:.1f} km): shoalest {pd.min():.1f} m at x {px[k]:.0f}, z {pz[k]:.0f}; '
          f'nearest shore {ps.min():.0f} m; under 3 m for {int((pd < 3).sum()) * 10} m; at the joins: '
          + ', '.join(f'x {j:.0f} {float(-at(H, j, np.interp(j, xs, zs))):.1f} m' for j in joins))
    if route_out:
        json.dump(dict(hawk=[[round(a, 1), round(b, 1)] for a, b in way]), open(route_out, 'w'))

    # ---- marks and the channels they mark (the new phases)
    marks = []
    for ph in NEW:
        mp = os.path.join(geo, ph + '_marks.txt')
        if not os.path.exists(mp):
            continue
        mine = []
        for ln in open(mp):
            if ln.startswith('#'): continue
            v = ln.split(None, 5)
            mine.append((float(v[0]), float(v[1]), v[2], int(v[3]), v[5].strip() if len(v) > 5 else ''))
        marks += mine
        md = np.array([-float(at(H, m[0], m[1])) for m in mine])
        # the game opens a channel along the line through its marks in number order (KeysGeo BuildNavSegs: marks named
        # alike, legs up to 420 m, dredged to 2.7 m wherever the line is more than 1 m off the shore). So a leg is open
        # unless the grid has land on the line; this is the same rule, on the baked grid.
        chans = {}
        for m in mine:
            if m[2] not in 'RG': continue      # the game reads the daybeacons, not the buoys
            nm = m[4]
            for cut in (' Daybeacon', ' Light', ' Buoy'):
                k = nm.rfind(cut)
                if k > 0: nm = nm[:k]
            chans.setdefault(nm, []).append(m)
        shut, clipped, nseg, natural = [], [], 0, 0
        for nm, ms in sorted(chans.items()):
            ms.sort(key=lambda m: m[3])
            for a, b in zip(ms[:-1], ms[1:]):
                L = float(np.hypot(b[0] - a[0], b[1] - a[1]))
                if L > 420 or L < 1: continue
                t = np.linspace(0, 1, max(2, int(L / 2)))
                lx, lz = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
                sh = at(S, lx, lz)
                nseg += 1
                if (-at(H, lx, lz)).min() >= 2.7: natural += 1
                dry = float((sh <= 1).sum() * L / len(t))
                if dry > 0:
                    # the straight line clips land (a bridge abutment, a point): is there water round it? Flood the water
                    # (2 m and more off the shore) within 150 m of the leg from one mark and see if it reaches the other
                    i0, i1 = max(0, int((min(a[0], b[0]) - 150 - x0) / cell)), int((max(a[0], b[0]) + 150 - x0) / cell) + 1
                    j0, j1 = max(0, int((min(a[1], b[1]) - 150 - z0) / cell)), int((max(a[1], b[1]) + 150 - z0) / cell) + 1
                    lab, _ = ndimage.label(S[j0:j1, i0:i1] >= 2)
                    cellof = lambda m: lab[min(lab.shape[0] - 1, int(round((m[1] - z0) / cell)) - j0), min(lab.shape[1] - 1, int(round((m[0] - x0) / cell)) - i0)]
                    way_round = cellof(a) != 0 and cellof(a) == cellof(b)
                    clipped.append(f'{nm} {a[3]}-{b[3]} ({dry:.0f} m of {L:.0f} m)')
                    if not way_round: shut.append(f'{nm} {a[3]}-{b[3]}')
        print(f'{ph} marks: {len(mine)} ({sum(1 for m in mine if m[2] in "RG")} daybeacons) in {len(chans)} named channels; water at the marks {md.min():.1f}..{md.max():.1f} m (median {np.median(md):.1f}); '
              f'{nseg} channel legs: {natural} already 2.7 m deep, {nseg - natural - len(clipped)} the game dredges clear, {len(clipped)} where the straight line clips land'
              + (' (' + '; '.join(clipped) + ')' if clipped else '') + f'; legs with no water way round: {len(shut)}' + (' (' + '; '.join(shut) + ')' if shut else ''))

    # ---- the picture
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    k = 2       # every second cell: the whole map is 65 km long
    fig, ax = plt.subplots(figsize=(48, 48 * (z1 - z0) / (x1 - x0) + 1.2))
    Hs, Ss = H[::k, ::k], S[::k, ::k]
    land = Ss < 0
    ax.imshow(np.where(land, np.nan, -Hs), extent=[x0, x1, z1, z0], cmap='turbo_r', vmin=0, vmax=14)
    ax.imshow(np.where(land, Hs, np.nan), extent=[x0, x1, z1, z0], cmap='Greys_r', vmin=-1, vmax=4)
    ax.contour(np.linspace(x0, x1, nx)[::4], np.linspace(z0, z1, nz)[::4], -np.nan_to_num(H[::4, ::4], nan=0), levels=[45], colors='w', linewidths=.6)
    cxs = np.arange(x0, x1, 50.0)
    ax.plot(cxs, [chain(x) for x in cxs], 'm', lw=.7)
    for p, g in G.items():
        gx1, gz1 = X1(g), Z1(g)
        ax.plot([g.x0, gx1, gx1, g.x0, g.x0], [g.z0, g.z0, gz1, gz1, g.z0], 'k--', lw=.6)
        ax.text(g.x0 + 60, g.z0 + 190, p, fontsize=11, color='k')
        for i in g.islands:
            if i['name'] and i['area'] > 6000:
                ax.text(i['x'], i['z'], i['name'], fontsize=5.5, color='k', ha='center')
        pj = os.path.join(geo, p + '_places.json')
        if os.path.exists(pj):
            pl = json.load(open(pj))
            for b in pl.get('bridges', []):
                ax.plot([b['a'][0], b['b'][0]], [b['a'][1], b['b'][1]], 'r', lw=2)
                ax.text((b['a'][0] + b['b'][0]) / 2, (b['a'][1] + b['b'][1]) / 2 + 170, b['name'], fontsize=5, color='darkred', ha='center')
            for r in pl.get('reef_sites', []):
                ax.plot(r['pts'][0], r['pts'][1], 'w*', ms=7); ax.text(r['pts'][0], r['pts'][1] + 150, r['name'], fontsize=5, color='w', ha='center')
            for l in pl.get('lights', []):
                ax.plot(l['pts'][0], l['pts'][1], 'y^', ms=6)
        cp = os.path.join(geo, p + '_canals.txt')
        if os.path.exists(cp):
            for ln in open(cp):
                v = ln.split()
                if not v or v[0] == '#': continue
                w = v[0] == 'w'
                q = np.array([float(t) for t in v[(2 if w else 1):]]).reshape(-1, 2)
                ax.plot(q[:, 0], q[:, 1], color='c' if w else 'b', lw=.5)
    for m in marks:
        ax.plot(m[0], m[1], 's' if m[2] in 'Gg' else '^', color='g' if m[2] in 'Gg' else 'r', ms=2)
    ax.plot(px, pz, 'w--', lw=.8)
    ax.set_title('The real map: ' + ' + '.join(G) + ' in the game frame (metres). Colour = depth clipped at 14 m, white line = 45 m, magenta = the chain line, red = US-1 bridges, '
                 'stars = reefs, yellow = lights, dashed white = the Hawk Channel line, dashed black = the files, blue = canals')
    plt.savefig(out, dpi=50, bbox_inches='tight')


if __name__ == '__main__':
    main()
