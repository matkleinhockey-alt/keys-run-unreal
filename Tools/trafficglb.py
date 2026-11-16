"""Turn a generated passing-boat model (textured GLB) into Content/KeysRun/Props/<key>.krfish for AKeysTraffic.

    python Tools/trafficglb.py <in.glb> <key> [--tris 14000] [--draft .055]

Same colour baking as propglb.py. The boat is laid in the traffic hull's frame: x forward to the bow, y up, z to
port, exactly 1 unit long, centred on its length and beam, with y = 0 at the waterline: the keel's depth amidships
(away from the engines and running gear that hang lower at the stern) plus --draft of the length.
The bow is the end whose bottom sits higher (the forefoot sweeps up; engines and props hang low at the stern).
"""
import argparse, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fishglb import load, weld, decimate


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb"); ap.add_argument("key")
    ap.add_argument("--tris", type=int, default=14000)
    ap.add_argument("--draft", type=float, default=.055)
    ap.add_argument("--flip", action="store_true", help="bow detection got it backwards")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "KeysRun", "Props"))
    a = ap.parse_args()
    V, F, C = load(a.glb)
    V, F, C = weld(V, F, C)
    V = V - V.mean(0)
    # the long horizontal axis becomes x
    if np.ptp(V[:, 2]) > np.ptp(V[:, 0]):
        V = V[:, [2, 1, 0]] * np.array([1, 1, -1])   # a quarter turn about y (keeps it right-handed)
    lo, hi = V.min(0), V.max(0)
    V = V - np.array([(lo[0] + hi[0]) / 2, 0, (lo[2] + hi[2]) / 2])
    Ln = np.ptp(V[:, 0])
    x = V[:, 0] / Ln
    # the bottom sweeps up into the bow, while the engines, props and rudders hang lowest at the stern
    def low(m): return V[m, 1].min() if m.sum() > 10 else 0
    wp, wn = low(x > .35), low(x < -.35)
    bow_pos = wp > wn
    if a.flip: bow_pos = not bow_pos
    if not bow_pos:
        V = V * np.array([-1, 1, -1])   # half turn about y
    V = V / Ln
    mid = np.abs(V[:, 0]) < .12
    keel = V[mid, 1].min()
    V[:, 1] -= keel + a.draft
    print(f"{a.key}: {len(V)} verts; beam {np.ptp(V[:,2]):.3f} L, height {np.ptp(V[:,1]):.3f} L, "
          f"ends lowest +{wp/Ln:.3f}/-{wn/Ln:.3f} -> bow {'+x' if bow_pos else 'was -x'}; lowest {V[:,1].min():.3f}")
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
