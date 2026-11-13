# girlglb.py raw/bikini_girl.glb dance-01.decoded.glb out.glb [preview.png]
#
# The deck crew / dancers at full detail. The web game ships bikini_girl (Sketchfab, doublesob, CC-BY-4.0) only as
# dance-01.glb: rigged to Mixamo's skeleton and the "Hip Hop Dancing" clip in Blender, then decimated from 49,860 to
# 11,964 triangles, which is what made her faceted. This puts the undecimated source mesh back under that same skin:
#
#  - the raw mesh, scaled into dance-01's mesh space (exactly 2x: the web build's quantization), keeps its own normals,
#    tangents and UVs (smooth shading, and its normal map lines up again);
#  - skin weights come across from dance-01: the simplifier only collapses edges onto existing vertices, so about a
#    third of the raw vertices sit exactly on a rigged one and take its weights; every other vertex takes them from the
#    nearest such vertex along the surface (Dijkstra over the welded mesh, so a hand resting on the hair never picks
#    up the hair's bones), then a few smoothing passes;
#  - the triangles are split by what the texture shows at them into three material sections: 0 skin, 1 hair,
#    2 swimsuit, so each can have its own shading (subsurface skin, sheeny hair, lycra) and its own colour per girl;
#  - the texture atlas's islands are grown out over its black ground, so no dark specks bleed in along the seams;
#  - the skeleton, inverse bind matrices and animation are dance-01's, unchanged.
import sys, os, math, heapq
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from glbio import read, acc, add, write
import png

rj, rb = read(sys.argv[1])
dj, db = read(sys.argv[2])
rp = rj['meshes'][0]['primitives'][0]
P = acc(rj, rb, rp['attributes']['POSITION'])
N = acc(rj, rb, rp['attributes']['NORMAL'])
T = acc(rj, rb, rp['attributes']['TANGENT'])
UV = acc(rj, rb, rp['attributes']['TEXCOORD_0'])
I = [i[0] for i in acc(rj, rb, rp['indices'])]
dp = dj['meshes'][0]['primitives'][0]
DP = acc(dj, db, dp['attributes']['POSITION'])
DJ = acc(dj, db, dp['attributes']['JOINTS_0'])
DW = acc(dj, db, dp['attributes']['WEIGHTS_0'])
Q = [(p[0] * 2, p[1] * 2, p[2] * 2) for p in P]
nv = len(Q); nt = len(I) // 3
print('raw', nv, 'verts', nt, 'tris; rigged', len(DP), 'verts')

# ---- weld by position, adjacency
wid = {}; W = [0] * nv; WP = []
for i, q in enumerate(Q):
    k = (round(q[0] * 1e5), round(q[1] * 1e5), round(q[2] * 1e5))
    if k not in wid: wid[k] = len(WP); WP.append(q)
    W[i] = wid[k]
nw = len(WP)
adj = [set() for _ in range(nw)]
for t in range(nt):
    a, b, c = W[I[3 * t]], W[I[3 * t + 1]], W[I[3 * t + 2]]
    adj[a].update((b, c)); adj[b].update((a, c)); adj[c].update((a, b))

# ---- sources: welded vertices on a rigged vertex
CELL = 0.01
grid = {}
for j, p in enumerate(DP): grid.setdefault((int(math.floor(p[0] / CELL)), int(math.floor(p[1] / CELL)), int(math.floor(p[2] / CELL))), []).append(j)
def wts(j):
    d = {}
    for k in range(4):
        if DW[j][k] > 0: d[DJ[j][k]] = d.get(DJ[j][k], 0) + DW[j][k]
    return d
