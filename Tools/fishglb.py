"""Turn a generated fish model (textured GLB) into the game's own fish mesh: Content/KeysRun/Fish/<key>.krfish

    python Tools/fishglb.py <in.glb> <key> [--nose +x|-x|+z|-z] [--flip-up] [--lo 2400] [--hi 16000] [--preview out.png]

The fish is turned into the frame KeysLife builds its fish in (nose toward -Z, back up +Y, X across), centred, and scaled
so it runs exactly 1 unit nose to tail (the game scales it by the species' length). The texture is baked into the vertex
colours (the fish material is vertex-colour lit), so the mesh is decimated to two sizes: lo for the fish swimming about
(the tail beat is done on the CPU every frame) and hi for the fish on the line, the catch card and the diver's spear.

Needs numpy, pillow, trimesh, scipy and fast_simplification (numpy<2 on Python 3.9).

File layout (little endian): b"KRF1", u32 lod count, then per lod: u32 verts, u32 tris, verts * (3 f32 pos, 3 f32 normal,
4 u8 sRGB colour), tris * 3 u32.
"""
import argparse, os, struct, sys
import numpy as np
import trimesh
from PIL import Image
from scipy.spatial import cKDTree
import fast_simplification


def load(path):
    sc = trimesh.load(path, force="scene")
    parts = []
    for name, geom in sc.geometry.items():
        for node in sc.graph.geometry_nodes.get(name, [name]):
            T, _ = sc.graph[node]
            g = geom.copy()
            g.apply_transform(T)
            parts.append(g)
    V, F, C = [], [], []
    off = 0
    for g in parts:
        col = vertex_colours(g)
        V.append(g.vertices); F.append(g.faces + off); C.append(col)
        off += len(g.vertices)
    return np.vstack(V), np.vstack(F), np.vstack(C)


def vertex_colours(g):
    """sRGB 0..1 per vertex: the base colour texture sampled at the vertex UV, times the factor."""
    n = len(g.vertices)
    vis = g.visual
    if isinstance(vis, trimesh.visual.ColorVisuals):
        return vis.vertex_colors[:, :3] / 255.0
    mat = getattr(vis, "material", None)
    uv = getattr(vis, "uv", None)
    img = None
    fac = np.ones(3)
    if mat is not None:
        img = getattr(mat, "baseColorTexture", None) or getattr(mat, "image", None)
        bf = getattr(mat, "baseColorFactor", None)
        if bf is not None:
            fac = np.asarray(bf[:3], float) / (255.0 if np.max(bf) > 1 else 1.0)
    if img is None or uv is None:
        return np.tile(fac, (n, 1))
    im = np.asarray(img.convert("RGB"), float) / 255.0
    h, w, _ = im.shape
    u = np.mod(uv[:, 0], 1.0) * (w - 1)
    v = (1 - np.mod(uv[:, 1], 1.0)) * (h - 1)
    # one texel: a wider tap bleeds the neighbouring atlas islands in along the seams
    xi = np.clip(np.round(u).astype(int), 0, w - 1)
    yi = np.clip(np.round(v).astype(int), 0, h - 1)
    return im[yi, xi] * fac


def weld(V, F, C):
    """Merge vertices that share a position (the model is split along its texture seams; decimating it unwelded tears
    the seams open into cracks). The colours of the merged copies are averaged."""
    q = np.round(V / (np.ptp(V, 0).max() * 1e-5)).astype(np.int64)
    _, first, inv = np.unique(q, axis=0, return_index=True, return_inverse=True)
    inv = inv.reshape(-1)
    V2 = V[first]
    C2 = np.zeros((len(first), 3)); n = np.zeros(len(first))
    np.add.at(C2, inv, C); np.add.at(n, inv, 1)
    C2 /= n[:, None]
    F2 = inv[F]
    F2 = F2[(F2[:, 0] != F2[:, 1]) & (F2[:, 1] != F2[:, 2]) & (F2[:, 0] != F2[:, 2])]
    return V2, F2, C2


