"""Recolour a baked reef model (Content/KeysRun/Props/reef_<key>.krfish and its _lo copy) toward a living colour,
keeping the model's own shading: python Tools/reeftint.py <key> <hex> [strength 0..1]. The text-to-3D sea fan came
out grey; Gorgonia ventalina is purple."""
import os, struct, sys
import numpy as np
key, hx = sys.argv[1], int(sys.argv[2], 16); k = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
tint = np.array([(hx >> 16) & 255, (hx >> 8) & 255, hx & 255], float) / 255
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "KeysRun", "Props")
for suf in ("", "_lo"):
    p = os.path.join(root, f"reef_{key}{suf}.krfish")
    b = bytearray(open(p, "rb").read()); nv = struct.unpack("<I", b[8:12])[0]
    rec = np.frombuffer(bytes(b[16:16 + nv * 28]), dtype=[("p", "<f4", 3), ("n", "<f4", 3), ("c", "u1", 4)]).copy()
    c = rec["c"][:, :3] / 255.0
    lum = c @ np.array([.3, .59, .11]); ref = tint @ np.array([.3, .59, .11])
    new = np.clip(tint[None, :] * (lum / ref)[:, None], 0, 1)
    rec["c"][:, :3] = ((c * (1 - k) + new * k) * 255 + .5).astype(np.uint8)
    b[16:16 + nv * 28] = rec.tobytes(); open(p, "wb").write(b)
    print("tinted", p)
