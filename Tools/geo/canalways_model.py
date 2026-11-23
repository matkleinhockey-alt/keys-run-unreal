"""canalways_model.py: the water the hull reads, as the game works it out, for planning canals' ways out.

A copy in numpy of what the game does with the baked files, kept to the parts that decide whether a boat floats:
KeysGeoReal (the six grids served as one map, the canal and way lines), KeysGeo::RealDepthAt (the bank rule, the
marked channels, the easing up to the shore, canals and ways) sampled on the game's 10 m depth grid, and the flood of
KeysSea::LogCanalWays (`fish:canals`). Left out, because they only ever deepen the water: marina basins, the yards'
basin, dock pockets, the named Middle Keys creeks. So a canal this model finds open is open in the game; the game is
still the judge (`fish:canals`).

Read-only on every baked file."""
import os, re

import numpy as np

from kgeo import KGeo

PHASES = ('p1', 'p2', 'p2_bay', 'p3', 'p4', 'p4_bay')      # the order the game loads them in (KeysGeoReal::LoadAll)
CANAL_PHASES = ('p2', 'p3', 'p4')
CANAL_DEPTH, WAY_DEPTH = 2.1, 1.9
NAV_HALF_W, NAV_FEATHER, NAV_DEPTH = 16.0, 20.0, 2.7


