"""Turn the generated commercial fishing boat (an unpainted GLB) into the working fleet's hulls:
Content/KeysRun/Props/work_<n>.krfish, one per boat in KeysLocals' fleet, each in her own colours.

    python Tools/boatglb.py <in.glb> [--tris 40000] [--preview out.png]

The model came without paint (its texture is flat grey), so it is painted here from the geometry: antifouling below the
waterline, a boot stripe, topsides in the boat's hull colour, decks a warm grey, the house white with a dark band of
wheelhouse glass, and anything thin (outriggers, masts, rails, antennas) steel. A part counts as thin where a vertex
with the opposite normal sits within a few centimetres of it.

Axes: the file is in the frame LoadProp reads (x forward to the bow, y up, z to port), unit length bow to stern, base
at y = 0, centred on its length and beam. The game scales it by the boat's length.
"""
import argparse, os, struct, sys
import numpy as np
import trimesh
import fast_simplification
from scipy.spatial import cKDTree

# name, hull, boot stripe, bottom paint, house accent (the band under the wheelhouse glass and the roof edge)
SCHEMES = [
    ("hot_tuna",  0xe9e6dc, 0x1f4f8c, 0x6e2418, 0x1f4f8c),
    ("dog_pound", 0x2c5a3a, 0xe9e6dc, 0x1c1c1c, 0x2c5a3a),
    ("pin_wheel", 0xe3dccb, 0x7a2c1c, 0x7a2c1c, 0x7a2c1c),
    ("no_limits", 0x1f2f4a, 0xc8a24a, 0x6e2418, 0xc8a24a),
    ("fat_tuna",  0xd8d2c2, 0x2a3d52, 0x1c1c1c, 0x2a3d52),
]

WATERLINE = .062   # above the keel, in boat lengths (about 0.9 m on a 14 m boat)
BOOT = .016


def rgb(h):
    return np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], float) / 255


def load(path, tris):
    sc = trimesh.load(path, force="scene")
    V, F = [], []
    off = 0
    for name, g in sc.geometry.items():
        for node in sc.graph.geometry_nodes.get(name, [name]):
            T, _ = sc.graph[node]
            gg = g.copy(); gg.apply_transform(T)
            V.append(np.asarray(gg.vertices)); F.append(np.asarray(gg.faces) + off); off += len(gg.vertices)
    V = np.vstack(V); F = np.vstack(F)
    q = np.round(V / (np.ptp(V, 0).max() * 1e-5)).astype(np.int64)
    _, first, inv = np.unique(q, axis=0, return_index=True, return_inverse=True)
    inv = inv.reshape(-1)
    V = V[first]; F = inv[F]
    F = F[(F[:, 0] != F[:, 1]) & (F[:, 1] != F[:, 2]) & (F[:, 0] != F[:, 2])]
    if len(F) > tris:
        V, F = fast_simplification.simplify(V.astype(np.float32), F.astype(np.int32), target_reduction=1 - tris / len(F))
    m = trimesh.Trimesh(V, F, process=True)
    # finer triangles along the waterline, so the bottom paint and the boot stripe come out as straight lines
    for _ in range(2):
        Vn = np.asarray(m.vertices); lo = Vn[:, 1].min(); L = np.ptp(Vn[:, 2])
        fy = (Vn[np.asarray(m.faces)][:, :, 1] - lo) / L
        near = np.where((fy.min(1) < WATERLINE + BOOT + .006) & (fy.max(1) > WATERLINE - .006))[0]
        Vs, Fs = trimesh.remesh.subdivide(m.vertices, m.faces, face_index=near)
        m = trimesh.Trimesh(Vs, Fs, process=True)
    trimesh.repair.fix_normals(m)
    return m