TOL = 0.0015
WT = [None] * nw; dist = [1e9] * nw; heap = []
for w_, q in enumerate(WP):
    c = (int(math.floor(q[0] / CELL)), int(math.floor(q[1] / CELL)), int(math.floor(q[2] / CELL)))
    best = TOL * TOL; bj = -1
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                for j in grid.get((c[0] + dx, c[1] + dy, c[2] + dz), ()):
                    p = DP[j]; d = (p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2
                    if d < best: best = d; bj = j
    if bj >= 0:
        WT[w_] = wts(bj); dist[w_] = 0.0; heapq.heappush(heap, (0.0, w_))
src = [w is not None for w in WT]
print('sources', sum(src), 'of', nw)
# ---- nearest source along the surface
while heap:
    d, u = heapq.heappop(heap)
    if d > dist[u]: continue
    pu = WP[u]
    for v in adj[u]:
        pv = WP[v]; nd = d + math.sqrt((pu[0] - pv[0]) ** 2 + (pu[1] - pv[1]) ** 2 + (pu[2] - pv[2]) ** 2)
        if nd < dist[v]: dist[v] = nd; WT[v] = WT[u]; heapq.heappush(heap, (nd, v))
miss = 0
for w_ in range(nw):
    if WT[w_] is None:
        miss += 1  # an island with no rigged vertex: nearest rigged vertex in space
        q = WP[w_]; WT[w_] = wts(min(range(len(DP)), key=lambda j: sum((a - b) ** 2 for a, b in zip(DP[j], q))))
print('unreached', miss)
# ---- smooth the transferred (non-source) weights a little
for it in range(4):
    NWt = list(WT)
    for u in range(nw):
        if src[u] or not adj[u]: continue
        acc_ = dict(WT[u])
        for v in adj[u]:
            for k, x in WT[v].items(): acc_[k] = acc_.get(k, 0) + x
        NWt[u] = acc_
    WT = NWt
def top4(d):
    it = sorted(d.items(), key=lambda kv: -kv[1])[:4]
    s = sum(x for _, x in it) or 1
    j = [k for k, _ in it] + [0] * (4 - len(it)); w = [x / s for _, x in it] + [0.0] * (4 - len(it))
    return tuple(j), tuple(w)
J4 = []; W4 = []
for i in range(nv):
    j, w = top4(WT[W[i]]); J4.append(j); W4.append(w)

# ---- sections from the texture: skin, hair, swimsuit
img_i = rj['textures'][rj['materials'][0]['pbrMetallicRoughness']['baseColorTexture']['index']]['source']
img = rj['images'][img_i]
bv = rj['bufferViews'][img['bufferView']]; o = bv.get('byteOffset', 0)
tmp = sys.argv[3] + '.base.png'; open(tmp, 'wb').write(rb[o:o + bv['byteLength']])
tw, th, tn, tex = png.read(tmp)
# The atlas is packed tight on a black ground: sampled at an island's edge (bilinear, mips) the black bleeds in as dark
# specks along every UV seam. Grow the islands out over the black (a breadth-first fill from their edges).
from collections import deque
tex = bytearray(tex)
empty = bytearray(1 if max(tex[k * tn:k * tn + 3]) <= 20 else 0 for k in range(tw * th))
q = deque(k for k in range(tw * th) if not empty[k] and (
    (k % tw and empty[k - 1]) or (k % tw < tw - 1 and empty[k + 1]) or (k >= tw and empty[k - tw]) or (k < tw * (th - 1) and empty[k + tw])))
while q:
    k = q.popleft(); x = k % tw
    for j in ((k - 1) if x else -1, (k + 1) if x < tw - 1 else -1, k - tw, k + tw):
        if 0 <= j < tw * th and empty[j]:
            empty[j] = 0; tex[j * tn:j * tn + tn] = tex[k * tn:k * tn + tn]; q.append(j)
png.write(tmp, tw, th, tn, tex)
dil = open(tmp, 'rb').read()
def col(u, v):
    x = min(tw - 1, max(0, int((u % 1.0) * tw))); y = min(th - 1, max(0, int((v % 1.0) * th)))
    k = (y * tw + x) * tn
    return tex[k] / 255.0, tex[k + 1] / 255.0, tex[k + 2] / 255.0
def is_skin(c):
    r, g, b = c
    return r > 0.42 and r > g > b and (r - b) > 0.13 and (r - b) < 0.6 * r + 0.08
ys = [q[1] for q in Q]; y0 = min(ys); y1 = max(ys); H = y1 - y0
sec = []
cnt = [0, 0, 0]
for t in range(nt):
    vs = I[3 * t:3 * t + 3]
    us = [UV[v] for v in vs]
    smp = [col(*u) for u in us] + [col(sum(u[0] for u in us) / 3, sum(u[1] for u in us) / 3)]
    nsk = sum(1 for c in smp if is_skin(c))
    h = (sum(Q[v][1] for v in vs) / 3 - y0) / H   # 0 feet .. 1 top of the raised hands
    if nsk >= 2: s = 0
    else:
        lum = sum(0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2] for c in smp) / 4
        # the swimsuit spans the hips to the chest (ties and shadows in it included); the hair is everything not skin
        # above the shoulders; below the swimsuit, anything not skin (shadows, toenails) stays skin
        s = 1 if h >= 0.81 else (2 if h > 0.48 else 0)
    sec.append(s); cnt[s] += 1