def ss(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def read_lines(path):
    """a <phase>_canals.txt -> (canals, ways): lists of (width, [(x, z), ...]), in file order"""
    canals, ways = [], []
    if not os.path.exists(path):
        return canals, ways
    for ln in open(path):
        if ln.startswith('#'):
            continue
        p = ln.split()
        w0 = 1 if p and p[0] == 'w' else 0
        if len(p) < 5 + w0:
            continue
        v = [float(q) for q in p[w0:]]
        pts = [(v[k], v[k + 1]) for k in range(1, len(v) - 1, 2)]
        (ways if w0 else canals).append((v[0], pts))
    return canals, ways


class World:
    """the map as the game holds it: one 10 m grid of heights, shore distances and mangrove from the six files"""

    def __init__(self, geo):
        self.geo = geo
        gs = [(p, KGeo(f'{geo}/{p}.kgeo')) for p in PHASES if os.path.exists(f'{geo}/{p}.kgeo')]
        self.cell = c = gs[0][1].cell
        self.x0 = min(g.x0 for _, g in gs)
        self.z0 = min(g.z0 for _, g in gs)
        x1 = max(g.x0 + (g.nx - 1) * c for _, g in gs)
        z1 = max(g.z0 + (g.nz - 1) * c for _, g in gs)
        self.nx, self.nz = int(round((x1 - self.x0) / c)) + 1, int(round((z1 - self.z0) / c)) + 1
        self.H = np.full((self.nz, self.nx), -1.0, np.float32)
        self.S = np.full((self.nz, self.nx), 300.0, np.float32)
        self.phase = np.full((self.nz, self.nx), -1, np.int8)
        self.mang = np.zeros((self.nz, self.nx), np.uint8)
        # where two files overlap the game reads the one loaded first: lay them down last first
        for n in range(len(gs) - 1, -1, -1):
            p, g = gs[n]
            i0, j0 = int(round((g.x0 - self.x0) / c)), int(round((g.z0 - self.z0) / c))
            sl = (slice(j0, j0 + g.nz), slice(i0, i0 + g.nx))
            self.H[sl] = g.height
            self.S[sl] = g.shore
            self.phase[sl] = PHASES.index(p)
            mb = f'{geo}/{p}_mangrove.bin'
            if os.path.exists(mb):
                b = np.fromfile(mb, np.uint8)
                if len(b) == 12 + g.nx * g.nz:
                    self.mang[sl] = b[12:].reshape(g.nz, g.nx)
        self.grids = [g for _, g in gs]
        self.marks = []
        for p, _ in gs:
            mp = f'{geo}/{p}_marks.txt'
            if not os.path.exists(mp):
                continue
            for ln in open(mp):
                q = ln.split()
                if ln.startswith('#') or len(q) < 5 or q[2] not in ('R', 'G'):
                    continue
                self.marks.append((float(q[0]), float(q[1]), q[2], int((re.match(r'-?\d+', q[3]) or [0])[0]), ' '.join(q[5:])))
        self.nav = self._nav_segs()

    def _nav_segs(self):
        """KeysGeo BuildNavSegs: the line through each channel's daybeacons in number order"""
        ch = {}
        for x, z, _, num, name in self.marks:
            for cut in (' Daybeacon', ' Light', ' Buoy'):
                at = name.rfind(cut)
                if at > 0:
                    name = name[:at]
            ch.setdefault(name, []).append((num, x, z))
        segs = []
        self.nav_names = []
        for name in ch:
            m = sorted(ch[name], key=lambda q: q[0])
            for a, b in zip(m, m[1:]):
                if (b[1] - a[1]) ** 2 + (b[2] - a[2]) ** 2 > 420.0 ** 2:
                    continue
                segs.append((a[1], a[2], b[1], b[2]))
                self.nav_names.append(name)
        return segs

    def chain_z(self, X):
        """US-1's line (KeysGeoReal::ChainZ): each stretch of x from the first file loaded that covers it"""
        X = np.asarray(X, float)
        out = np.zeros(X.shape)
        done = np.zeros(X.shape, bool)
        for g in self.grids:
            m = ~done & (X >= g.x0 - 1) & (X <= g.x0 + (g.nx - 1) * g.cell + 1)
            if m.any():
                out[m] = g.chain_z(X[m])
                done |= m
        if (~done).any():
            out[~done] = self.grids[0].chain_z(X[~done])
        return out

    # ---- rasters on the node grid
    def nodes(self, i0, i1, j0, j1):
        return np.meshgrid(self.x0 + np.arange(i0, i1) * self.cell, self.z0 + np.arange(j0, j1) * self.cell)

    def seg_dist(self, segs, pad, sub=None):
        """distance from every node to the nearest of segs [(ax, az, bx, bz, extra)], minus each one's `extra` (its half
        width); 1e9 beyond `pad` of all of them. sub = (i0, i1, j0, j1) keeps it to a window."""
        I0, I1, J0, J1 = sub or (0, self.nx, 0, self.nz)
        out = np.full((J1 - J0, I1 - I0), 1e9, np.float32)
        c = self.cell
        for ax, az, bx, bz, ex in segs:
            r = pad + ex
            i0 = max(I0, int(np.floor((min(ax, bx) - r - self.x0) / c))); i1 = min(I1, int(np.ceil((max(ax, bx) + r - self.x0) / c)) + 1)
            j0 = max(J0, int(np.floor((min(az, bz) - r - self.z0) / c))); j1 = min(J1, int(np.ceil((max(az, bz) + r - self.z0) / c)) + 1)
            if i0 >= i1 or j0 >= j1:
                continue
            X, Z = self.nodes(i0, i1, j0, j1)
            vx, vz = bx - ax, bz - az
            l2 = vx * vx + vz * vz
            t = np.clip(((X - ax) * vx + (Z - az) * vz) / l2, 0, 1) if l2 > 1e-9 else 0.0
            d = np.hypot(X - ax - vx * t, Z - az - vz * t) - ex
            o = out[j0 - J0:j1 - J0, i0 - I0:i1 - I0]
            np.minimum(o, d, out=o, casting='unsafe')
        return out

    @staticmethod
    def line_segs(lines):
        return [(a[0], a[1], b[0], b[1], w / 2) for w, pts in lines for a, b in zip(pts, pts[1:])]

    def base_depth(self):
        """RealDepthAt at every node, before canals and ways"""
        D = np.maximum(.3, -self.H.astype(np.float64))
        rule = .25 + 2.15 * np.power(np.clip((D - .3) / 2.1, 0, None), 2.2)
        D = np.where(D < 2.4, lerp(rule, D, ss(.75, 1.1, D)), D)
        S = self.S.astype(np.float64)
        nd = self.seg_dist([(a, b, c, d, 0.0) for a, b, c, d in self.nav], NAV_HALF_W + NAV_FEATHER)
        k = np.clip(1 - (nd - NAV_HALF_W) / NAV_FEATHER, 0, 1) * np.minimum(1.0, (S - 1) / 8)
        k = np.where((S > 1) & (D < NAV_DEPTH), np.clip(k, 0, 1), 0)
        D = lerp(D, NAV_DEPTH, k * k * (3 - 2 * k))
        t = np.maximum(0.0, S) / 25
        D = np.where(S < 25, np.minimum(D, .35 + t * (2 - t) * (D - .35)), D)
        self.navdist = nd
        return np.maximum(.3, D)

    def with_lines(self, D, canals, ways, sub=None):
        """the depth with canal and way lines dredged (the whole grid, or the window sub of it)"""
        cd = self.seg_dist(self.line_segs(canals), 4.0, sub)
        D = np.where(cd < 0, np.maximum(D, lerp(1.7, CANAL_DEPTH, ss(0, -3, cd))), D)
        wd = self.seg_dist(self.line_segs(ways), 8.0, sub)
        D = np.where(wd < 6, np.maximum(D, lerp(WAY_DEPTH, D, ss(-2, 6, wd))), D)
        return D


def fine(A):
    """a node grid read every 5 m, as the game's bilinear lookup gives it"""
    nz, nx = A.shape
    out = np.empty((nz * 2 - 1, nx * 2 - 1), A.dtype)
    out[::2, ::2] = A
    out[::2, 1::2] = (A[:, :-1] + A[:, 1:]) / 2
    out[1::2, ::2] = (A[:-1] + A[1:]) / 2
    out[1::2, 1::2] = (A[:-1, :-1] + A[:-1, 1:] + A[1:, :-1] + A[1:, 1:]) / 4
    return out


def load_all(geo):
    """every canal and way of the map, in the game's order: [(phase, width, pts)]"""
    canals, ways = [], []
    for p in CANAL_PHASES:
        c, w = read_lines(f'{geo}/{p}_canals.txt')
        canals += [(p, wd, pts) for wd, pts in c]
        ways += [(p, wd, pts) for wd, pts in w]
    return canals, ways
