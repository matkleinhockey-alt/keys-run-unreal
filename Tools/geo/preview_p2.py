"""preview_p2.py <geo dir> <out.png>: the whole real map as the game will see it, p1 + p2 + p2_bay side by side, and
the checks Phase 2 has to pass on the data alone:

  - the seams: heights and shore distances along every shared edge, the largest difference
  - Hawk Channel: a line down the ocean side from Marathon to Tavernier, the shoalest water on it, and the waypoints
    (the scripted run steers by them)
  - the marked channels: the water at every mark and between consecutive marks of a channel

Reads only baked files (through kgeo.py). Prints one summary line per check."""
import json, os, re, sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kgeo import KGeo

PHASES = ('p1', 'p2', 'p2_bay')


def main():
    geo, out = sys.argv[1], sys.argv[2]
    G = {p: KGeo(os.path.join(geo, p + '.kgeo')) for p in PHASES if os.path.exists(os.path.join(geo, p + '.kgeo'))}
    cell = G['p1'].cell
    x0 = min(g.x0 for g in G.values()); z0 = min(g.z0 for g in G.values())
    x1 = max(g.x0 + (g.nx - 1) * cell for g in G.values()); z1 = max(g.z0 + (g.nz - 1) * cell for g in G.values())
    nx, nz = int(round((x1 - x0) / cell)) + 1, int(round((z1 - z0) / cell)) + 1
    H = np.full((nz, nx), np.nan); S = np.full((nz, nx), np.nan)
    for p in reversed(list(G)):     # p1 last: where two files share an edge the game reads the first loaded
        g = G[p]
        i, j = int(round((g.x0 - x0) / cell)), int(round((g.z0 - z0) / cell))
        H[j:j + g.nz, i:i + g.nx] = g.height; S[j:j + g.nz, i:i + g.nx] = g.shore
    print(f'map: {len(G)} grids, {nx} x {nz} cells, x {x0:.0f}..{x1:.0f}, z {z0:.0f}..{z1:.0f}; cells with no data {int(np.isnan(H).sum())}')

    # ---- seams
    def seam(a, b, side):
        ga, gb = G[a], G[b]
        if side == 'x':      # a's last column against b's first
            assert abs(ga.x0 + (ga.nx - 1) * cell - gb.x0) < 1e-6
            lo, hi = max(ga.z0, gb.z0), min(ga.z0 + (ga.nz - 1) * cell, gb.z0 + (gb.nz - 1) * cell)
            ja, jb, n = int(round((lo - ga.z0) / cell)), int(round((lo - gb.z0) / cell)), int(round((hi - lo) / cell)) + 1
            dh = np.abs(ga.height[ja:ja + n, -1] - gb.height[jb:jb + n, 0]); ds = np.abs(ga.shore[ja:ja + n, -1] - gb.shore[jb:jb + n, 0])
            # and the step from each side's edge to its own next cell, so a hidden cliff one cell in would show
            st = max(np.abs(np.diff(ga.height[ja:ja + n, -3:], axis=1)).max(), np.abs(np.diff(gb.height[jb:jb + n, :3], axis=1)).max())
            where = f'x = {gb.x0:.0f}, z {lo:.0f}..{hi:.0f}'
        else:                # a's last row against b's first
            assert abs(ga.z0 + (ga.nz - 1) * cell - gb.z0) < 1e-6
            lo, hi = max(ga.x0, gb.x0), min(ga.x0 + (ga.nx - 1) * cell, gb.x0 + (gb.nx - 1) * cell)
            ia, ib, n = int(round((lo - ga.x0) / cell)), int(round((lo - gb.x0) / cell)), int(round((hi - lo) / cell)) + 1
            dh = np.abs(ga.height[-1, ia:ia + n] - gb.height[0, ib:ib + n]); ds = np.abs(ga.shore[-1, ia:ia + n] - gb.shore[0, ib:ib + n])
            st = max(np.abs(np.diff(ga.height[-3:, ia:ia + n], axis=0)).max(), np.abs(np.diff(gb.height[:3, ib:ib + n], axis=0)).max())
            where = f'z = {gb.z0:.0f}, x {lo:.0f}..{hi:.0f}'
        print(f'seam {a} | {b} ({where}): {n} cells, height differs by at most {dh.max():.3f} m, shore distance by at most {ds.max():.2f} m; '
              f'steepest step within two cells of the edge {st:.2f} m')
    if 'p2' in G: seam('p1', 'p2', 'x')
    if 'p2_bay' in G:
        seam('p2_bay', 'p1', 'z')
        if 'p2' in G: seam('p2_bay', 'p2', 'x')

    def at(A, x, z):
        return A[np.clip(np.round((np.asarray(z) - z0) / cell).astype(int), 0, nz - 1), np.clip(np.round((np.asarray(x) - x0) / cell).astype(int), 0, nx - 1)]

    def chain(x):
        g = G['p2'] if ('p2' in G and x > G['p2'].x0) else G['p1']
        return float(g.chain_z(x))

    # ---- Hawk Channel: from off Boot Key Harbor east, the deepest water between 500 m and 1900 m south of US-1,
    # smoothed, checked every 10 m
    xs = np.arange(-400, x1 - 900, 100.0)
    zs = []
    for x in xs:
        c = chain(x)
        band = np.arange(c + 500, c + 1900, 20.0)
        d = -at(H, np.full_like(band, x), band)
        # the middle of the widest run of water 4 m deep or more (Hawk Channel), not a single deep hole
        ok = d >= 4
        best, cur, run0 = None, 0, 0
        for k, v in enumerate(list(ok) + [False]):
            if v:
                if cur == 0: run0 = k
                cur += 1
            else:
                if cur and (best is None or cur > best[0]): best = (cur, run0)
                cur = 0
        zs.append(band[best[1] + best[0] // 2] if best else band[int(np.argmax(d))])
    zs = np.array(zs)
    from scipy import ndimage
    zs = ndimage.gaussian_filter1d(zs, 8, mode='nearest')
    px = np.arange(xs[0], xs[-1], 10.0); pz = np.interp(px, xs, zs)
    pd, ps = -at(H, px, pz), at(S, px, pz)
    k = int(np.argmin(pd))
    way = [(float(px[i]), float(pz[i])) for i in range(0, len(px), 150)] + [(float(px[-1]), float(pz[-1]))]
    print(f'Hawk Channel line x {px[0]:.0f}..{px[-1]:.0f} ({(px[-1] - px[0]) / 1000:.1f} km): shoalest {pd.min():.1f} m at x {px[k]:.0f}, z {pz[k]:.0f}; '
          f'nearest shore {ps.min():.0f} m; at the join (x 7500) {float(-at(H, 7500, np.interp(7500, xs, zs))):.1f} m')
    json.dump(dict(hawk=[[round(a, 1), round(b, 1)] for a, b in way]), open(os.path.join(os.path.dirname(out), 'p2_hawk_route.json'), 'w'))

    # ---- marks and the channels they mark (p2 only)
    marks = []
    mp = os.path.join(geo, 'p2_marks.txt')
    if os.path.exists(mp):
        for ln in open(mp):
            if ln.startswith('#'): continue
            v = ln.split(None, 5)
            marks.append((float(v[0]), float(v[1]), v[2], int(v[3]), v[5].strip() if len(v) > 5 else ''))
        md = np.array([-float(at(H, m[0], m[1])) for m in marks])
        # the game opens a channel along the line through its marks in number order (KeysGeo BuildNavSegs: marks named
        # alike, legs up to 420 m, dredged to 2.7 m wherever the line is more than 1 m off the shore). So a leg is open
        # unless the grid has land on the line; this is the same rule, on the baked grid.
        chans = {}
        for m in marks:
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
                    i0, i1 = int((min(a[0], b[0]) - 150 - x0) / cell), int((max(a[0], b[0]) + 150 - x0) / cell) + 1
                    j0, j1 = int((min(a[1], b[1]) - 150 - z0) / cell), int((max(a[1], b[1]) + 150 - z0) / cell) + 1
                    lab, _ = ndimage.label(S[j0:j1, i0:i1] >= 2)
                    cellof = lambda m: lab[int(round((m[1] - z0) / cell)) - j0, int(round((m[0] - x0) / cell)) - i0]
                    way_round = cellof(a) != 0 and cellof(a) == cellof(b)
                    clipped.append(f'{nm} {a[3]}-{b[3]} ({dry:.0f} m of {L:.0f} m)')
                    if not way_round: shut.append(f'{nm} {a[3]}-{b[3]}')
        print(f'p2 marks: {len(marks)} ({sum(1 for m in marks if m[2] in "RG")} daybeacons) in {len(chans)} named channels; water at the marks {md.min():.1f}..{md.max():.1f} m (median {np.median(md):.1f}); '
              f'{nseg} channel legs: {natural} already 2.7 m deep, {nseg - natural - len(clipped)} the game dredges clear, {len(clipped)} where the straight line clips land'
              + (' (' + '; '.join(clipped) + ')' if clipped else '') + f'; legs with no water way round: {len(shut)}' + (' (' + '; '.join(shut) + ')' if shut else ''))

    # ---- the picture
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(34, 14.5))
    land = S < 0
    ax.imshow(np.where(land, np.nan, -H), extent=[x0, x1, z1, z0], cmap='turbo_r', vmin=0, vmax=14)
    ax.imshow(np.where(land, H, np.nan), extent=[x0, x1, z1, z0], cmap='Greys_r', vmin=-1, vmax=4)
    ax.contour(np.linspace(x0, x1, nx)[::4], np.linspace(z0, z1, nz)[::4], -H[::4, ::4], levels=[45], colors='w', linewidths=.6)
    cxs = np.arange(x0, x1, 50.0)
    ax.plot(cxs, [chain(x) for x in cxs], 'm', lw=.7)
    for p, g in G.items():
        gx1, gz1 = g.x0 + (g.nx - 1) * cell, g.z0 + (g.nz - 1) * cell
        ax.plot([g.x0, gx1, gx1, g.x0, g.x0], [g.z0, g.z0, gz1, gz1, g.z0], 'k--', lw=.6)
        ax.text(g.x0 + 60, g.z0 + 160, p, fontsize=11, color='k')
        for i in g.islands:
            if i['name'] and i['area'] > 6000:
                ax.text(i['x'], i['z'], i['name'], fontsize=6, color='k', ha='center')
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
    for m in marks:
        ax.plot(m[0], m[1], 's' if m[2] in 'Gg' else '^', color='g' if m[2] in 'Gg' else 'r', ms=2.5)
    ax.plot(px, pz, 'w--', lw=.8)
    ax.set_title('The real map: p1 + p2 + p2_bay in the game frame (metres). Colour = depth clipped at 14 m, white line = 45 m, magenta = US-1, red = US-1 bridges, '
                 'stars = reefs, dashed white = the Hawk Channel line, dashed black = the files')
    plt.savefig(out, dpi=60, bbox_inches='tight')


if __name__ == '__main__':
    main()