print('sections skin/hair/suit', cnt)

if len(sys.argv) > 4:
    # a front and a side view of the sections, for checking
    S = 512; out = bytearray(S * 2 * S * 3)
    xs = [q[0] for q in Q]; zs = [q[2] for q in Q]
    pal = [(240, 190, 160), (60, 40, 30), (40, 120, 240)]
    order = sorted(range(nt), key=lambda t: sum(Q[v][2] for v in I[3 * t:3 * t + 3]))
    for view in (0, 1):
        tris = order if view == 0 else sorted(range(nt), key=lambda t: sum(Q[v][0] for v in I[3 * t:3 * t + 3]))
        for t in tris:
            vs = I[3 * t:3 * t + 3]
            px = [((Q[v][0] if view == 0 else -Q[v][2]) / H + 0.5) * S + view * S for v in vs]
            py = [(1 - (Q[v][1] - y0) / H) * (S - 1) for v in vs]
            nz = abs(sum(N[v][2 if view == 0 else 0] for v in vs)) / 3
            cc = pal[sec[t]]; sh = 0.35 + 0.65 * nz
            for yy in range(int(min(py)), int(max(py)) + 1):
                for xx in range(int(min(px)), int(max(px)) + 1):
                    if 0 <= xx < 2 * S and 0 <= yy < S:
                        k = (yy * 2 * S + xx) * 3; out[k] = int(cc[0] * sh); out[k + 1] = int(cc[1] * sh); out[k + 2] = int(cc[2] * sh)
    png.write(sys.argv[4], 2 * S, S, 3, out)

# ---- write: dance-01's skeleton and clip, the raw mesh in three sections, the raw textures
js, bn = dj, db
pa = add(js, bn, Q, 5126, 'VEC3', 34962, minmax=True)
na = add(js, bn, N, 5126, 'VEC3', 34962)
ta = add(js, bn, T, 5126, 'VEC4', 34962)
ua = add(js, bn, UV, 5126, 'VEC2', 34962)
ja = add(js, bn, J4, 5123, 'VEC4', 34962)
wa = add(js, bn, W4, 5126, 'VEC4', 34962)
texs = []
for ii in range(len(rj['images'])):
    im = rj['images'][ii]; b_ = rj['bufferViews'][im['bufferView']]; o = b_.get('byteOffset', 0)
    data = dil if ii == img_i else rb[o:o + b_['byteLength']]
    while len(bn) % 4: bn.append(0)
    off = len(bn); bn.extend(data)
    js['bufferViews'].append({'buffer': 0, 'byteOffset': off, 'byteLength': len(data)})
    texs.append({'bufferView': len(js['bufferViews']) - 1, 'mimeType': 'image/png', 'name': 'GirlBase' if ii == img_i else 'GirlNormal'})
js['images'] = texs
js['textures'] = [{'source': img_i}, {'source': 1 - img_i}]
js.pop('samplers', None)
names = ['Girl_Skin', 'Girl_Hair', 'Girl_Suit']
js['materials'] = [{'name': n, 'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'metallicFactor': 0},
                    'normalTexture': {'index': 1}} for n in names]
prims = []
for s in range(3):
    ix = [I[3 * t + k] for t in range(nt) if sec[t] == s for k in range(3)]
    ia = add(js, bn, [(i,) for i in ix], 5125, 'SCALAR', 34963)
    prims.append({'attributes': {'POSITION': pa, 'NORMAL': na, 'TANGENT': ta, 'TEXCOORD_0': ua, 'JOINTS_0': ja,
                                 'WEIGHTS_0': wa}, 'indices': ia, 'material': s})
js['meshes'] = [{'name': 'Girl', 'primitives': prims}]
write(sys.argv[3], js, bn)
os.remove(tmp)
print('wrote', sys.argv[3])
