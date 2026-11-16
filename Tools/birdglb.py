"""Turn a generated seabird (textured GLB, wings spread) into Content/KeysRun/Props/bird_<key>.krfish for AKeysLife.

    python Tools/birdglb.py <in.glb> <key> [--tris 900] [--flip] [--preview out.png]

The bird is laid in the seabird frame AKeysLife poses: x along the span (unit span, wingtip to wingtip), y up, z forward
to the head, the body's centre line at the origin. The wings are flattened: whatever V (dihedral) the model was made
with is rotated out about each shoulder, so the game's flap angles start from a level glide. The game splits the mesh
by |x| into body, inner wing (shoulder to elbow) and hand (elbow to tip) and turns each about its joint.
"""
import argparse, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fishglb import load, weld, decimate

SHOULDER = .07   # of the span, from the centre line: where the wing leaves the body


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb"); ap.add_argument("key")
    ap.add_argument("--tris", type=int, default=900)
    ap.add_argument("--flip", action="store_true", help="head detection got it backwards")
    ap.add_argument("--preview")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "KeysRun", "Props"))
    a = ap.parse_args()
    V, F, C = load(a.glb)
    V, F, C = weld(V, F, C)
    V = V - V.mean(0)
    if np.ptp(V[:, 2]) > np.ptp(V[:, 0]):   # the span becomes x
        V = V[:, [2, 1, 0]] * np.array([1, 1, -1])
    lo, hi = V.min(0), V.max(0)
    V[:, 0] -= (lo[0] + hi[0]) / 2
    V = V / np.ptp(V[:, 0])
    ax = np.abs(V[:, 0])
    core = ax < .04
    # head forward: the wings sweep back from the body, so the wing's own trailing edge points to the tail
    wing = ax > .3
    zc = V[core, 2]
    head_pos = (zc.max() - V[wing, 2].mean()) < (V[wing, 2].mean() - zc.min())
    if a.flip: head_pos = not head_pos
    if not head_pos: V = V * np.array([-1, 1, -1])
    # centre the body: its centre line at y = 0, the wing roots at z = 0
    V[:, 1] -= V[ax < .04, 1].mean()
    roots = (ax > SHOULDER) & (ax < SHOULDER + .08)
    V[:, 2] -= V[roots, 2].mean()
    # flatten the dihedral: fit the wing's rise against |x| and rotate it out about each shoulder
    w = ax > SHOULDER
    slope = np.polyfit(ax[w] - SHOULDER, V[w, 1], 1)[0]
    ang = np.arctan(slope)
    for s in (-1, 1):
        m = (V[:, 0] * s) > SHOULDER
        x = V[m, 0] * s - SHOULDER; y = V[m, 1]
        c, sn = np.cos(-ang), np.sin(-ang)
        V[m, 0] = (SHOULDER + x * c - y * sn) * s
        V[m, 1] = x * sn + y * c
    V[:, 0] /= np.ptp(V[:, 0])
    print(f"{a.key}: {len(V)} verts; length {np.ptp(V[:,2]):.3f} span, dihedral {np.degrees(ang):.1f} deg flattened, "
          f"head {'+z' if head_pos else 'was -z'}")
    Vd, Fd, Cd, Nd = decimate(V, F, C, a.tris)
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, "bird_" + a.key + ".krfish")
    with open(path, "wb") as f:
        f.write(b"KRF1"); f.write(struct.pack("<I", 1))
        f.write(struct.pack("<II", len(Vd), len(Fd)))
        rgb = (np.clip(Cd, 0, 1) * 255 + .5).astype(np.uint8)
        rec = np.zeros(len(Vd), dtype=[("p", "<f4", 3), ("n", "<f4", 3), ("c", "u1", 4)])
        rec["p"] = Vd; rec["n"] = Nd; rec["c"][:, :3] = rgb; rec["c"][:, 3] = 255
        f.write(rec.tobytes()); f.write(Fd.astype("<u4").tobytes())
    print(f"  {len(Vd)} verts, {len(Fd)} tris -> {path}")
    if a.preview:
        import matplotlib; matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, axs = plt.subplots(1, 2, figsize=(10, 4))
        for k, (i, j, t) in enumerate(((0, 2, "top: x right, z (head) up"), (0, 1, "front: x right, y up"))):
            axs[k].scatter(Vd[:, i], Vd[:, j], s=2, c=np.clip(Cd, 0, 1)); axs[k].set_aspect("equal"); axs[k].set_title(t)
        plt.savefig(a.preview, dpi=90)


if __name__ == "__main__":
    sys.exit(main())
