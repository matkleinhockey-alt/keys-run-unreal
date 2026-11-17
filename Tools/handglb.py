"""Turn a generated gripping hand (textured GLB: a right fist closed round a thin rod, forearm and shirt cuff) into
Content/KeysRun/Props/hand_grip.krfish for the first-person rod hands (KeysBoatPawn AddGripHand).

    python Tools/handglb.py <in.glb> [--tris 5000] [--width .1] [--grip .024] [--flipaxis] [--preview out.png]

The rod the model was made holding is cut away (the grey, unsaturated faces along the fitted axis). The output is in
meters, in the grip frame: x along the rod axis, z out from the axis toward the forearm (perpendicular to x), y = z cross
x, the origin on the axis in the middle of the fist. The fist is scaled so its width along the rod is --width, then the
tunnel through it is opened out to --grip radius (the game's fore grip is far thicker than the model's rod), pushing the
fingers out and letting the push fade off toward the back of the hand.
"""
import argparse, warnings
warnings.filterwarnings('ignore')
import argparse, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fishglb import load, weld, decimate


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb")
    ap.add_argument("--tris", type=int, default=5000)
    ap.add_argument("--width", type=float, default=.1)
    ap.add_argument("--grip", type=float, default=.024)
    ap.add_argument("--flipaxis", action="store_true")
    ap.add_argument("--preview")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "KeysRun", "Props"))
    a = ap.parse_args()
    V, F, C = load(a.glb)
    V = V.astype(np.float64); C = C.astype(np.float64)
    mx, mn = C.max(1), C.min(1)
    sat = mx - mn
    # fit the rod's axis to its grey vertices, dropping the outliers (creases, nails) a few times
    R = V[(sat < .06) & (mx < .45)]
    for _ in range(4):
        c = R.mean(0); ax = np.linalg.svd(R - c, full_matrices=False)[2][0]
        d = R - c; rad = np.linalg.norm(d - np.outer(d @ ax, ax), axis=1)
        R = R[rad < np.median(rad) * 1.6]
    c = R.mean(0); ax = np.linalg.svd(R - c, full_matrices=False)[2][0]
    d = R - c; rr = np.median(np.linalg.norm(d - np.outer(d @ ax, ax), axis=1))
    d = V - c; t = d @ ax; rad = np.linalg.norm(d - np.outer(t, ax), axis=1)
    rod = (rad < rr * 1.3) & (sat < .1)
    keepF = ~rod[F].all(1)
    F = F[keepF]
    used = np.unique(F); remap = -np.ones(len(V), int); remap[used] = np.arange(len(used))
    V, C, F = V[used], C[used], remap[F]
    V, F, C = weld(V, F, C)
    skin = C[:, 0] > C[:, 2] + .08
    blue = C[:, 2] > C[:, 0] + .15
    if a.flipaxis: ax = -ax
    d = V - c; t = d @ ax
    tlo, thi = np.percentile(t[skin], [1, 99])
    c = c + ax * (tlo + thi) / 2
    fa = V[blue].mean(0) - c; fa -= ax * (fa @ ax); fa /= np.linalg.norm(fa)
    by = np.cross(fa, ax)
    d = V - c
    P = np.stack([d @ ax, d @ by, d @ fa], 1) * (a.width / (thi - tlo))
    r_t = rr * a.width / (thi - tlo)
    # open the tunnel out to the game's grip
    rho = np.linalg.norm(P[:, 1:], axis=1)
    push = (a.grip - r_t) * np.exp(-np.maximum(rho - r_t, 0) / .03)
    P[:, 1:] *= ((rho + push) / np.maximum(rho, 1e-6))[:, None]
    print(f"rod radius {rr:.4f} model units -> {r_t*1000:.1f} mm, opened to {a.grip*1000:.0f} mm; "
          f"forearm reaches {P[blue, 2].max():.3f} m; {len(P)} verts")
    # the rod's far end cap, left floating past the fist
    sat2 = C.max(1) - C.min(1)
    cap = (np.abs(P[:, 0]) > .07) & (sat2 < .15) & (P[:, 2] < .03)
    F = F[~cap[F].any(1)]
    used = np.unique(F); remap = -np.ones(len(P), int); remap[used] = np.arange(len(used))
    P, C, F = P[used], C[used], remap[F]
    Vd, Fd, Cd, Nd = decimate(P, F, C, a.tris)
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, "hand_grip.krfish")
    with open(path, "wb") as f:
        f.write(b"KRF1"); f.write(struct.pack("<I", 1))
        f.write(struct.pack("<II", len(Vd), len(Fd)))
        rec = np.zeros(len(Vd), dtype=[("p", "<f4", 3), ("n", "<f4", 3), ("c", "u1", 4)])
        rec["p"] = Vd; rec["n"] = Nd; rec["c"][:, :3] = (np.clip(Cd, 0, 1) * 255 + .5).astype(np.uint8); rec["c"][:, 3] = 255
        f.write(rec.tobytes()); f.write(Fd.astype("<u4").tobytes())
    print(f"  {len(Vd)} verts, {len(Fd)} tris -> {path}")
    if a.preview:
        import matplotlib; matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, axs = plt.subplots(1, 3, figsize=(14, 5))
        for k, (i, j, tl) in enumerate(((0, 2, "x along rod, z to forearm"), (1, 2, "down the rod: y, z"), (0, 1, "x, y"))):
            axs[k].scatter(Vd[:, i], Vd[:, j], s=1, c=np.clip(Cd, 0, 1)); axs[k].set_aspect("equal"); axs[k].set_title(tl)
            if k == 1: axs[k].add_patch(plt.Circle((0, 0), a.grip, fill=False, color="r"))
        plt.savefig(a.preview, dpi=90)


if __name__ == "__main__":
    sys.exit(main())