def frame(m):
    """glTF (x across, y up, bow toward -z) -> unit length, keel at 0, and the order LoadProp reads: bow +x, up y, z."""
    V = np.asarray(m.vertices, float).copy()
    lo, hi = V.min(0), V.max(0)
    L = hi[2] - lo[2]
    V = (V - np.array([(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2])) / L
    return V


def regions(V, N, F):
    """Per vertex: 0 bottom, 1 boot, 2 topsides, 3 deck, 4 house, 5 glass, 6 steel, 7 house accent."""
    x, y, z = V[:, 0], V[:, 1], V[:, 2]          # still glTF order here: x across, y up, z (bow -) .. (stern +)
    ny = N[:, 1]
    # thin parts: an opposite-facing vertex close by
    tree = cKDTree(V)
    thin = np.zeros(len(V), bool)
    for i, js in enumerate(tree.query_ball_point(V, .006)):
        if len(js) > 1:
            thin[i] = ((N[js] @ N[i]) < -.4).any()
    R = np.full(len(V), 4)
    # the sheer (the bulwark top): high at the bow, down to the waist, then nearly level to the transom
    sheer = np.interp(z, [-.5, -.15, .5], [.188, .127, .112])
    hull = y < sheer
    # the hull's outer skin: within a few cm of the widest point of its (length, height) slice
    zb = np.clip(((z + .5) / .02).astype(int), 0, 50); yb = np.clip((y / .01).astype(int), 0, 70)
    beam = np.zeros((51, 71))
    np.maximum.at(beam, (zb[~thin], yb[~thin]), np.abs(x[~thin]))
    out = (np.abs(x) > beam[zb, yb] - .018) | (ny < -.3) | (z > .485) | (z < -.485)
    R[hull] = 3
    R[hull & out] = 2
    R[hull & out & (y < WATERLINE + BOOT)] = 1
    R[hull & out & (y < WATERLINE)] = 0
    # the house: a band of glass on its upright faces, an accent stripe under it
    up = ~hull & ~thin
    upright = np.abs(ny) < .35
    zc = (z > -.32) & (z < .16)
    R[up & upright & zc & (y > .178) & (y < .228)] = 5
    R[up & upright & zc & (y > .232) & (y < .268) & (z < -.1)] = 5    # the flybridge screen
    R[thin & (R >= 3)] = 6
    # tidy the speckle: each vertex takes the commonest label among itself and its neighbours (twice)
    E = np.vstack([F[:, [0, 1]], F[:, [1, 2]], F[:, [2, 0]]]); E = np.vstack([E, E[:, ::-1]])
    for _ in range(2):
        cnt = np.zeros((len(V), 8)); np.add.at(cnt, (E[:, 0], R[E[:, 1]]), 1); np.add.at(cnt, (np.arange(len(V)), R), 1.5)
        R2 = cnt.argmax(1)
        keep = (R == 6) | (R2 == 6)          # leave the thin parts alone
        R = np.where(keep, R, R2)
    return R


def paint(R, scheme):
    _, hull, boot, bottom, accent = scheme
    pal = {0: rgb(bottom), 1: rgb(boot), 2: rgb(hull), 3: rgb(0xb9b4a6), 4: rgb(0xf1efe8), 5: rgb(0x1b2630),
           6: rgb(0x9aa1a8), 7: rgb(accent)}
    return np.array([pal[r] for r in R])


def write(path, V, F, C, N):
    with open(path, "wb") as f:
        f.write(b"KRF1"); f.write(struct.pack("<I", 1))
        f.write(struct.pack("<II", len(V), len(F)))
        rec = np.zeros(len(V), dtype=[("p", "<f4", 3), ("n", "<f4", 3), ("c", "u1", 4)])
        rec["p"] = V; rec["n"] = N
        rec["c"][:, :3] = (np.clip(C, 0, 1) * 255 + .5).astype(np.uint8); rec["c"][:, 3] = 255
        f.write(rec.tobytes()); f.write(F.astype("<u4").tobytes())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb")
    ap.add_argument("--tris", type=int, default=24000)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "KeysRun", "Props"))
    ap.add_argument("--preview")
    a = ap.parse_args()
    m = load(a.glb, a.tris)
    V = frame(m)
    N = np.asarray(m.vertex_normals, float)
    R = regions(V, N, np.asarray(m.faces))
    print(f"{len(V)} verts, {len(m.faces)} tris; beam {np.ptp(V[:,0]):.3f}, height {np.ptp(V[:,1]):.3f} (length 1)")
    print("region counts", np.bincount(R, minlength=8))
    # glTF (x across, y up, z bow-) -> file (x bow+, y up, z = x across): a turn about y
    P = np.stack([-V[:, 2], V[:, 1], V[:, 0]], 1)
    Nf = np.stack([-N[:, 2], N[:, 1], N[:, 0]], 1)
    F = np.asarray(m.faces)
    os.makedirs(a.out, exist_ok=True)
    cols = []
    for i, s in enumerate(SCHEMES):
        C = paint(R, s); cols.append(C)
        path = os.path.join(a.out, f"work_{i}.krfish")
        write(path, P, F, C, Nf)
        print("wrote", path, s[0])
    if a.preview:
        preview(V, F, N, cols, a.preview)


def preview(V, F, N, cols, path):
    from PIL import Image, ImageDraw
    rows = []
    for C in cols:
        row = Image.new("RGB", (1200, 400), (225, 232, 238))
        for k, (yaw, pitch) in enumerate([(2.3, .3), (np.pi / 2, 0)]):
            cy, sy = np.cos(yaw), np.sin(yaw)
            R1 = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
            cp, sp = np.cos(pitch), np.sin(pitch)
            R2 = np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]])
            W = R2 @ R1
            P = (V - V.mean(0)) @ W.T; Nn = N @ W.T
            im = Image.new("RGB", (600, 400), (225, 232, 238)); d = ImageDraw.Draw(im)
            sh = .45 + .55 * np.clip(Nn @ np.array([.35, .7, .62]), 0, 1)
            fc = C[F].mean(1) * sh[F].mean(1)[:, None]
            for i in np.argsort(P[F].mean(1)[:, 2]):
                f = F[i]
                d.polygon([(300 + P[j, 0] * 520, 200 - P[j, 1] * 520) for j in f], fill=tuple((np.clip(fc[i], 0, 1) * 255).astype(int)))
            row.paste(im, (600 * k, 0))
        rows.append(row)
    out = Image.new("RGB", (1200, 400 * len(rows)))
    for i, r in enumerate(rows): out.paste(r, (0, 400 * i))
    out.save(path)
    print("preview", path)


if __name__ == "__main__":
    sys.exit(main())
