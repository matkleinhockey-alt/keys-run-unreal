# Tiling concrete textures for the bridges (M_KR_Concrete). Every map tiles seamlessly (built from periodic noise).
#   T_KR_Concrete_D   2048  base colour: weathered cast concrete, near-neutral so the mesh's vertex colours tint it
#                     (mean ~0.62 in sRGB; the material divides it back out), pores, aggregate, faint formwork board lines
#   T_KR_Concrete_N   2048  tangent-space normal from the same height field
#   T_KR_ConcreteDet_N 1024 detail normal: fine grit, tiled much smaller on top
# usage: devkit/pyenv/bin/python Tools/concrete_tex.py <out dir>
import sys, os
import numpy as np
from PIL import Image

rng = np.random.default_rng(7)


def periodic_noise(n, octaves, base, persistence=.5):
    """fbm that tiles: band-limited white noise filtered in the frequency domain, summed over octaves"""
    out = np.zeros((n, n))
    amp, tot = 1.0, 0.0
    fy = np.fft.fftfreq(n)[:, None] * n
    fx = np.fft.fftfreq(n)[None, :] * n
    r = np.sqrt(fx * fx + fy * fy)
    for o in range(octaves):
        f0 = base * 2 ** o
        w = np.exp(-((r - f0) / (f0 * .6)) ** 2)
        spec = np.fft.fft2(rng.standard_normal((n, n))) * w
        layer = np.real(np.fft.ifft2(spec))
        layer /= layer.std() + 1e-9
        out += layer * amp
        tot += amp
        amp *= persistence
    return out / tot


def specks(n, count, rmin, rmax):
    """round pits / stones scattered with wrap-around (so they tile)"""
    h = np.zeros((n, n))
    yy, xx = np.mgrid[0:n, 0:n]
    for _ in range(count):
        cx, cy = rng.integers(0, n, 2)
        r = rng.uniform(rmin, rmax)
        x0, x1 = int(cx - r - 2), int(cx + r + 3)
        y0, y1 = int(cy - r - 2), int(cy + r + 3)
        ys = np.arange(y0, y1) % n
        xs = np.arange(x0, x1) % n
        dy = (np.arange(y0, y1) - cy)[:, None]
        dx = (np.arange(x0, x1) - cx)[None, :]
        d = np.sqrt(dx * dx + dy * dy) / r
        bump = np.clip(1 - d, 0, 1) ** 1.5
        h[np.ix_(ys, xs)] = np.maximum(h[np.ix_(ys, xs)], bump)
    return h


def normal_from(h, strength):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * .5
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * .5
    nx, ny, nz = -dx * strength, -dy * strength, np.ones_like(h)
    l = np.sqrt(nx * nx + ny * ny + nz * nz)
    n = np.stack([nx / l, ny / l, nz / l], -1)
    return ((n * .5 + .5) * 255).round().clip(0, 255).astype(np.uint8)


def save(a, path):
    Image.fromarray(a).save(path)
    print("wrote", path, a.shape)


def main(out):
    os.makedirs(out, exist_ok=True)
    N = 2048
    # the tile is 4 m of concrete: large blotches, mid mottling, fine grain
    big = periodic_noise(N, 3, 2.0)
    mid = periodic_noise(N, 3, 9.0)
    fine = periodic_noise(N, 2, 120.0)
    pores = specks(N, 9000, 1.0, 3.2)          # air pockets: small pits
    stones = specks(N, 2500, 2.5, 7.0)          # exposed aggregate where the skin has worn
    # formwork: the old bridges were cast against plank forms, leaving faint horizontal board lines (8 boards per tile)
    v = (np.arange(N) / N * 8) % 1.0
    board = np.exp(-((np.minimum(v, 1 - v)) / .006) ** 2)[:, None] * np.ones((1, N))
    board_var = (periodic_noise(N, 1, 4.0)[:, :1] * .5 + .5)   # each board a slightly different shade
    rows = (np.floor(np.arange(N) / N * 8)).astype(int)
    shade_rows = rng.uniform(-.035, .035, 8)[rows][:, None]

    lum = .62 + .045 * big + .035 * mid + .025 * fine + shade_rows - .16 * pores - .05 * board + .05 * (stones > .3) * (mid > .4)
    lum = np.clip(lum, 0, 1)
    warm = .5 + .5 * np.tanh(big * 1.2)
    rgb = np.stack([lum * (1 + .03 * warm), lum * (1 + .005 * warm), lum * (1 - .03 * warm)], -1)
    save((np.clip(rgb, 0, 1) ** (1 / 1.0) * 255).round().astype(np.uint8), os.path.join(out, "T_KR_Concrete_D.png"))

    h = .35 * mid + .15 * fine - 1.2 * pores + .5 * stones * (mid > .2) - .6 * board
    save(normal_from(h, 2.2), os.path.join(out, "T_KR_Concrete_N.png"))

    D = 1024
    grit = periodic_noise(D, 2, 90.0) + .6 * periodic_noise(D, 1, 30.0) - 1.5 * specks(D, 6000, .8, 2.0)
    save(normal_from(grit, 1.6), os.path.join(out, "T_KR_ConcreteDet_N.png"))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "Tools/tex")