def to_fish_frame(V, nose=None, flip_up=False):
    """Rotate so the length runs along Z with the nose at -Z and the back up +Y (glTF is Y up). Returns V, det."""
    V = V - (V.min(0) + V.max(0)) / 2
    ext = V.max(0) - V.min(0)
    # the length is the longer of X and Z (the generated models stand Y up)
    ax = 0 if ext[0] >= ext[2] else 2
    if nose is None:
        # the tail fin is the tall end: compare the height of the last and first 10% of the length
        t = V[:, ax]
        lo, hi = t.min(), t.max()
        L = hi - lo
        a = V[t < lo + .1 * L, 1]; b = V[t > hi - .1 * L, 1]
        ha = np.ptp(a) if len(a) else 0; hb = np.ptp(b) if len(b) else 0
        nose = ("+" if ha > hb else "-") + "xz"[ax // 2]   # the tall end is the tail, so the nose is the other one
    s = 1 if nose[0] == "+" else -1
    if nose[1] == "x":
        # nose along s*X -> -Z: z' = -s*x, x' = s*z (a turn about Y)
        R = np.array([[0, 0, s], [0, 1, 0], [-s, 0, 0]], float)
    else:
        # nose along s*Z -> -Z: z' = -s*z, x' = -s*x
        R = np.array([[-s, 0, 0], [0, 1, 0], [0, 0, -s]], float)
    if flip_up:
        R = np.array([[-1, 0, 0], [0, -1, 0], [0, 0, 1]], float) @ R
    V = V @ R.T
    L = V[:, 2].max() - V[:, 2].min()
    V = V / L
    V[:, 2] -= (V[:, 2].max() + V[:, 2].min()) / 2
    V[:, 1] -= (V[:, 1].max() + V[:, 1].min()) / 2
    V[:, 0] -= (V[:, 0].max() + V[:, 0].min()) / 2
    return V, nose


def decimate(V, F, C, target):
    if len(F) > target:
        V2, F2 = fast_simplification.simplify(V.astype(np.float32), F.astype(np.int32), target_reduction=1 - target / len(F))
    else:
        V2, F2 = V, F
    # colours: the nearest few source vertices, inverse-distance weighted
    d, i = cKDTree(V).query(V2, k=4)
    w = 1.0 / (d + 1e-5)
    C2 = (C[i] * w[..., None]).sum(1) / w.sum(1)[:, None]
    m = trimesh.Trimesh(V2, F2, process=True)
    # weld kept the vertex order; re-fetch colours for any merge
    if len(m.vertices) != len(V2):
        d, i = cKDTree(V2).query(m.vertices)
        C2 = C2[i]
    trimesh.repair.fix_normals(m)
    return np.asarray(m.vertices), np.asarray(m.faces), C2, np.asarray(m.vertex_normals)


def preview(lods, path):
    """Side and top views of the hi mesh, painted by vertex colour (for checking the axes and the colours)."""
    V, F, C, N = lods[-1]
    W, H = 1200, 700
    img = np.full((H, W, 3), 255, np.uint8)
    zbuf = np.full((H, W), -1e9)
    def draw(px, py, depth, oy, scale):
        for f in F:
            xs = px[f] * scale + W / 2; ys = -py[f] * scale + oy
            x0, x1 = int(max(0, xs.min())), int(min(W - 1, xs.max() + 1))
            y0, y1 = int(max(0, ys.min())), int(min(H - 1, ys.max() + 1))
            if x1 < x0 or y1 < y0: continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
            (ax, ay), (bx, by), (cx, cy) = zip(xs, ys)
            den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(den) < 1e-9: continue
            l1 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
            l2 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
            l3 = 1 - l1 - l2
            ins = (l1 >= 0) & (l2 >= 0) & (l3 >= 0)
            if not ins.any(): continue
            z = l1 * depth[f[0]] + l2 * depth[f[1]] + l3 * depth[f[2]]
            col = (l1[..., None] * C[f[0]] + l2[..., None] * C[f[1]] + l3[..., None] * C[f[2]])
            sub = zbuf[y0:y1 + 1, x0:x1 + 1]
            upd = ins & (z > sub)
            sub[upd] = z[upd]
            img[y0:y1 + 1, x0:x1 + 1][upd] = (np.clip(col[upd], 0, 1) * 255).astype(np.uint8)
    # side view from +X (nose left: screen x = z), top view from +Y
    draw(V[:, 2], V[:, 1], V[:, 0], 200, 1000)
    draw(V[:, 2], -V[:, 0], V[:, 1], 520, 1000)
    Image.fromarray(img).save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb"); ap.add_argument("key")
    ap.add_argument("--nose", choices=["+x", "-x", "+z", "-z"])
    ap.add_argument("--flip-up", action="store_true")
    ap.add_argument("--lo", type=int, default=2400)
    ap.add_argument("--hi", type=int, default=16000)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "KeysRun", "Fish"))
    ap.add_argument("--preview")
    a = ap.parse_args()
    V, F, C = load(a.glb)
    print(f"{a.glb}: {len(V)} verts, {len(F)} tris")
    V, F, C = weld(V, F, C)
    print(f"welded: {len(V)} verts")
    V, nose = to_fish_frame(V, a.nose, a.flip_up)
    print(f"nose was {nose}; size across {np.ptp(V[:,0]):.3f}, high {np.ptp(V[:,1]):.3f} (length 1)")
    lods = [decimate(V, F, C, a.lo), decimate(V, F, C, a.hi)]
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, a.key + ".krfish")
    with open(path, "wb") as f:
        f.write(b"KRF1"); f.write(struct.pack("<I", len(lods)))
        for Vv, Ff, Cc, Nn in lods:
            f.write(struct.pack("<II", len(Vv), len(Ff)))
            rgb = (np.clip(Cc, 0, 1) * 255 + .5).astype(np.uint8)
            rec = np.zeros(len(Vv), dtype=[("p", "<f4", 3), ("n", "<f4", 3), ("c", "u1", 4)])
            rec["p"] = Vv; rec["n"] = Nn; rec["c"][:, :3] = rgb; rec["c"][:, 3] = 255
            f.write(rec.tobytes()); f.write(Ff.astype("<u4").tobytes())
            print(f"  lod: {len(Vv)} verts, {len(Ff)} tris")
    print("wrote", path)
    if a.preview:
        preview(lods, a.preview)
        print("preview", a.preview)


if __name__ == "__main__":
    sys.exit(main())
