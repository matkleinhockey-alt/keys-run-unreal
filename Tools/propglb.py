"""Turn a generated prop model (textured GLB) into the game's mesh format: Content/KeysRun/Props/<key>.krfish

    python Tools/propglb.py <in.glb> <key> [--tris 3000] [--upright] [--preview out.png]

Same file layout and colour baking as fishglb.py (vertex colours from the base colour texture, seams welded, decimated),
but the prop stays the way it stands: glTF +Y up becomes the game's up, it is centred over the origin with its base at
y = 0, and scaled to exactly 1 unit tall (the game scales it to its real height). --upright first turns the model so
its longest axis stands vertical (for something modelled lying down, like a lobster float), the orange end up.
"""
import argparse, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fishglb import load, weld, decimate


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb"); ap.add_argument("key")
    ap.add_argument("--tris", type=int, default=3000)
    ap.add_argument("--upright", action="store_true")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "KeysRun", "Props"))
    a = ap.parse_args()
    V, F, C = load(a.glb)
    V, F, C = weld(V, F, C)
    V = V - V.mean(0)
    if a.upright:
        # principal axis -> +Y, with the more saturated (painted) end up
        w, vec = np.linalg.eigh(np.cov(V.T))
        ax = vec[:, -1]
        sat = C.max(1) - C.min(1)
        t = V @ ax
        if sat[t > np.median(t)].mean() < sat[t < np.median(t)].mean(): ax = -ax
        y = np.array([0.0, 1.0, 0.0])
        v = np.cross(ax, y); s = np.linalg.norm(v); c = float(ax @ y)
        if s > 1e-6:
            k = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
            R = np.eye(3) + k + k @ k * ((1 - c) / s ** 2)
            V = V @ R.T
    lo, hi = V.min(0), V.max(0)
    V = V - np.array([(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2])
    V = V / (hi[1] - lo[1])
    print(f"{a.key}: {len(V)} verts welded; footprint {np.ptp(V[:,0]):.2f} x {np.ptp(V[:,2]):.2f} (height 1)")
    Vd, Fd, Cd, Nd = decimate(V, F, C, a.tris)
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, a.key + ".krfish")
    with open(path, "wb") as f:
        f.write(b"KRF1"); f.write(struct.pack("<I", 1))
        f.write(struct.pack("<II", len(Vd), len(Fd)))
        rgb = (np.clip(Cd, 0, 1) * 255 + .5).astype(np.uint8)
        rec = np.zeros(len(Vd), dtype=[("p", "<f4", 3), ("n", "<f4", 3), ("c", "u1", 4)])
        rec["p"] = Vd; rec["n"] = Nd; rec["c"][:, :3] = rgb; rec["c"][:, 3] = 255
        f.write(rec.tobytes()); f.write(Fd.astype("<u4").tobytes())
    print(f"  {len(Vd)} verts, {len(Fd)} tris -> {path}")


if __name__ == "__main__":
    sys.exit(main())
