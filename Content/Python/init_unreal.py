# Runs every time the editor opens this project. The first time, it creates the materials
# and the empty map the C++ game needs, so nothing has to be authored by hand.
import unreal

ROOT = "/Game/KeysRun"
MAT_DIR = ROOT + "/Materials"
MAP_PATH = ROOT + "/Maps/Keys"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def _link(a, ao, b, bi):
    ok = mel.connect_material_expressions(a, ao, b, bi)
    if not ok:
        unreal.log_error("Keys Run: could not connect %s.%s -> %s.%s" % (a.get_name(), ao, b.get_name(), bi))
    return ok


def _const(mat, value, x, y):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", value)
    return c


PLAIN = {"M_KR_Cloud"}   # volume-domain materials: no mesh usage flags


def _make_material(name, build):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        return False
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if name not in PLAIN:
        mat.set_editor_property("two_sided", True)
        mat.set_editor_property("used_with_instanced_static_meshes", True)
    build(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log("Keys Run: created " + path)
    return True




def _water(mat):
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -500, 0)
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_const(mat, 0.05, -500, 220), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_const(mat, 0.5, -500, 320), "", unreal.MaterialProperty.MP_SPECULAR)


# The detailed sea. The C++ water mesh supplies the big swells and wakes, the foam amount in vertex alpha, and in UV0:
# x = crest height (for light glowing through thin crests), y = aerated prop wash. This material adds what the mesh is
# too coarse for: wind ripples that catch the sun, foam broken into real-looking patches, and (as Single Layer Water)
# the see-through water itself, with the light scattering brighter through the wave tops.
OCEAN_FOAM = """
float2 p = WP.xy / 100.0;
float cover = saturate(A);
// whitecaps from the simulated sea, sharp to the pixel (the mesh carries them too, but only at its vertices)
if (On > 0.5) {
  float sf = max(Sim0.SampleLevel(Sim0Sampler, p / L0 + 0.5 / 64.0, 0).b, 0.55 * Sim1.SampleLevel(Sim1Sampler, p / L1 + 0.5 / 64.0, 0).b);
  cover = max(cover, sf * (0.3 + 0.7 * saturate(Caps)) * saturate(1.0 - D / 300000.0));
}
// in the prop wash the foam is dragged out into streaks along the boat's track (TD, radians in the sea's own x/y)
float2 td = float2(cos(TD), sin(TD));
float2 tp = float2(dot(p, td) * 0.3, dot(p, float2(-td.y, td.x)));
p = lerp(p, tp, saturate(Aer * 1.6));
// two layers of value noise drifting at different speeds: bubbly cells plus larger streaks
float n = 0.0, amp = 0.55;
float2 q = p * float2(1.25, 0.95) + float2(T * 0.035, T * 0.021);
for (int i = 0; i < 5; i++) {
  float2 ip = floor(q), fp = frac(q);
  float2 u = fp * fp * (3.0 - 2.0 * fp);
  float3 p3a = frac(float3(ip.xyx) * .1031); p3a += dot(p3a, p3a.yzx + 33.33); float h00 = frac((p3a.x + p3a.y) * p3a.z);
  float2 i10 = ip + float2(1, 0); float3 p3b = frac(float3(i10.xyx) * .1031); p3b += dot(p3b, p3b.yzx + 33.33); float h10 = frac((p3b.x + p3b.y) * p3b.z);
  float2 i01 = ip + float2(0, 1); float3 p3c = frac(float3(i01.xyx) * .1031); p3c += dot(p3c, p3c.yzx + 33.33); float h01 = frac((p3c.x + p3c.y) * p3c.z);
  float2 i11 = ip + float2(1, 1); float3 p3d = frac(float3(i11.xyx) * .1031); p3d += dot(p3d, p3d.yzx + 33.33); float h11 = frac((p3d.x + p3d.y) * p3d.z);
  n += amp * lerp(lerp(h00, h10, u.x), lerp(h01, h11, u.x), u.y);
  q = q * 2.07 + float2(17.3, 9.1) + float2(T * 0.05, -T * 0.03) * (i + 1);
  amp *= 0.5;
}
n = saturate(n / 1.05);
// more cover = the threshold drops, so foam grows from the brightest noise cells outward.
// Even full cover leaves gaps of green water, like real churned-up wash.
float th = 0.92 - cover * 0.55;
float m = smoothstep(th - 0.06, th + 0.16, n) * saturate(cover * 3.0);   // soft edges, not a cut-out
// real wash is lace, not paint: a web of bubbles around holes of dark water, at two sizes. Cellular noise: the
// distance to the nearest of a jittered grid of points; foam lies along the cell walls, the cells are the holes.
float lace = 0.0;
for (int s2 = 0; s2 < 2; s2++) {
  float sc = s2 == 0 ? 1.6 : 4.3;
  float2 lq = p * sc + float2(T * (0.06 + 0.05 * s2), -T * 0.04) + s2 * 11.7;
  float2 li = floor(lq), lf = frac(lq);
  float f1 = 9.0, f2 = 9.0;
  for (int yy = -1; yy <= 1; yy++)
    for (int xx = -1; xx <= 1; xx++) {
      float2 c = li + float2(xx, yy);
      float3 h3 = frac(float3(c.xyx) * float3(.1031, .1030, .0973)); h3 += dot(h3, h3.yzx + 33.33);
      float2 o = frac((h3.xx + h3.yz) * h3.zy);
      o = 0.5 + 0.38 * sin(T * (0.4 + o * 0.6) + o * 6.2831853);   // the bubbles drift and pop
      float d = length(float2(xx, yy) + o - lf);
      if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) { f2 = d; }
    }
  float wall = 1.0 - smoothstep(0.02, 0.22, f2 - f1);   // near a wall between two cells
  lace += (s2 == 0 ? 0.6 : 0.4) * wall;
}
// fresh, thick foam (high cover) is nearly solid; thin, old foam is mostly lace with water showing through
float thick = smoothstep(0.55, 1.0, cover) * smoothstep(th + 0.1, th + 0.35, n);
m *= lerp(0.25 + 0.75 * lace, 0.85 + 0.15 * lace, thick);
// the prop wash itself (heavy cover over aerated water): a solid white column, its texture in the bubbles, not holes
// ragged edges (the cover threshold wanders with the noise) and a churned brightness, not a flat white road
float wash = smoothstep(0.35, 0.75, Aer) * smoothstep(0.42 + 0.45 * (n - 0.5), 0.88 + 0.2 * (n - 0.5), cover);
m = max(m, wash * (0.66 + 0.24 * n + 0.1 * lace));
m = min(m, lerp(0.92, 0.97, wash));
// a windy day: long streaks of old foam lying down the wind, and scattered breaking whitecaps between the crests
float2 wd = float2(cos(WD), sin(WD));
float2 sq = float2(dot(p, wd) * 0.045, dot(p, float2(-wd.y, wd.x)) * 0.6) + float2(-T * 0.08, 0.0);
float2 si = floor(sq), sf = frac(sq); float2 su = sf * sf * (3.0 - 2.0 * sf);
float3 s3a = frac(float3(si.xyx) * .1031); s3a += dot(s3a, s3a.yzx + 33.33); float sa = frac((s3a.x + s3a.y) * s3a.z);
float3 s3b = frac(float3((si + float2(1, 0)).xyx) * .1031); s3b += dot(s3b, s3b.yzx + 33.33); float sb = frac((s3b.x + s3b.y) * s3b.z);
float3 s3c = frac(float3((si + float2(0, 1)).xyx) * .1031); s3c += dot(s3c, s3c.yzx + 33.33); float sc = frac((s3c.x + s3c.y) * s3c.z);
float3 s3d = frac(float3((si + float2(1, 1)).xyx) * .1031); s3d += dot(s3d, s3d.yzx + 33.33); float sd = frac((s3d.x + s3d.y) * s3d.z);
float streak = lerp(lerp(sa, sb, su.x), lerp(sc, sd, su.x), su.y);
float caps = saturate(Caps);
m = max(m, smoothstep(0.78, 0.95, streak) * smoothstep(0.55, 0.85, n) * caps * 0.55);
// rain beats the sea down to a matte, faintly frothy grey-white
m = max(m, smoothstep(0.75, 0.95, n) * saturate(Rain) * 0.18);
// thin lacy edge around each patch
m = max(m, 0.35 * smoothstep(th - 0.08, th, n) * (1.0 - smoothstep(th, th + 0.04, n)) * saturate(cover * 4.0));
// far away the noise would shimmer, so fade to the average
float far = saturate((D - 8000.0) / 40000.0);
return lerp(m, max(cover * 0.75, caps * 0.06), far);
"""

OCEAN_NORMAL = """
float2 p = WP.xy / 100.0;
float2 g = float2(0, 0);
// ripples blown by an easterly trade wind: 18 wave trains from 0.2 m to about 15 m long, spread unevenly
// around the wind so they never line up into a visible grid
float wind = WD;
float foot = max(D, 1.0) / 100.0 * 0.0012;   // meters per pixel at this distance
// with the simulated sea on (KeysOcean, On = 1) the 1-8 m chop and, out past where the mesh can show them, the 8-130 m
// waves come from its slope textures (R,G = slope, B = foam, A = height; one texel per grid point); only the tiny
// sub-metre ripples stay procedural
int nTrains = On > 0.5 ? 11 : 18;   // (keep the ripples up to ~2.5 m: they overlap the small tile, which gives sparkle)
float2 gs = float2(0, 0);
if (On > 0.5) {
  float kChop = 0.35 + 0.65 * saturate(Deep);   // KeysGeo ChopAmp: the wind sea is smaller over the flats
  float2 uv1 = p / L1 + 0.5 / 64.0, uv0 = p / L0 + 0.5 / 64.0;
  float aa1 = saturate(1.6 / (foot * 14.0) - 1.0);          // the shortest chop needs pixels to show
  float far0 = saturate((D / 100.0 - 120.0) / 160.0);        // the mesh carries the big tile near the camera
  // the chop's slopes are real-world gentle; on a screen they read as glassy, so they are drawn steeper (like the
  // classic ripples were), the big tile less so
  gs += Sim1.SampleLevel(Sim1Sampler, uv1, 0).rg * (kChop * aa1 * 2.6);
  gs += Sim0.SampleLevel(Sim0Sampler, uv0, 0).rg * (kChop * far0 * 1.6);
}
for (int i = 0; i < nTrains; i++) {
  float fi = (float)i;
  float r1 = frac(sin(fi * 12.9898) * 43758.5453);
  float r2 = frac(sin(fi * 78.233) * 12543.123);
  float ang = wind + (r1 * 2.0 - 1.0) * 1.35;
  float2 dir = float2(cos(ang), sin(ang));
  float L = 0.2 * pow(1.29, fi) * (0.85 + 0.3 * r2);
  float k = 6.2831853 / L;
  float w = sqrt(9.81 * k);
  float steep = 0.055 * (0.6 + 0.8 * r2);
  // drop waves too small to draw at this distance: a wave needs a dozen pixels or it only makes single-pixel glints
  // (and more again once the upscaler has stretched them); the slope it would have had goes to roughness instead
  float aa = saturate(L / (foot * 14.0) - 1.0);
  float ph = k * dot(dir, p) - w * T + r1 * 6.2831853 + r2 * 3.1;
  g += dir * (steep * cos(ph)) * aa;
}
float s = Strength * (1.0 - 0.75 * saturate(F)) * (1.0 - 0.5 * saturate(Aer));
g *= s;
g += gs * (1.0 - 0.75 * saturate(F)) * (1.0 - 0.5 * saturate(Aer));   // (true slopes: not scaled by the ripple strength)
// rain: every drop leaves a little ring spreading out and fading; two staggered layers of drop cells
float rain = saturate(Rain);
if (rain > 0.01 && foot < 0.04) {
  float near = 1.0 - saturate(foot / 0.04);
  for (int l = 0; l < 2; l++) {
    float2 rp = p * 2.6 + float2(l * 0.37, l * 0.71);
    float2 ci = floor(rp), cf = frac(rp);
    float3 h3 = frac(float3(ci.xyx + l * 17.0) * float3(.1031, .1030, .0973)); h3 += dot(h3, h3.yzx + 33.33);
    float3 hh = frac((h3.xxy + h3.yzz) * h3.zyx);
    if (hh.z > rain * 1.1) continue;
    float age = frac(T * 1.1 + hh.x * 7.0 + l * 0.5);
    float2 dv = cf - (0.25 + 0.5 * hh.xy);
    float d = length(dv);
    float r = age * 0.42;
    float w = (d - r) * 28.0;
    float ring = sin(w * 3.1416) * exp(-w * w * 0.5) * (1.0 - age) * (1.0 - age);
    g += dv / max(d, 1e-3) * ring * 0.55 * near;
  }
}
return normalize(VN + float3(-g.x, -g.y, 0.0));
"""

OCEAN_GLINT = """
// sun glitter: the sun's reflection off the facets of the sea. Up close it breaks into points that twinkle as the
// facets tilt through the angle (cells of the water that catch it for a moment); farther out they blur into the
// broad bright path toward the sun. Soft on purpose: a hard one shimmers as single pixels.
float3 L = normalize(SunDir);
float3 V = normalize(CamV);
float3 R = reflect(-V, normalize(N));
float s = saturate(dot(R, L));
float far = saturate(D / 30000.0);
float e = lerp(700.0, 90.0, far);
float spec = pow(s, e);
float2 cp = WP.xy / lerp(35.0, 400.0, far);
float2 ci = floor(cp);
float h = frac(sin(dot(ci, float2(127.1, 311.7))) * 43758.5453);
float tw = smoothstep(0.35, 1.0, 0.5 + 0.5 * sin(T * (2.0 + 3.0 * h) + h * 40.0));
float spark = lerp(0.25 + 2.2 * tw, 1.0, far);
float day = saturate(L.z * 5.0);
return float3(1.0, 0.94, 0.82) * (spec * spark * lerp(1.6, 0.7, far) * day * (1.0 - saturate(F * 1.5)) * Glint);
"""

OCEAN_ROUGH = """
// glassy up close, rougher with distance as the ripples there are too small to draw but still blur the reflection
// (a mirror-smooth sea throws the sun back as single-pixel glints that shimmer and read as pixelated); rain and a
// hard blow roughen the whole surface into a duller sheen
float r = lerp(0.06, 0.12, saturate(D / 4000.0));
r = lerp(r, 0.2, saturate((D - 4000.0) / 60000.0));
r = lerp(r, 0.22, saturate(Rain) * 0.7);
r = lerp(r, 0.1, saturate(Caps) * 0.5);
return lerp(r, 0.6, saturate(F));
"""

def _custom(mat, x, y, code, out_type, names, desc):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out_type)
    c.set_editor_property("description", desc)
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c


# The everyday surface: vertex color for the paint, UV1.x for what kind of surface it is (KeysMesh.h EKeysSurf).
# Each kind gets its own roughness / metalness and a procedural detail normal from 3D noise in the mesh's own space,
# so nothing needs textures or UVs and moving things (boats, people) carry their detail with them: weave on cloth,
# pores on skin, strands in hair, orange-peel gelcoat, non-skid diamonds, brushed steel, wood grain, stucco, corrugated
# tin, lap siding, shingles, asphalt speckle, sand ripples, bark rings, fish scales with a rainbow sheen. Fine detail
# fades out with distance so it never shimmers.
KRF_NOISE = """
struct KRF {
  float hash(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
  float4 nd(float3 x) {
    float3 i = floor(x); float3 f = frac(x);
    float3 u = f * f * (3.0 - 2.0 * f); float3 du = 6.0 * f * (1.0 - f);
    float a = hash(i), b = hash(i + float3(1, 0, 0)), c = hash(i + float3(0, 1, 0)), d = hash(i + float3(1, 1, 0));
    float e = hash(i + float3(0, 0, 1)), f1 = hash(i + float3(1, 0, 1)), g = hash(i + float3(0, 1, 1)), h = hash(i + float3(1, 1, 1));
    float k1 = b - a, k2 = c - a, k3 = e - a, k4 = a - b - c + d, k5 = a - c - e + g, k6 = a - b - e + f1, k7 = -a + b + c - d + e - f1 - g + h;
    float v = a + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z;
    float3 dv = du * float3(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z, k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x, k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y);
    return float4(v, dv);
  }
  float4 fbm(float3 x, int oct) {
    float4 s = 0; float a = 0.5; float fr = 1.0;
    for (int i = 0; i < oct; i++) { float4 n = nd(x * fr + i * 7.31); s += float4(n.x * a, n.yzw * a * fr); fr *= 2.03; a *= 0.5; }
    return s;
  }
  // fade for detail of F cycles per meter: 1 while a cell spans 5+ pixels, 0 by 1.5 (finer than that it only aliases
  // into speckle and sparkle, worst on a moving boat under TSR)
  float aa(float f, float fw) { return smoothstep(1.5, 5.0, 1.0 / max(f * fw, 1e-6)); }
};
"""

SURFACE_FN = KRF_NOISE + """
KRF F;
float kind = floor(K.x + 0.5);
float3 p = LP / 100.0;
// meters per pixel here, from the screen-space derivatives of the position (before the wrap below)
#if PIXELSHADER
float fw = max(length(ddx(p)), length(ddy(p)));
#else
float fw = max(D, 1.0) / 100.0 * 0.0012;
#endif
p = p - floor(p / 256.0) * 256.0;
float3 ax = normalize(AX), ay = normalize(AY), az = normalize(AZ);
float3 N0 = normalize(VN) * TS;
float3 ln = normalize(float3(dot(N0, ax), dot(N0, ay), dot(N0, az)));
float dist = D / 100.0;
float nearF = 1.0 - saturate((dist - 6.0) / 30.0);
float closeF = 1.0 - saturate((dist - 1.5) / 5.0);
float midF = 1.0 - saturate((dist - 30.0) / 170.0);
float ndv = saturate(dot(N0, normalize(CV)));
float fres = pow(1.0 - ndv, 3.0);
float3 col = VC.rgb;
float df = 1.0;
float rough = 0.7; float metal = 0.0; float spec = 0.5;
float3 g = 0;
float3 aa = abs(ln);
// the two axes across the face's dominant direction (a cheap planar projection for woven / tiled patterns)
float2 q = (aa.z >= aa.x && aa.z >= aa.y) ? p.xy : (aa.x >= aa.y ? p.yz : p.xz);
float3 qa = (aa.z >= aa.x && aa.z >= aa.y) ? float3(1, 0, 0) : (aa.x >= aa.y ? float3(0, 1, 0) : float3(1, 0, 0));
float3 qb = (aa.z >= aa.x && aa.z >= aa.y) ? float3(0, 1, 0) : float3(0, 0, 1);
if (kind < 0.5) {
  float4 m = F.nd(p * 1.3);
  float4 b = F.fbm(p * 7.0, 2);
  df = lerp(0.94, 1.05, m.x) * lerp(0.97, 1.02, b.x);
  g = b.yzw * 0.035 * F.aa(14.0, fw);
  rough = 0.64 + 0.12 * m.x;
} else if (kind < 1.5) {           // skin
  float4 m = F.nd(p * 22.0);
  float4 pr = F.nd(p * 240.0);
  df = lerp(0.95, 1.04, m.x);
  float apr = F.aa(240.0, fw);
  g = pr.yzw * 0.015 * apr + m.yzw * 0.012 * F.aa(22.0, fw);
  rough = 0.46 - 0.08 * m.x + 0.06 * pr.x * apr; spec = 0.4;
  col += fres * 0.18 * float3(1.0, 0.5, 0.4) * VC.rgb;
} else if (kind < 2.5) {           // cloth: a fine weave and soft folds, with a velvety rim
  float wv = 6.2831853 * 140.0;
  float s1 = sin(q.x * wv), s2 = sin(q.y * wv);
  float4 fo = F.fbm(p * 5.0, 2);
  float awv = F.aa(140.0, fw);
  df = lerp(1.0, 0.95 + 0.05 * (0.5 + 0.5 * s1 * s2), awv) * lerp(0.93, 1.04, fo.x);
  g = (qa * cos(q.x * wv) * s2 + qb * s1 * cos(q.y * wv)) * 0.04 * awv + fo.yzw * 0.06 * F.aa(10.0, fw);
  rough = 0.86; spec = 0.3;
  col += fres * 0.14 * VC.rgb;
} else if (kind < 3.5) {           // hair: strands running down
  float4 st = F.nd(p * float3(260.0, 260.0, 7.0));
  float4 cl = F.nd(p * float3(40.0, 40.0, 3.0));
  float ast = F.aa(260.0, fw);
  df = lerp(1.0, lerp(0.72, 1.18, st.x), ast) * lerp(0.9, 1.08, cl.x);
  g = (st.yzw * float3(1.0, 1.0, 0.03)) * 0.12 * ast + cl.yzw * 0.05 * F.aa(40.0, fw);
  rough = 0.36 + 0.1 * cl.x; spec = 0.45;
} else if (kind < 4.5) {           // gelcoat: glossy, a faint orange peel
  float4 op = F.nd(p * 55.0);
  g = op.yzw * 0.006 * F.aa(55.0, fw);
  rough = 0.08 + 0.03 * F.nd(p * 2.0).x; spec = 0.6;
} else if (kind < 5.5) {           // non-skid: a fine field of low rounded bumps, gone before it can alias
  float2 c = frac(q * 38.0) - 0.5;
  float r = length(c);
  float an = F.aa(38.0, fw);
  float dome = smoothstep(0.34, 0.16, r);
  g = (qa * c.x + qb * c.y) * 0.45 * dome * an;
  float4 dirt = F.nd(p * 3.0);
  df = lerp(1.0, lerp(0.965, 1.0, dome), an) * lerp(0.95, 1.02, dirt.x);
  rough = 0.78; spec = 0.4;
} else if (kind < 6.5) {           // stainless: brushed, bright
  float4 br = F.nd(p * float3(4.0, 4.0, 90.0));
  metal = 1.0; rough = 0.05 + 0.07 * br.x;
  g = br.yzw * 0.006 * F.aa(90.0, fw);
  col = lerp(col, float3(0.92, 0.93, 0.95), 0.6);
} else if (kind < 7.5) {           // marine vinyl
  float4 gr = F.nd(p * 160.0);
  float4 pu = F.nd(p * 6.0);
  df = lerp(0.97, 1.03, pu.x);
  float agr = F.aa(160.0, fw);
  g = gr.yzw * 0.012 * agr + pu.yzw * 0.025;
  rough = 0.4 + 0.08 * gr.x * agr; spec = 0.45;
  col += fres * 0.06;
} else if (kind < 8.5) {           // wood: grain along the plank, darker late-wood rings, weathered
  // grain: long streaks along the plank (the longest local axis of most timber here is x or y), late-wood lines
  float3 gs = abs(ln.x) > 0.6 ? float3(40.0, 1.5, 40.0) : float3(1.5, 40.0, 40.0);
  if (abs(ln.z) > 0.6) gs = float3(1.5, 30.0, 30.0);
  float4 n = F.fbm(p * gs, 3);
  float ring = frac(n.x * 3.0 + p.z * 0.0);
  float4 wx = F.nd(p * 2.5);
  df = lerp(0.74, 1.08, smoothstep(0.0, 0.85, ring)) * lerp(0.86, 1.06, wx.x);
  df = lerp(0.92, df, midF);
  g = n.yzw * float3(0.02, 0.2, 0.2) * F.aa(80.0, fw) + F.nd(p * float3(8.0, 90.0, 90.0)).yzw * 0.01 * F.aa(90.0, fw);
  rough = 0.8; spec = 0.35;
} else if (kind < 9.5) {           // stucco
  float4 b = F.fbm(p * 34.0, 3);
  float4 lo = F.nd(p * 0.9);
  df = lerp(0.92, 1.04, b.x) * lerp(0.93, 1.03, lo.x);
  g = b.yzw * 0.13 * F.aa(70.0, fw) + F.nd(p * 5.0).yzw * 0.02;
  rough = 0.92; spec = 0.35;
} else if (kind < 10.5) {          // corrugated tin
  float3 t = cross(ln, float3(0, 0, 1));
  t = dot(t, t) < 0.01 ? float3(1, 0, 0) : normalize(t);
  float ph = dot(p, t) * 6.2831853 * 11.0;
  g = t * cos(ph) * 0.5 * F.aa(11.0, fw);
  float4 r = F.nd(p * 2.6);
  df = lerp(1.0, 0.9 + 0.1 * (0.5 + 0.5 * sin(ph)), midF);
  col = lerp(col, col * float3(0.78, 0.58, 0.45), smoothstep(0.62, 0.92, r.x) * 0.6);
  metal = 0.55; rough = 0.34 + 0.25 * r.x; spec = 0.5;
} else if (kind < 11.5) {          // asphalt
  float4 n1 = F.nd(p * 85.0);
  float4 n2 = F.nd(p * 2.0);
  float4 n3 = F.nd(p * 0.35);
  float an1 = F.aa(85.0, fw);
  df = lerp(1.0, lerp(0.8, 1.15, n1.x), an1) * lerp(0.9, 1.06, n2.x) * lerp(0.92, 1.06, n3.x);
  g = n1.yzw * 0.08 * an1;
  rough = 0.86 + 0.08 * n2.x; spec = 0.4;
} else if (kind < 12.5) {          // the ground: sand with wind ripples and grain, grass where it is green
  float veg = saturate((VC.g - VC.r) * 9.0 + 0.1);
  float2 dir = float2(0.8, 0.6);
  float4 w = F.nd(p * 0.6);
  float ph = dot(p.xy, dir) * 6.2831853 * 2.6 + w.x * 7.0;
  float4 gr = F.nd(p * 110.0);
  float agd = F.aa(110.0, fw);
  float sdf = lerp(1.0, lerp(0.92, 1.06, gr.x), agd) * lerp(0.96, 1.03, 0.5 + 0.5 * sin(ph));
  float3 sg = float3(dir * cos(ph) * 0.1, 0) * midF + gr.yzw * 0.03 * agd;
  float4 bl = F.fbm(p * 18.0, 2);
  float4 lo = F.nd(p * 0.3);
  float4 lo2 = F.nd(p * 0.07);
  float abl = F.aa(36.0, fw);
  float gdf = lerp(1.0, lerp(0.78, 1.14, bl.x), abl) * lerp(0.82, 1.12, lo.x) * lerp(0.88, 1.08, lo2.x);
  float3 gg = bl.yzw * 0.12 * abl + lo.yzw * 0.04;
  df = lerp(sdf, gdf, veg);
  g = lerp(sg, gg, veg);
  // patches of dry, yellower grass
  col = lerp(col, col * float3(1.18, 1.05, 0.7), veg * smoothstep(0.55, 0.8, lo2.x) * 0.6);
  rough = lerp(0.94, 0.9, veg); spec = 0.35;
} else if (kind < 13.5) {          // leaves: waxy, veined
  float4 v = F.nd(p * 26.0);
  float4 lo = F.nd(p * 2.0);
  df = lerp(0.86, 1.1, v.x) * lerp(0.9, 1.08, lo.x);
  g = v.yzw * 0.05 * F.aa(26.0, fw);
  rough = 0.5; spec = 0.38;
  col = lerp(col, col * float3(1.12, 1.12, 0.8), fres * 0.6);
} else if (kind < 14.5) {          // glass
  rough = 0.03; spec = 1.0; col *= 0.55;
} else if (kind < 15.5) {          // fish: overlapping scales in offset rows, a wet slime sheen and a rainbow at the edges
  float2 sq = q * 90.0;
  sq.y += 0.5 * floor(sq.x);
  float2 c = frac(sq) - 0.5;
  float rim = smoothstep(0.25, 0.5, length(c));
  float asc = F.aa(90.0, fw);
  g = (qa * c.x + qb * c.y) * 0.22 * asc + F.nd(p * 9.0).yzw * 0.025 + F.nd(p * 600.0).yzw * 0.01 * F.aa(600.0, fw);
  float3 ir = 0.5 + 0.5 * cos(6.2831853 * (ndv * 1.4 + float3(0.0, 0.33, 0.67)));
  // a wet sheen, not a mirror: in full sun the old glassy setting (rough 0.07, spec 0.95, +45% rainbow) burned the
  // flanks out to white
  col = col * lerp(1.0, 0.88, rim * asc) * (1.0 + 0.14 * ir * (1.0 - ndv));
  rough = 0.26 + 0.1 * rim * asc; spec = 0.55;
} else if (kind < 16.5) {          // bark / palm trunk: rings and fibres
  float4 w = F.nd(p * 3.0);
  float ph = p.z * 6.2831853 * 4.5 + w.x * 2.0;
  float4 fb = F.nd(p * float3(36.0, 36.0, 2.5));
  float afb = F.aa(36.0, fw);
  df = lerp(0.8, 1.06, 0.5 + 0.5 * sin(ph)) * lerp(1.0, lerp(0.84, 1.06, fb.x), afb);
  g = float3(0, 0, cos(ph)) * 0.35 * midF + fb.yzw * float3(0.12, 0.12, 0.01) * afb;
  rough = 0.9; spec = 0.3;
} else if (kind < 17.5) {          // rubber, dark plastic
  float4 n = F.nd(p * 70.0);
  g = n.yzw * 0.02 * F.aa(70.0, fw);
  rough = 0.55; spec = 0.35;
} else if (kind < 18.5) {          // lap siding
  float s = frac(p.z / 0.19);
  float vert = 1.0 - saturate((aa.z - 0.5) * 4.0);
  df = lerp(1.0, lerp(0.76, 1.0, smoothstep(0.0, 0.14, s)), vert * midF);
  g = float3(0, 0, lerp(0.7, -0.06, smoothstep(0.0, 0.1, s))) * vert * midF + F.nd(p * float3(2, 2, 40)).yzw * 0.02 * nearF;
  rough = 0.72; spec = 0.45;
} else if (kind < 19.5) {          // concrete
  float4 n = F.fbm(p * 11.0, 3);
  df = lerp(0.88, 1.06, n.x) * lerp(0.94, 1.03, F.nd(p * 0.7).x);
  g = n.yzw * 0.06 * F.aa(44.0, fw);
  rough = 0.86; spec = 0.4;
} else if (kind < 20.5) {          // fins: thin, glossy, with rays
  float4 r = F.nd(p * float3(160.0, 160.0, 12.0));
  float afn = F.aa(160.0, fw);
  df = lerp(1.0, lerp(0.88, 1.08, r.x), afn);
  g = r.yzw * 0.04 * afn;
  rough = 0.16; spec = 0.75;
  col += fres * 0.1 * VC.rgb;
} else if (kind < 21.5) {          // eyes, lenses, wet things
  rough = 0.04; spec = 0.9;
} else if (kind < 22.5) {          // asphalt shingles
  float row = p.z * 8.0;
  float s = frac(row);
  float3 t = cross(ln, float3(0, 0, 1));
  t = dot(t, t) < 0.01 ? float3(1, 0, 0) : normalize(t);
  float tab = floor(dot(p, t) * 3.0 + floor(row) * 0.5);
  float hv = F.hash(float3(tab, floor(row), 3.0));
  df = lerp(1.0, lerp(0.72, 1.0, smoothstep(0.0, 0.2, s)) * lerp(0.86, 1.08, hv), midF) * lerp(1.0, lerp(0.92, 1.04, F.nd(p * 60.0).x), F.aa(60.0, fw));
  g = float3(0, 0, lerp(0.6, -0.1, smoothstep(0.0, 0.15, s))) * midF;
  rough = 0.85; spec = 0.35;
} else if (kind > 23.5 && kind < 24.5) {   // diamond-quilted cushion
  float2 r = float2(q.x + q.y, q.x - q.y) * 0.7071 * 11.0;
  float2 c = frac(r) - 0.5;
  float puff = 1.0 - 2.0 * max(abs(c.x), abs(c.y));
  float seam = 1.0 - smoothstep(0.0, 0.08, puff);
  float2 sg = sign(c) * step(abs(c.yx), abs(c));
  float2 gq = float2(sg.x + sg.y, sg.x - sg.y) * 0.7071;
  g = (qa * gq.x + qb * gq.y) * 0.35 * (1.0 - puff) * F.aa(16.0, fw) + F.nd(p * 150.0).yzw * 0.01 * F.aa(150.0, fw);
  df = lerp(1.0, 0.72, seam * nearF) * lerp(0.96, 1.03, puff);
  rough = 0.48; spec = 0.45;
} else if (kind > 24.5 && kind < 25.5) {   // honeycomb-quilted leather with dark stitching
  float2 h = q * 22.0;
  float2 hr = float2(1.0, 1.7320508);
  float2 ha = fmod(abs(h), hr) - hr * 0.5;
  float2 hb = fmod(abs(h - hr * 0.5), hr) - hr * 0.5;
  float2 hc = dot(ha, ha) < dot(hb, hb) ? ha : hb;
  float2 ah = abs(hc);
  float hd = max(dot(ah, normalize(float2(1.0, 1.7320508))), ah.x);   // 0 centre .. 0.5 edge
  float puff = 1.0 - saturate(hd * 2.0);
  float seam = 1.0 - smoothstep(0.03, 0.09, 0.5 - hd);
  float2 gq = hc / max(length(hc), 1e-3) * (1.0 - puff);
  g = (qa * gq.x + qb * gq.y) * 0.3 * F.aa(22.0, fw) + F.nd(p * 140.0).yzw * 0.012 * F.aa(140.0, fw);
  df = lerp(1.0, 0.35, seam * nearF) * lerp(0.95, 1.04, puff);
  rough = 0.42; spec = 0.5;
} else if (kind > 25.5 && kind < 26.5) {   // metallic paint with flake, under a glossy clear
  float4 fl = F.nd(p * 420.0);
  float4 lo = F.nd(p * 1.5);
  float afl = F.aa(420.0, fw);
  g = fl.yzw * 0.03 * afl + lo.yzw * 0.004;
  // fully metallic so the sky it reflects is tinted by the paint (a dielectric layer would wash it to grey), a little
  // rougher so the reflection is a soft sheen, and grazing angles brightening the paint's own colour, not white
  metal = 0.92; rough = 0.26 + 0.08 * (fl.x * afl + 0.5 * (1.0 - afl)); spec = 0.35;
  col *= lerp(0.95, 1.06, fl.x * afl + 0.5 * (1.0 - afl));
  col *= 1.0 + fres * 0.35;
} else if (kind > 26.5 && kind < 27.5) {   // teak-look foam decking with black caulk lines, fore and aft
  float pl = frac(p.y / 0.07);
  float caulk = 1.0 - smoothstep(0.0, 0.08, min(pl, 1.0 - pl));
  float4 gr = F.fbm(p * float3(2.0, 40.0, 40.0), 2);
  float acl = F.aa(14.0, fw);
  df = lerp(lerp(0.9, 1.06, gr.x), lerp(0.75, 0.12, acl), caulk * midF);
  g = float3(0, sign(pl - 0.5) * caulk * 0.4, 0) * acl + gr.yzw * 0.02 * F.aa(40.0, fw);
  rough = 0.8; spec = 0.3;
} else {                           // grass, ground cover
  float4 n = F.fbm(p * 22.0, 2);
  float4 lo = F.nd(p * 0.45);
  float agc = F.aa(44.0, fw);
  df = lerp(1.0, lerp(0.8, 1.12, n.x), agc) * lerp(0.86, 1.1, lo.x);
  g = n.yzw * 0.08 * agc;
  rough = 0.88; spec = 0.35;
}
col *= df;
float3 gw = g.x * ax + g.y * ay + g.z * az;
gw -= dot(gw, N0) * N0;
Nrm = normalize(N0 - gw);
Rough = saturate(rough);
Metal = metal;
Spec = spec;
return saturate(col);
"""


def _vertex_color(mat):
    mat.set_editor_property("tangent_space_normal", False)
    F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1400, 0)
    uv1 = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 120)
    uv1.set_editor_property("coordinate_index", 1)
    lp = mel.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -1400, 240)
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1400, 360)
    cv = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -1400, 480)
    dp = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1400, 600)
    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTwoSidedSign, -1400, 1100)
    axes = []
    for i, v in enumerate(((1, 0, 0), (0, 1, 0), (0, 0, 1))):
        c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -1400, 720 + i * 120)
        c.set_editor_property("constant", unreal.LinearColor(v[0], v[1], v[2], 1.0))
        t = mel.create_material_expression(mat, unreal.MaterialExpressionTransform, -1150, 720 + i * 120)
        t.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
        t.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        _link(c, "", t, "")
        axes.append(t)
    s = _custom(mat, -700, 0, SURFACE_FN, F3, ["VC", "K", "LP", "VN", "CV", "D", "AX", "AY", "AZ", "TS"], "Surface")
    outs = []
    for name, ty in (("Rough", F1), ("Metal", F1), ("Spec", F1), ("Nrm", F3)):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", ty)
        outs.append(o)
    s.set_editor_property("additional_outputs", outs)
    for src, pin in ((vc, "VC"), (uv1, "K"), (lp, "LP"), (vn, "VN"), (cv, "CV"), (dp, "D"), (axes[0], "AX"), (axes[1], "AY"), (axes[2], "AZ"), (ts, "TS")):
        _link(src, "", s, pin)
    mel.connect_material_property(s, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(s, "Rough", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(s, "Metal", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(s, "Spec", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(s, "Nrm", unreal.MaterialProperty.MP_NORMAL)


# The schooling fish (KeysSchools.cpp). The motion lives here, on the GPU: the C++ only steers a few leaders per school
# and writes their recent track (20 samples a second, ~2.4 s of history) into a float texture, one row per leader,
# four texels per sample: A = position (cm) + tail-beat phase, B = heading + the school's spread, C = the threat
# (position + strength), D = floor top z, surface z, swim strength, alarm. Every fish is an instance that never moves:
# from its custom data (0 leader row, 1 delay s, 2-4 slot cm in the leader's frame, 5 seed, 6 length m, 7 shade) this
# shader reads where its leader was Delay seconds ago, places the fish at its slot around that (so turns ripple back
# through the school), adds an idle wander, pushes it away from the threat (the school splits around a predator and
# closes behind it), turns it along its motion, bends the body in a tail wave and returns the whole move as WPO.
# The mesh runs nose (+X 50) to tail (-50) in its own units.
FISH_MOVE = """
float hist = max(Hist, 4.0);
float s = Head - Delay / StepDt;
float i0 = floor(s); float fr = s - i0;
i0 = i0 - floor(i0 / hist) * hist;
float i1 = i0 + 1.0; i1 = i1 - floor(i1 / hist) * hist;
float vv = (Row + 0.5) / Rows;
float tw = hist * 4.0;
float4 A0 = Texture2DSampleLevel(Tex, TexSampler, float2((i0 * 4.0 + 0.5) / tw, vv), 0);
float4 A1 = Texture2DSampleLevel(Tex, TexSampler, float2((i1 * 4.0 + 0.5) / tw, vv), 0);
float4 B0 = Texture2DSampleLevel(Tex, TexSampler, float2((i0 * 4.0 + 1.5) / tw, vv), 0);
float4 B1 = Texture2DSampleLevel(Tex, TexSampler, float2((i1 * 4.0 + 1.5) / tw, vv), 0);
float4 C0 = Texture2DSampleLevel(Tex, TexSampler, float2((i0 * 4.0 + 2.5) / tw, vv), 0);
float4 C1 = Texture2DSampleLevel(Tex, TexSampler, float2((i1 * 4.0 + 2.5) / tw, vv), 0);
float4 D0 = Texture2DSampleLevel(Tex, TexSampler, float2((i0 * 4.0 + 3.5) / tw, vv), 0);
float4 D1 = Texture2DSampleLevel(Tex, TexSampler, float2((i1 * 4.0 + 3.5) / tw, vv), 0);
float ph1 = A1.w; if (ph1 < A0.w - 100.0) ph1 += 3216.9908772;
float3 P = lerp(A0.xyz, A1.xyz, fr);
float ph = lerp(A0.w, ph1, fr);
float4 B = lerp(B0, B1, fr); float4 C = lerp(C0, C1, fr); float4 D = lerp(D0, D1, fr);
float3 Up = float3(0, 0, 1);
float3 F = normalize(B.xyz + float3(1e-4, 0, 0));
float3 R = cross(Up, F); float rl = length(R); R = rl > 1e-3 ? R / rl : float3(0, 1, 0);
float3 U = cross(F, R);
float3 sl = float3(SlotX, SlotY, SlotZ);
float sp = 6.0 + 0.1 * length(sl);
sl *= B.w;
float a1 = Seed * 40.0, a2 = Seed * 71.0, a3 = Seed * 13.0;
float f1 = 0.31 + Seed * 0.2, f2 = 0.27 + frac(Seed * 7.0) * 0.2, f3 = 0.21;
float3 wv = float3(sin(T * f1 + a1), sin(T * f2 + a2), 0.5 * sin(T * f3 + a3)) * sp;
float3 wd = float3(f1 * cos(T * f1 + a1), f2 * cos(T * f2 + a2), 0.5 * f3 * cos(T * f3 + a3)) * sp;
float3 pos = P + F * (sl.x + wv.x) + R * (sl.y + wv.y) + U * (sl.z + wv.z);
// the threat: fish within ~10 m of it are pushed off its line and close in again behind it
float3 dv = pos - C.xyz; float dist = length(dv) + 1.0;
float k = C.w * saturate(1.6 - dist / 700.0);
float3 away = dv / dist; away.z *= 0.35;
pos += away * k * 450.0;
pos.z = clamp(pos.z, D.x + 15.0 + Size * 25.0, D.y - 20.0 - Size * 12.0);
// face along the motion: the leader's heading, the wander and the dodge
float3 Fh = F * 110.0 + (F * wd.x + R * wd.y + U * wd.z) * 1.4 + away * (k * 300.0);
Fh.z = clamp(Fh.z, -0.5 * length(Fh.xy), 0.5 * length(Fh.xy));
float3 F2 = normalize(Fh + float3(1e-4, 0, 0));
float3 R2 = cross(Up, F2); float r2 = length(R2); R2 = r2 > 1e-3 ? R2 / r2 : float3(0, 1, 0);
float3 U2 = cross(F2, R2);
// the body wave
float u = saturate((50.0 - LP.x) / 100.0);
float env = 0.05 + 0.95 * u * u;
float amp = D.z * (1.0 + 1.2 * k);
float lat = amp * 12.0 * env * sin(ph + Seed * 6.2831853 - u * 5.2);
float3 lv = float3(LP.x - abs(lat) * 0.15 * u, LP.y + lat, LP.z) * Size;
float3 world = pos + F2 * lv.x + R2 * lv.y + U2 * lv.z;
float fl = saturate(k + D.w * 0.6) * (0.5 + 0.5 * sin(Seed * 31.0 + T * 6.0));
FT = float4(F2, Shade * (1.0 + 0.7 * fl));
RR = R2;
return world - WP;
"""

# pixel side: the mesh's normals and axes turned to the way the fish faces now
FISH_AXES = """
float3 F = normalize(FT.xyz); float3 R = normalize(RR); float3 U = cross(F, R);
AY = R; AZ = U;
return F;
"""

FISH_NRM = """
float3 F = normalize(FT.xyz); float3 R = normalize(RR); float3 U = cross(F, R);
return normalize(F * VN.x + R * VN.y + U * VN.z);
"""


def _fish_tex():
    path = MAT_DIR + "/T_KR_FishData"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tex = tools.create_asset("T_KR_FishData", MAT_DIR, unreal.Texture2D, unreal.Texture2DFactoryNew())
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_HDR)
    tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    eal.save_loaded_asset(tex)
    return tex


def _fish(mat):
    F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    F4 = unreal.CustomMaterialOutputType.CMOT_FLOAT4
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -2400, 1400)
    tex.set_editor_property("parameter_name", "LeaderTex")
    tex.set_editor_property("texture", _fish_tex())
    tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    names = ["Row", "Delay", "SlotX", "SlotY", "SlotZ", "Seed", "Size", "Shade"]
    cd = {}
    for i, n in enumerate(names):
        c = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -2400, 1500 + i * 80)
        c.set_editor_property("data_index", i)
        cd[n] = c
    params = {}
    for i, (n, dv) in enumerate((("Head", 0.0), ("HeadPrev", 0.0), ("Hist", 48.0), ("Rows", 512.0), ("StepDt", 0.05))):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -2400, 2200 + i * 80)
        e.set_editor_property("parameter_name", n)
        e.set_editor_property("default_value", dv)
        params[n] = e
    # motion vectors: the previous frame reads the track as it stood then
    pf = mel.create_material_expression(mat, unreal.MaterialExpressionPreviousFrameSwitch, -2100, 2200)
    if not (mel.connect_material_expressions(params["Head"], "", pf, "Current Frame") and mel.connect_material_expressions(params["HeadPrev"], "", pf, "Previous Frame")):
        unreal.log_warning("Keys Run: no previous-frame switch; fish motion vectors follow the current frame")
        pf = params["Head"]
    lp = mel.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -2400, 2650)
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -2400, 2750)
    try:
        wp.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    except Exception as e:
        unreal.log_warning("Keys Run: fish world position: %s" % e)
    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -2400, 2850)
    mv = _custom(mat, -1800, 1600, FISH_MOVE, F3, ["Tex", "Row", "Delay", "SlotX", "SlotY", "SlotZ", "Seed", "Size", "Shade", "LP", "WP", "T", "Head", "Hist", "Rows", "StepDt"], "Follow the leader")
    outs = []
    for name, ty in (("FT", F4), ("RR", F3)):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", ty)
        outs.append(o)
    mv.set_editor_property("additional_outputs", outs)
    _link(tex, "", mv, "Tex")
    for n in names:
        _link(cd[n], "", mv, n)
    for src, pin in ((lp, "LP"), (wp, "WP"), (tm, "T"), (pf, "Head"), (params["Hist"], "Hist"), (params["Rows"], "Rows"), (params["StepDt"], "StepDt")):
        _link(src, "", mv, pin)
    mel.connect_material_property(mv, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    ift = mel.create_material_expression(mat, unreal.MaterialExpressionVertexInterpolator, -1500, 1900)
    _link(mv, "FT", ift, "")
    irr = mel.create_material_expression(mat, unreal.MaterialExpressionVertexInterpolator, -1500, 2000)
    _link(mv, "RR", irr, "")
    # the everyday surface (as M_KR_VertexColor), with the paint brightened per fish and the frame turned with it
    mat.set_editor_property("tangent_space_normal", False)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1400, 0)
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1300, 1900)
    tint.set_editor_property("r", False); tint.set_editor_property("g", False); tint.set_editor_property("b", False); tint.set_editor_property("a", True)
    _link(ift, "", tint, "")
    vct = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1150, 0)
    _link(vc, "", vct, "A")
    _link(tint, "", vct, "B")
    uv1 = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 120)
    uv1.set_editor_property("coordinate_index", 1)
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1400, 360)
    cv = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -1400, 480)
    dp = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1400, 600)
    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTwoSidedSign, -1400, 1100)
    ax = _custom(mat, -1100, 700, FISH_AXES, F3, ["FT", "RR"], "Fish axes")
    aouts = []
    for name in ("AY", "AZ"):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", F3)
        aouts.append(o)
    ax.set_editor_property("additional_outputs", aouts)
    nr = _custom(mat, -1100, 360, FISH_NRM, F3, ["FT", "RR", "VN"], "Fish normal")
    for c in (ax, nr):
        _link(ift, "", c, "FT")
        _link(irr, "", c, "RR")
    _link(vn, "", nr, "VN")
    s = _custom(mat, -700, 0, SURFACE_FN, F3, ["VC", "K", "LP", "VN", "CV", "D", "AX", "AY", "AZ", "TS"], "Surface")
    so = []
    for name, ty in (("Rough", F1), ("Metal", F1), ("Spec", F1), ("Nrm", F3)):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", ty)
        so.append(o)
    s.set_editor_property("additional_outputs", so)
    for src, out, pin in ((vct, "", "VC"), (uv1, "", "K"), (lp, "", "LP"), (nr, "", "VN"), (cv, "", "CV"), (dp, "", "D"), (ax, "", "AX"), (ax, "AY", "AY"), (ax, "AZ", "AZ"), (ts, "", "TS")):
        _link(src, out, s, pin)
    mel.connect_material_property(s, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(s, "Rough", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(s, "Metal", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(s, "Spec", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(s, "Nrm", unreal.MaterialProperty.MP_NORMAL)


OCEAN_SCATTER = """
// Single Layer Water: per-meter scattering, boosted where light shines through the thin top of a crest,
// and turned milky in the aerated prop wash. Returned per centimeter, which is what the water output wants.
float glow = saturate((UV.x - 0.4) * 1.6) * (1.0 - saturate(F));
// gin-clear turquoise over the flats, a cleaner, bluer cobalt out over the deep (the Gulf Stream has almost nothing in it)
float3 sc = lerp(Scat * float3(1.25, 1.2, 0.95), Scat * float3(0.6, 0.62, 1.12), saturate(Deep));
sc *= (1.0 + glow * 3.0);
// the prop wash: milky with air, not a lit-up cyan carpet. Boils of churned water well up and drift apart, darker
// clearer water between them; the milk is whiter and greener than the sea round it, never neon.
float2 bp = WP.xy / 100.0 * 0.32 + float2(T * 0.07, -T * 0.05);
float boil = 0.0, bamp = 0.6;
for (int bi = 0; bi < 3; bi++) {
  float2 bi2 = floor(bp), bf = frac(bp); float2 bu = bf * bf * (3.0 - 2.0 * bf);
  float3 a3 = frac(float3(bi2.xyx) * .1031); a3 += dot(a3, a3.yzx + 33.33); float n00 = frac((a3.x + a3.y) * a3.z);
  float3 b3 = frac(float3((bi2 + float2(1, 0)).xyx) * .1031); b3 += dot(b3, b3.yzx + 33.33); float n10 = frac((b3.x + b3.y) * b3.z);
  float3 c3 = frac(float3((bi2 + float2(0, 1)).xyx) * .1031); c3 += dot(c3, c3.yzx + 33.33); float n01 = frac((c3.x + c3.y) * c3.z);
  float3 d3 = frac(float3((bi2 + float2(1, 1)).xyx) * .1031); d3 += dot(d3, d3.yzx + 33.33); float n11 = frac((d3.x + d3.y) * d3.z);
  boil += bamp * lerp(lerp(n00, n10, bu.x), lerp(n01, n11, bu.x), bu.y);
  bp = bp * 2.3 + float2(5.2, 1.3) + float2(-T * 0.09, T * 0.06); bamp *= 0.5;
}
boil = saturate(boil / 1.05);
float wash = saturate(UV.y) * smoothstep(0.38, 0.85, boil * 0.85 + saturate(UV.y) * 0.3);
sc = lerp(sc, float3(0.13, 0.16, 0.15) * (0.5 + 0.9 * boil), wash * 0.38);
return sc / 100.0;
"""

OCEAN_ABSORB = """
return Absorb / 100.0;
"""


def _ocean(mat):
    # Single Layer Water: the floor shows through with real refraction and per-channel absorption, so the flats
    # read as clear turquoise over sand and the deep as blue. Foam is the material's own surface (Opacity = how much
    # of it covers the water); the ripples, foam patches, wakes and wash are unchanged.
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    mat.set_editor_property("tangent_space_normal", False)
    mat.set_editor_property("two_sided", False)
    F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1400, 0)
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, 200)
    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1400, 350)
    dp = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1400, 450)
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1400, 550)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 650)
    aer = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1150, 650)
    aer.set_editor_property("r", False)
    aer.set_editor_property("g", True)
    _link(uv, "", aer, "")
    strength = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1150, 760)
    strength.set_editor_property("parameter_name", "RippleStrength")
    strength.set_editor_property("default_value", 1.0)
    wdir = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1150, 1300)
    wdir.set_editor_property("parameter_name", "WindDir")
    wdir.set_editor_property("default_value", 3.4)
    # UV1: x = rain here, y = whitecap cover; UV2: x = how deep (0 flats .. 1 blue water)
    uv1 = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 1400)
    uv1.set_editor_property("coordinate_index", 1)
    uv2 = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 1500)
    uv2.set_editor_property("coordinate_index", 2)
    rain = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1150, 1400)
    rain.set_editor_property("r", True); rain.set_editor_property("g", False)
    _link(uv1, "", rain, "")
    caps = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1150, 1460)
    caps.set_editor_property("r", False); caps.set_editor_property("g", True)
    _link(uv1, "", caps, "")
    deep = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1150, 1520)
    deep.set_editor_property("r", True); deep.set_editor_property("g", False)
    _link(uv2, "", deep, "")

    trackdir = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1150, 1600)
    trackdir.set_editor_property("parameter_name", "TrackDir")
    trackdir.set_editor_property("default_value", 0.0)
    # the simulated sea's textures (KeysOcean, filled every frame by AKeysWorld) and its tile sizes
    black = eal.load_asset("/Engine/EngineResources/Black")
    sims = []
    for i, nm in enumerate(("Sim0", "Sim1")):
        t = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -1400, 1800 + i * 150)
        t.set_editor_property("parameter_name", nm)
        if black: t.set_editor_property("texture", black)
        t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        sims.append(t)
    def _sp(name, dv, y):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1150, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", dv)
        return e
    sim_l0 = _sp("SimL0", 133.0, 2100); sim_l1 = _sp("SimL1", 31.0, 2160); sim_on = _sp("SimOn", 0.0, 2220)
    foam = _custom(mat, -900, 0, OCEAN_FOAM, F1, ["WP", "T", "A", "D", "WD", "Caps", "Rain", "Aer", "TD", "Sim0", "Sim1", "L0", "L1", "On"], "Foam")
    for src, pin in ((sims[0], "Sim0"), (sims[1], "Sim1"), (sim_l0, "L0"), (sim_l1, "L1"), (sim_on, "On")):
        _link(src, "", foam, pin)
    _link(aer, "", foam, "Aer")
    _link(trackdir, "", foam, "TD")
    _link(wp, "", foam, "WP")
    _link(tm, "", foam, "T")
    _link(vc, "A", foam, "A")
    _link(dp, "", foam, "D")
    _link(wdir, "", foam, "WD")
    _link(caps, "", foam, "Caps")
    _link(rain, "", foam, "Rain")

    nrm = _custom(mat, -500, 300, OCEAN_NORMAL, F3, ["WP", "T", "VN", "D", "F", "Aer", "Strength", "WD", "Rain", "Sim0", "Sim1", "L0", "L1", "On", "Deep"], "Ripples")
    for src, pin in ((wp, "WP"), (tm, "T"), (vn, "VN"), (dp, "D"), (foam, "F"), (aer, "Aer"), (strength, "Strength"), (wdir, "WD"), (rain, "Rain"),
                     (sims[0], "Sim0"), (sims[1], "Sim1"), (sim_l0, "L0"), (sim_l1, "L1"), (sim_on, "On"), (deep, "Deep")):
        _link(src, "", nrm, pin)

    rough = _custom(mat, -500, 150, OCEAN_ROUGH, F1, ["F", "D", "Rain", "Caps"], "Roughness")
    _link(foam, "", rough, "F")
    _link(dp, "", rough, "D")
    _link(rain, "", rough, "Rain")
    _link(caps, "", rough, "Caps")

    foam_col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, -150)
    foam_col.set_editor_property("constant", unreal.LinearColor(0.84, 0.9, 0.93, 1.0))

    scat = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 900)
    scat.set_editor_property("parameter_name", "Scattering")
    scat.set_editor_property("default_value", unreal.LinearColor(0.003, 0.012, 0.034, 1.0))
    absorb = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 1050)
    absorb.set_editor_property("parameter_name", "Absorption")
    absorb.set_editor_property("default_value", unreal.LinearColor(0.45, 0.085, 0.04, 1.0))
    sc = _custom(mat, -500, 900, OCEAN_SCATTER, F3, ["Scat", "UV", "F", "Deep", "WP", "T"], "Scattering")
    _link(wp, "", sc, "WP")
    _link(tm, "", sc, "T")
    _link(deep, "", sc, "Deep")
    _link(scat, "", sc, "Scat")
    _link(uv, "", sc, "UV")
    _link(foam, "", sc, "F")
    ab = _custom(mat, -500, 1050, OCEAN_ABSORB, F3, ["Absorb"], "Absorption")
    _link(absorb, "", ab, "Absorb")
    phase = _const(mat, 0.2, -500, 1200)
    out = mel.create_material_expression(mat, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, -150, 1000)
    _link(sc, "", out, "ScatteringCoefficients")
    _link(ab, "", out, "AbsorptionCoefficients")
    _link(phase, "", out, "PhaseG")

    mel.connect_material_property(foam_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_const(mat, 0.255, -500, 800), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(foam, "", unreal.MaterialProperty.MP_OPACITY)
    # the sun glitter (KeysWorld sets SunDir and Glint every frame)
    sundir = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 1900)
    sundir.set_editor_property("parameter_name", "SunDir")
    sundir.set_editor_property("default_value", unreal.LinearColor(0.5, -0.6, 0.6, 0.0))
    glintk = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 2050)
    glintk.set_editor_property("parameter_name", "Glint")
    glintk.set_editor_property("default_value", 1.0)
    camv = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -900, 2150)
    gl = _custom(mat, -500, 1900, OCEAN_GLINT, F3, ["SunDir", "CamV", "N", "WP", "D", "T", "F", "Glint"], "Glitter")
    for src, pin in ((sundir, "SunDir"), (camv, "CamV"), (nrm, "N"), (wp, "WP"), (dp, "D"), (tm, "T"), (foam, "F"), (glintk, "Glint")):
        _link(src, "", gl, pin)
    mel.connect_material_property(gl, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def _solid(mat):
    col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(0.8, 0.8, 0.8, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 220)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.8)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)



# Water caustics: the bright net of focused sunlight on the bottom (a few warped layers, Dave Hoskins' trick),
# strongest in the first 10 m and gone by 20 m. Shared by the sea floor and the reef.
CAUSTICS_FN = """
float2 cp = P * 1.9 + 250.0;
float2 ci = cp;
float cc = 1.0;
const float inten = 0.005;
for (int n = 0; n < 4; n++) {
  float tt = T * 0.45 * (1.0 - (3.5 / float(n + 1)));
  ci = cp + float2(cos(tt - ci.x) + sin(tt + ci.y), sin(tt - ci.y) + cos(tt + ci.x));
  cc += 1.0 / length(float2(cp.x / (sin(ci.x + tt) / inten), cp.y / (cos(ci.y + tt) / inten)));
}
cc /= 4.0;
cc = 1.17 - pow(abs(cc), 1.4);
float caus = pow(abs(cc), 8.0);
float cdepth = max(0.0, -WZ / 100.0);
float band = (1.0 - smoothstep(10.0, 20.0, cdepth)) * saturate(cdepth * 1.5);
// far away the net is finer than a pixel; fade to its average instead of shimmering
float cfade = 1.0 - saturate((D / 100.0 - 25.0) / 90.0);
caus = lerp(0.12, caus, cfade) * band;
"""

SEAFLOOR_BASE = """
float2 p = WP.xy / 100.0;
// sand grain: two octaves of value noise
float2 q = p * 3.1;
float g = 0.0, amp = 0.6;
for (int i = 0; i < 2; i++) {
  float2 ip = floor(q), fp = frac(q);
  float2 u = fp * fp * (3.0 - 2.0 * fp);
  float a0 = frac(sin(dot(ip, float2(127.1, 311.7))) * 43758.5453);
  float a1 = frac(sin(dot(ip + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
  float a2 = frac(sin(dot(ip + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
  float a3 = frac(sin(dot(ip + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
  g += amp * lerp(lerp(a0, a1, u.x), lerp(a2, a3, u.x), u.y);
  q *= 4.3; amp *= 0.4;
}
float grain = lerp(0.9, 1.06, g);
float2 P = p; float WZ = WP.z;
""" + CAUSTICS_FN + """
return saturate(VC.rgb * grain * (0.58 + 0.9 * caus));
"""


SEAFLOOR_NORMAL = KRF_NOISE + """
KRF F;
float3 p = WP / 100.0;
float dist = D / 100.0;
float nearF = 1.0 - saturate((dist - 4.0) / 30.0);
float midF = 1.0 - saturate((dist - 20.0) / 80.0);
// sand where the floor is pale; ripples run across the surge, broken up by a slow warp
float sand = saturate((dot(VC.rgb, float3(0.33, 0.33, 0.33)) - 0.25) * 4.0);
float4 w = F.nd(p * 0.35);
float2 dir = normalize(float2(0.6, 0.8));
float ph = dot(p.xy, dir) * 6.2831853 * 1.6 + w.x * 9.0;
float sh = sin(ph);
float3 g = float3(dir * (cos(ph) * (0.6 + 0.4 * sh)) * 0.22 * sand * midF, 0.0);
float4 gr = F.nd(p * 70.0);
float4 lump = F.fbm(p * 1.5, 2);
g += gr.yzw * 0.05 * nearF + lump.yzw * 0.1;
float3 N0 = float3(0, 0, 1);
g -= dot(g, N0) * N0;
return normalize(N0 - g);
"""


def _seafloor(mat):
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, 0)
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 150)
    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -900, 300)
    dp = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -900, 400)
    base = _custom(mat, -500, 0, SEAFLOOR_BASE, F3, ["VC", "WP", "T", "D"], "Sand, grass and caustics")
    for src, pin in ((vc, "VC"), (wp, "WP"), (tm, "T"), (dp, "D")):
        _link(src, "", base, pin)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_const(mat, 0.95, -500, 220), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_const(mat, 0.3, -500, 320), "", unreal.MaterialProperty.MP_SPECULAR)
    mat.set_editor_property("tangent_space_normal", False)
    nrm = _custom(mat, -500, 450, SEAFLOOR_NORMAL, F3, ["VC", "WP", "D"], "Ripples")
    for src, pin in ((vc, "VC"), (wp, "WP"), (dp, "D")):
        _link(src, "", nrm, pin)
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)


# Coral, sponges and boulders. Instance color comes in per-instance custom data 0-2; vertex red is the mottle and
# the darker ground-contact band baked into each model.
REEF_BASE = """
float2 P = WP.xy / 100.0; float WZ = WP.z;
""" + CAUSTICS_FN + """
return saturate(Inst * VC.r * (0.8 + 1.0 * caus));
"""


def _inst_color(mat, x, y):
    parts = []
    for i in range(3):
        c = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, x, y + i * 90)
        c.set_editor_property("data_index", i)
        parts.append(c)
    ap = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, x + 200, y)
    _link(parts[0], "", ap, "A")
    _link(parts[1], "", ap, "B")
    ap2 = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, x + 350, y + 60)
    _link(ap, "", ap2, "A")
    _link(parts[2], "", ap2, "B")
    return ap2


# coral surface: polyp pits and fine grooves as a world-space detail normal, faded with distance
REEF_NORMAL = KRF_NOISE + """
KRF F;
float3 p = WP / 100.0;
float3 N0 = normalize(VN) * TS;
float dist = D / 100.0;
float nearF = 1.0 - saturate((dist - 3.0) / 22.0);
float4 a = F.nd(p * 38.0);
float4 b = F.fbm(p * 7.0, 2);
float pits = abs(a.x - 0.5);
float3 g = a.yzw * 0.09 * nearF + b.yzw * 0.07;
g -= dot(g, N0) * N0;
return normalize(N0 - g);
"""


def _reef(mat):
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    inst = _inst_color(mat, -1300, 0)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, 300)
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 450)
    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -900, 600)
    dp = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -900, 700)
    base = _custom(mat, -500, 0, REEF_BASE, F3, ["Inst", "VC", "WP", "T", "D"], "Coral color")
    for src, pin in ((inst, "Inst"), (vc, "VC"), (wp, "WP"), (tm, "T"), (dp, "D")):
        _link(src, "", base, pin)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_const(mat, 0.7, -500, 220), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property("tangent_space_normal", False)
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 800)
    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTwoSidedSign, -900, 900)
    nrm = _custom(mat, -500, 400, REEF_NORMAL, F3, ["WP", "VN", "D", "TS"], "Polyps")
    for src, pin in ((wp, "WP"), (vn, "VN"), (dp, "D"), (ts, "TS")):
        _link(src, "", nrm, pin)
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)


# Sea fans, sea plumes, seagrass and far coral cards: alpha-tested cards that sway with the current and the surge.
# Custom data 3 is the cut-out (0 seagrass blade, 1 sea fan lattice, 2 lacy plume), 4 the flexibility.
# UV0.y runs 0 at the root to 1 at the tip.
REEF_CARD_MASK = """
float2 uv = UV;
float a = 1.0;
if (Kind < 0.5) {
  float hw = 0.5 * (1.0 - 0.75 * uv.y);
  a = 1.0 - smoothstep(hw - 0.08, hw, abs(uv.x - 0.5));
} else if (Kind < 1.5) {
  // a gorgonian: a fan-shaped outline (wider at the top, ragged), main branches forking up from the stem, and
  // between them a fine irregular net (the edges of a stretched Voronoi pattern), denser toward the rim
  float2 c = (uv - float2(0.5, 0.06)) * float2(1.0, 0.95);
  float ang = atan2(c.x, c.y);
  float rr = length(c);
  float rim = 0.86 + 0.05 * sin(ang * 7.0 + 1.3) + 0.03 * sin(ang * 17.0) + 0.02 * sin(ang * 31.0 + 2.0);
  float shape = (1.0 - smoothstep(rim - 0.04, rim, rr)) * (1.0 - smoothstep(1.05, 1.25, abs(ang)));
  // Voronoi edges in a frame stretched along the radius (the net's cells run outward)
  float2 q = float2(ang * 17.0, rr * 30.0);
  q += float2(sin(q.y * 1.7) * 0.25, sin(q.x * 1.3) * 0.25);
  float2 ip = floor(q), fp = frac(q);
  float f1 = 8.0, f2 = 8.0;
  for (int j = -1; j <= 1; j++)
    for (int i = -1; i <= 1; i++) {
      float2 o = float2(i, j);
      float2 h = frac(sin(float2(dot(ip + o, float2(127.1, 311.7)), dot(ip + o, float2(269.5, 183.3)))) * 43758.5453);
      float dd = length(o + h - fp);
      if (dd < f1) { f2 = f1; f1 = dd; } else if (dd < f2) { f2 = dd; }
    }
  float net = (1.0 - smoothstep(0.05, 0.1, f2 - f1)) * step(0.06, rr);
  // main branches: a few forking ribs from the stem, thinning outward
  float br = 0.0;
  for (int k = 0; k < 7; k++) {
    float a0 = (float(k) - 3.0) * 0.32;
    float a1 = a0 + sin(float(k) * 2.1 + rr * 5.0) * 0.08 * rr;
    float wdt = (0.022 - rr * 0.014) / max(rr, 0.05);
    br = max(br, 1.0 - smoothstep(wdt * 0.5, wdt, abs(ang - a1)));
  }
  float stem = (1.0 - smoothstep(0.018, 0.03, abs(uv.x - 0.5))) * (1.0 - step(0.12, uv.y));
  a = max(shape * max(net, br * step(0.04, rr)), stem);
} else {
  float2 c = uv - float2(0.5, 0.0);
  float n = sin(uv.y * 41.0 + uv.x * 13.0) * sin(uv.x * 37.0 - uv.y * 7.0) + 0.6 * sin(uv.y * 87.0 + uv.x * 51.0);
  float outline = 1.0 - smoothstep(0.32, 0.5, abs(c.x) / (0.25 + 0.75 * sin(min(1.0, uv.y * 1.05) * 3.14159)));
  a = outline * step(-0.35, n) * (1.0 - step(0.98, uv.y));
}
return a;
"""

REEF_CARD_BASE = """
float2 P = WP.xy / 100.0; float WZ = WP.z;
""" + CAUSTICS_FN + """
return saturate(Inst * (0.55 + 0.45 * UV.y) * (0.82 + 0.9 * caus));
"""

REEF_CARD_WPO = """
// the web game's attachSway: lean with the steady current, swing with the wave surge, more at the tip
float2 root = InstPos.xy / 100.0;
float sPh = SurgePhase - dot(root, SurgeAxis.xy) * 0.18479957;
float2 surge = SurgeAxis.xy * ((sin(sPh) + 0.35 * sin(2.0 * sPh + 1.3)) * 0.85 * SurgeAmp);
float2 lean = Flow.xy / (1.0 + length(Flow.xy) * 0.75);
float flex = UV.y * UV.y;
float2 bend = (lean + surge) * Flex * flex;
return float3(bend * 100.0, -length(bend) * flex * 45.0);
"""


def _reef_card(mat):
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    # thin fans, plumes and blades let the light through: lit from either side
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    inst = _inst_color(mat, -1500, 0)
    kind = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -1300, 400)
    kind.set_editor_property("data_index", 3)
    flex = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -1300, 500)
    flex.set_editor_property("data_index", 4)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1300, 600)
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1300, 700)
    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1300, 800)
    dp = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1300, 900)
    ip = mel.create_material_expression(mat, unreal.MaterialExpressionObjectPositionWS, -1300, 1000)
    mask = _custom(mat, -700, 400, REEF_CARD_MASK, F1, ["UV", "Kind"], "Cut-out")
    _link(uv, "", mask, "UV")
    _link(kind, "", mask, "Kind")
    base = _custom(mat, -700, 0, REEF_CARD_BASE, F3, ["Inst", "UV", "WP", "T", "D"], "Card color")
    for src, pin in ((inst, "Inst"), (uv, "UV"), (wp, "WP"), (tm, "T"), (dp, "D")):
        _link(src, "", base, pin)
    params = []
    for i, (nm, kindp, dv) in enumerate((("Flow", "v", (0, 0)), ("SurgeAxis", "v", (0, 1)), ("SurgePhase", "s", 0), ("SurgeAmp", "s", 0))):
        if kindp == "v":
            e = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1000, 1100 + i * 120)
            e.set_editor_property("default_value", unreal.LinearColor(dv[0], dv[1], 0, 0))
        else:
            e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1000, 1100 + i * 120)
            e.set_editor_property("default_value", dv)
        e.set_editor_property("parameter_name", nm)
        params.append((nm, e))
    wpo = _custom(mat, -500, 800, REEF_CARD_WPO, F3, ["InstPos", "UV", "Flex", "Flow", "SurgeAxis", "SurgePhase", "SurgeAmp"], "Sway")
    _link(ip, "", wpo, "InstPos")
    _link(uv, "", wpo, "UV")
    _link(flex, "", wpo, "Flex")
    for nm, e in params:
        _link(e, "", wpo, nm)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    mel.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(_const(mat, 0.9, -500, 220), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)


# Underwater: the web game's global fog override (per-channel extinction, depth-banded visibility, the deep
# inscatter veil and its ambient falloff) as a post-process, plus its lens wetting (a soft vignette under water
# and droplets on the mask right after crossing the surface).
UNDERWATER = """
float3 col = Col;
float camDepth = max(0.0, -Cam.z / 100.0);
float d = Dist / 100.0;
// the web game's path: object-to-eye plus the camera's own depth (the light coming down to it). The pixel's own depth
// is folded in at half weight, so a floor far below a diver near the surface still loses its reds.
float pixDepth = clamp(-WP.z / 100.0, 0.0, 60.0);
const float3 K = float3(0.22, 0.095, 0.045) * 0.38;
float3 tr = exp(-K * (d + max(camDepth, 0.5 * (camDepth + pixDepth))));
float amb = exp(-camDepth / 11.0);
float3 insc = float3(0.045, 0.34, 0.47) * Haze / max(Ex, 1e-6);
float3 below = col * tr + insc * amb * (1.0 - tr);
float vis = 10.0;
if (camDepth <= 0.0) vis = 30.0;
else if (camDepth <= 5.0) vis = lerp(30.0, 25.0, camDepth / 5.0);
else if (camDepth <= 10.0) vis = lerp(25.0, 20.0, (camDepth - 5.0) / 5.0);
else if (camDepth <= 15.0) vis = lerp(20.0, 15.0, (camDepth - 10.0) / 5.0);
else if (camDepth <= 20.0) vis = lerp(15.0, 10.0, (camDepth - 15.0) / 5.0);
vis += Bonus;
float turb = smoothstep(vis * 0.6, vis, d);
below = lerp(below, insc * amb, turb);
// light shafts: march the view ray through the first 20 m, sampling a caustic-like pattern projected down
// the refracted sun direction from the surface; brighter near the surface, under the sun's side of the sky
{
  float3 sd = normalize(SunDir.xyz);
  float3 rd = normalize(SunDir.xyz * float3(0.75, 0.75, 1.0));   // refraction steepens the beams
  rd.z = max(rd.z, 0.35);
  rd = normalize(rd);
  float3 ro = Cam / 100.0;
  float3 vd = normalize(WP - Cam);
  float maxD = min(d, 22.0);
  float acc = 0.0;
  const int NS = 14;
  for (int i = 0; i < NS; i++) {
    float t = (float(i) + frac(sin(dot(UV, float2(12.9898, 78.233)) + float(i)) * 43758.5453)) / float(NS) * maxD;
    float3 sp = ro + vd * t;
    float dep = max(0.0, -sp.z);
    // where this point's beam crossed the surface
    float2 s2 = sp.xy + rd.xy / rd.z * dep;
    // a caustic net (warped cells) seen from below: many soft beams rather than a few planes
    float2 q = s2 * 0.9 + float2(Time * 0.05, Time * 0.035);
    float2 wq = q + 0.6 * float2(sin(q.y * 1.3 + Time * 0.4), sin(q.x * 1.1 - Time * 0.33));
    float n = sin(wq.x * 2.9) * sin(wq.y * 3.3) + 0.6 * sin((wq.x + wq.y) * 4.7 + Time * 0.6) * sin((wq.x - wq.y) * 3.9);
    float beam = pow(saturate(n * 0.5 + 0.2), 2.0);
    acc += beam * exp(-dep / 7.0) * exp(-t / 14.0) * (sp.z < 0.0 ? 1.0 : 0.0);
  }
  acc /= float(NS);
  float facing = 0.45 + 0.55 * saturate(dot(vd, sd) * 0.5 + 0.5);
  float3 shaft = float3(0.55, 0.85, 0.9) * acc * facing * 2.8 * amb / max(Ex, 1e-6) * 0.25;
  below += shaft * (1.0 - turb * 0.5);
}
col = lerp(col, below, Under);
float vig = smoothstep(0.35, 0.95, length(UV - 0.5));
col = lerp(col, col * 0.9, vig * Under * 0.5);
if (Wet > 0.003) {
  float aspectR = View.ViewSizeAndInvSize.x / max(View.ViewSizeAndInvSize.y, 1.0);
  float acc = 0.0;
  for (int i = 0; i < 9; i++) {
    float2 seed = float2(float(i) * 12.9898, float(i) * 78.233);
    float2 center = float2(frac(sin(dot(seed, float2(41.3, 289.1))) * 43758.5453), frac(sin(dot(seed + 7.0, float2(41.3, 289.1))) * 43758.5453));
    float r = 0.012 + 0.03 * frac(sin(dot(seed + 13.0, float2(41.3, 289.1))) * 43758.5453);
    float2 dd = (UV - center) * float2(aspectR, 1.0);
    float dist = length(dd) / r;
    float drop = smoothstep(1.0, 0.75, dist) * (1.0 - smoothstep(0.0, 0.55, dist) * 0.6);
    acc = max(acc, drop);
  }
  col += float3(0.75, 0.85, 0.9) * acc * Wet * 0.5 / max(Ex, 1e-6) * 0.18;
  col = lerp(col, col * (1.0 - 0.25 * acc), Wet);
}
return col;
"""


def _underwater(mat):
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    col = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -1200, 0)
    col.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    dep = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -1200, 200)
    dep.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_SCENE_DEPTH)
    dmask = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1000, 200)
    dmask.set_editor_property("r", True); dmask.set_editor_property("g", False)
    dmask.set_editor_property("b", False); dmask.set_editor_property("a", False)
    _link(dep, "Color", dmask, "")
    cmask = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1000, 0)
    cmask.set_editor_property("r", True); cmask.set_editor_property("g", True)
    cmask.set_editor_property("b", True); cmask.set_editor_property("a", False)
    _link(col, "Color", cmask, "")
    cam = mel.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -1200, 350)
    sp = mel.create_material_expression(mat, unreal.MaterialExpressionScreenPosition, -1200, 450)
    ex = mel.create_material_expression(mat, unreal.MaterialExpressionEyeAdaptation, -1200, 550)
    params = {}
    for i, (nm, dv) in enumerate((("Under", 0.0), ("Wet", 0.0), ("Bonus", 0.0), ("Haze", 1.0), ("Time", 0.0))):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1200, 700 + i * 100)
        e.set_editor_property("parameter_name", nm)
        e.set_editor_property("default_value", dv)
        params[nm] = e
    sdir = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1200, 1250)
    sdir.set_editor_property("parameter_name", "SunDir")
    sdir.set_editor_property("default_value", unreal.LinearColor(0.55, -0.62, 0.56, 0.0))
    params["SunDir"] = sdir
    uw = _custom(mat, -600, 0, UNDERWATER, F3, ["Col", "Dist", "Cam", "WP", "UV", "Ex", "Under", "Wet", "Bonus", "Haze", "Time", "SunDir"], "Underwater")
    wpp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1200, 1150)
    _link(wpp, "", uw, "WP")
    _link(cmask, "", uw, "Col")
    _link(dmask, "", uw, "Dist")
    _link(cam, "", uw, "Cam")
    _link(sp, "ViewportUV", uw, "UV")
    _link(ex, "", uw, "Ex")
    for nm, e in params.items():
        _link(e, "", uw, nm)
    mel.connect_material_property(uw, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


# The underside of the sea seen from below: Snell's window (the whole sky squeezed into a ~97 degree cone overhead)
# and, outside it, total internal reflection of the dim water below (web game world/water.ts, back-face branch).
WATER_UNDER = """
float3 Vw = normalize(Cam - WP);
float3 rayDir = -Vw;
float2 p = WP.xy / 100.0;
float2 rip = float2(sin(p.x * 1.7 + T * 1.3) + sin(p.y * 2.3 - T * 1.1), cos(p.x * 2.1 - T * 0.9) + sin(p.y * 1.5 + T * 1.6)) * 0.035;
float3 Nw = normalize(float3(-VN.x + rip.x, -VN.y + rip.y, abs(VN.z)));
float cosT = clamp(dot(rayDir, Nw), -1.0, 1.0);
float windowT = saturate(acos(cosT) / 0.84806);
float3 refr = refract(rayDir, -Nw, 1.3333);
float3 o;
if (dot(refr, refr) < 1e-5) {
  float shimmer = 0.5 + 0.5 * sin(p.x * 0.9 + reflect(rayDir, Nw).x * 3.0 + T * 0.7) * sin(p.y * 1.1 - T * 0.5);
  o = float3(0.04, 0.1, 0.15) * (0.55 + 0.65 * shimmer) * 0.3;
} else {
  float3 skyDir = normalize(refr);
  float3 zen = Sky * 0.78;
  float3 rim = lerp(Sky, float3(1.0, 0.99, 0.95), 0.6);
  o = lerp(zen, rim, pow(windowT, 1.8));
  float sunCos = max(dot(skyDir, normalize(SunDir.xyz)), 0.0);
  o += float3(1.0, 0.92, 0.78) * pow(sunCos, 140.0) * 1.6;
  o += float3(1.0, 0.97, 0.92) * pow(sunCos, 1800.0) * 5.0;
}
return o * Bright / max(Ex, 1e-6);
"""


def _water_under(mat):
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", False)
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1200, 0)
    cam = mel.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -1200, 120)
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1200, 240)
    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1200, 360)
    ex = mel.create_material_expression(mat, unreal.MaterialExpressionEyeAdaptation, -1200, 480)
    sky = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1200, 600)
    sky.set_editor_property("parameter_name", "Sky")
    sky.set_editor_property("default_value", unreal.LinearColor(0.49, 0.69, 0.81, 1))
    sun = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1200, 750)
    sun.set_editor_property("parameter_name", "SunDir")
    sun.set_editor_property("default_value", unreal.LinearColor(0.55, -0.62, 0.32, 0))
    br = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1200, 900)
    br.set_editor_property("parameter_name", "Bright")
    br.set_editor_property("default_value", 0.9)
    c = _custom(mat, -600, 0, WATER_UNDER, F3, ["WP", "Cam", "VN", "T", "Ex", "Sky", "SunDir", "Bright"], "Snell's window")
    for src, pin in ((wp, "WP"), (cam, "Cam"), (vn, "VN"), (tm, "T"), (ex, "Ex"), (sky, "Sky"), (sun, "SunDir"), (br, "Bright")):
        _link(src, "", c, pin)
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


# Bubbles and marine snow: camera-facing quads, translucent and unlit. UV in [0,1] draws a bubble (bright rim and a
# highlight), UV offset by 2 draws a soft speck of snow. Vertex color is the display color, alpha the opacity.
BUBBLE = """
bool snow = UV.x >= 1.5;
float2 c = frac(UV) - 0.5;
float d = length(c);
float a;
if (snow) a = smoothstep(0.5, 0.0, d);
else {
  float rim = smoothstep(0.5, 0.34, d) * (0.35 + 0.65 * smoothstep(0.2, 0.46, d));
  float hl = smoothstep(0.12, 0.0, length(c - float2(-0.14, -0.14))) * 0.5;
  a = rim + hl;
}
a *= step(d, 0.5);
return float4(VC.rgb * Bright / max(Ex, 1e-6), saturate(a * VA));
"""


def _bubble(mat):
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    F4 = unreal.CustomMaterialOutputType.CMOT_FLOAT4
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1000, 0)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1000, 150)
    ex = mel.create_material_expression(mat, unreal.MaterialExpressionEyeAdaptation, -1000, 300)
    br = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1000, 400)
    br.set_editor_property("parameter_name", "Bright")
    br.set_editor_property("default_value", 0.12)
    c = _custom(mat, -600, 0, BUBBLE, F4, ["VC", "VA", "UV", "Ex", "Bright"], "Bubble")
    for src, pin in ((vc, "VC"), (uv, "UV"), (ex, "Ex"), (br, "Bright")):
        _link(src, "", c, pin)
    _link(vc, "A", c, "VA")
    rgb = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -300, 0)
    rgb.set_editor_property("r", True); rgb.set_editor_property("g", True); rgb.set_editor_property("b", True); rgb.set_editor_property("a", False)
    _link(c, "", rgb, "")
    al = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -300, 150)
    al.set_editor_property("r", False); al.set_editor_property("g", False); al.set_editor_property("b", False); al.set_editor_property("a", True)
    _link(c, "", al, "")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(al, "", unreal.MaterialProperty.MP_OPACITY)


# The helm's instruments: gauge faces and the big screens are drawn every frame into a canvas render target
# (KeysDash.cpp) and shown through this. The picture glows (backlit, steady under any exposure, brighter at dusk);
# underneath it is dark, glossy glass, so the sky and the sun slide across the dials as the boat moves.
DASH = """
return Tex.rgb * Bright / max(Ex, 1e-6);
"""


def _dash(mat):
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1000, 0)
    tex.set_editor_property("parameter_name", "Tex")
    tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    ex = mel.create_material_expression(mat, unreal.MaterialExpressionEyeAdaptation, -1000, 300)
    br = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1000, 400)
    br.set_editor_property("parameter_name", "Bright")
    br.set_editor_property("default_value", 1.0)
    c = _custom(mat, -600, 0, DASH, F3, ["Tex", "Ex", "Bright"], "Dash")
    _link(tex, "RGB", c, "Tex")
    _link(ex, "", c, "Ex")
    _link(br, "", c, "Bright")
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    base = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 250)
    base.set_editor_property("const_b", 0.03)
    _link(tex, "RGB", base, "A")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_const(mat, 0.12, -600, 400), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_const(mat, 0.35, -600, 500), "", unreal.MaterialProperty.MP_SPECULAR)


# The windshields of your own boat, so the helm view looks out through them: tinted (the vertex colour), glossy, lit
# with reflections, mostly see-through.
def _glass(mat):
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -800, 0)
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    op = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 250)
    op.set_editor_property("parameter_name", "Opacity")
    op.set_editor_property("default_value", 0.22)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(_const(mat, 0.04, -800, 400), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_const(mat, 0.9, -800, 500), "", unreal.MaterialProperty.MP_SPECULAR)


# The girls (Content/KeysRun/Crew/Girl, Tools/girlglb.py): one textured figure in three sections, each with its own
# material, and each girl her own skin tone, hair, eye and swimsuit colours (vector parameters set per figure).
# The source texture is a baked, pale-peach skin, dark navy hair and a navy swimsuit with white stripes; every pixel is
# recoloured by what it shows: peach -> the skin tone (its shading and blush kept as a ratio to the average skin),
# navy -> hair or swimsuit colour (its light and dark kept), the white stripes -> the stripe colour, the bright blue of
# the eyes -> the eye colour. Fine strands run down the hair and a soft weave over the swimsuit, from noise in the
# figure's own rest space so they ride along with the dance; both fade out with distance.
GIRL_FN = KRF_NOISE + """
KRF F;
float3 t = T;
float l = dot(t, float3(0.3, 0.59, 0.11));
float sat = max(t.r, max(t.g, t.b)) - min(t.r, min(t.g, t.b));
float3 p = P / 100.0;
#if PIXELSHADER
float fw = max(length(ddx(p)), length(ddy(p)));
#else
float fw = 0.01;
#endif
float3 n = normalize(lerp(float3(0, 0, 1), NM, NS));
float2 g = 0;
// peach: the skin, its pale bake remapped to this girl's tone (0.92 0.69 0.52 is the bake's average skin)
float skin = saturate((t.r - t.b) * 6.0 - 0.6) * saturate((t.r - t.g) * 12.0) * saturate(t.r * 4.0 - 1.2);
// (its contrast eased: the bake's streaks and seams fade, its shading and blush stay)
float3 sk = pow(max(t / float3(0.92, 0.69, 0.52), 0.0), 0.6) * Tone;
float4 m = F.nd(p * 30.0);
float rough = 0.5 - 0.08 * m.x; float spec = 0.36;
float3 col;
if (Part < 0.5) {
  // anything here that is not peach (a navy-tinted seam, a stray texel of swimsuit) is just more skin
  col = lerp(Tone * lerp(0.8, 1.0, m.x), sk, saturate(skin * 2.0));
} else if (Part < 1.5) {
  // hair: dark navy -> the hair colour, keeping the bake's light and shade; bright, unsaturated texels a sheen on
  // the hair (and the eye whites a soft grey);
  // the bright blue irises -> the eye colour; peach here is the face's edge -> skin
  float dark = 1.0 - smoothstep(0.1, 0.3, l);
  float4 st = F.nd(p * float3(320.0, 320.0, 9.0));
  float4 cl = F.nd(p * float3(45.0, 45.0, 3.0));
  float ast = F.aa(320.0, fw);
  float3 hc = Hair * clamp(l / 0.045, 0.45, 2.0) * lerp(1.0, lerp(0.7, 1.25, st.x), ast) * lerp(0.88, 1.1, cl.x);
  float blue = saturate((t.b - t.r) * 5.0) * smoothstep(0.08, 0.2, l);
  float3 other = lerp(lerp(hc * 1.5, float3(l, l, l) * 0.7, 0.3), Eye * l * 2.5, blue);
  other = lerp(other, sk, skin);
  col = lerp(other, hc, dark);
  g = (st.yz * 0.16 * ast + cl.yz * 0.05) * dark;
  rough = lerp(0.45, 0.32 + 0.12 * cl.x, dark); spec = lerp(0.4, 0.55, dark);
} else {
  // swimsuit: navy -> the suit colour, the white stripes -> the stripe colour, a fine lycra knit
  float stripe = smoothstep(0.25, 0.5, l) * (1.0 - saturate(sat * 2.5));
  float3 sc = Suit * clamp(l / 0.05, 0.4, 1.8);
  col = lerp(sc, Suit2 * saturate(l * 1.3), stripe);
  col = lerp(col, sk, skin);
  float4 kn = F.nd(p * 400.0);
  float akn = F.aa(400.0, fw);
  g = kn.yz * 0.05 * akn * (1.0 - skin);
  rough = lerp(0.42 + 0.1 * kn.x * akn, rough, skin); spec = lerp(0.45, spec, skin);
}
Nrm = normalize(n + float3(g, 0));
Rough = saturate(rough);
Spec = spec;
return saturate(col);
"""


def _girl(part):
    def build(mat):
        mat.set_editor_property("two_sided", part == 1)
        mat.set_editor_property("used_with_skeletal_mesh", True)
        if part == 0:
            # the engine's default skin scattering profile (no profile asset needed)
            mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE_PROFILE)
        F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
        F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
        tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1400, 0)
        tex.set_editor_property("parameter_name", "Base")
        tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
        nm = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1400, 300)
        nm.set_editor_property("parameter_name", "Normal")
        nm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        nm.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"))
        # the rest-pose position is a vertex-shader value: carried to the pixels through an interpolator
        lp = mel.create_material_expression(mat, unreal.MaterialExpressionPreSkinnedPosition, -1700, 600)
        vi = mel.create_material_expression(mat, unreal.MaterialExpressionVertexInterpolator, -1400, 600)
        _link(lp, "", vi, "")
        ins = [(tex, "RGB", "T"), (nm, "RGB", "NM"), (vi, "", "P")]
        y = 700
        for name, v in (("Tone", (0.6, 0.42, 0.32)), ("Hair", (0.05, 0.025, 0.012)), ("Eye", (0.12, 0.07, 0.03)),
                        ("Suit", (0.02, 0.05, 0.25)), ("Suit2", (0.9, 0.9, 0.9))):
            c = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1400, y)
            c.set_editor_property("parameter_name", name)
            c.set_editor_property("default_value", unreal.LinearColor(v[0], v[1], v[2], 1.0))
            ins.append((c, "", name))
            y += 120
        ns = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, y)
        ns.set_editor_property("parameter_name", "NormalStrength")
        ns.set_editor_property("default_value", 0.45)
        ins.append((ns, "", "NS"))
        ins.append((_const(mat, float(part), -1400, y + 120), "", "Part"))
        s = _custom(mat, -700, 0, GIRL_FN, F3, [i[2] for i in ins], "Girl")
        outs = []
        for name, ty in (("Rough", F1), ("Spec", F1), ("Nrm", F3)):
            o = unreal.CustomOutput()
            o.set_editor_property("output_name", name)
            o.set_editor_property("output_type", ty)
            outs.append(o)
        s.set_editor_property("additional_outputs", outs)
        for src, out, pin in ins:
            _link(src, out, s, pin)
        mel.connect_material_property(s, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(s, "Rough", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(s, "Spec", unreal.MaterialProperty.MP_SPECULAR)
        mel.connect_material_property(s, "Nrm", unreal.MaterialProperty.MP_NORMAL)
    return build


# The sky's clouds (the volumetric cloud layer, ray-marched by the engine): fair-weather cumulus with flat bases near
# 700 m and rounded tops, drifting with the wind, and up to four storm cells - towering cumulonimbus with a dark flat
# base, a column swelling in turrets and an anvil sheared downwind - placed and grown by KeysWeather. Lightning lights
# the cloud from inside (Flash, FlashI). Density is per meter; out: x density, y flash glow, z how storm-dark.
CLOUD = """
struct KCN {
  float h(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
  float n(float3 x) {
    float3 i = floor(x); float3 f = frac(x); f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(h(i), h(i + float3(1, 0, 0)), f.x), lerp(h(i + float3(0, 1, 0)), h(i + float3(1, 1, 0)), f.x), f.y),
                lerp(lerp(h(i + float3(0, 0, 1)), h(i + float3(1, 0, 1)), f.x), lerp(h(i + float3(0, 1, 1)), h(i + float3(1, 1, 1)), f.x), f.y), f.z);
  }
};
KCN K;
float3 p = WP / 100.0;
// height above the curved sea (the planet's top is at the origin)
float alt = p.z + dot(p.xy, p.xy) / 12742000.0;
float dens = 0.0, dark = 0.0;
// ---- fair-weather cumulus
float cbase = 700.0;
if (alt > cbase && alt < cbase + 2500.0) {
  float2 q = (p.xy + Drift.xy) / 1700.0;
  float cv = K.n(float3(q, 0.5)) * 0.65 + K.n(float3(q * 2.7, 3.7)) * 0.35;
  cv = saturate((cv - (1.0 - Cov)) * 3.2);
  if (cv > 0.0) {
    float hc = 250.0 + 2200.0 * cv * cv;
    float u = (alt - cbase) / hc;
    float3 r3 = float3(p.xy + Drift.xy, alt);
    float det = K.n(r3 / 190.0) * 0.62 + K.n(r3 / 70.0) * 0.38;
    float body = cv * 1.15 - u * u - 0.12 - (det - 0.5) * 0.55;
    dens = saturate(body * 4.0) * saturate(u * 12.0);
  }
}
// ---- storm cells
float3 Cs[4] = {C0, C1, C2, C3};
float Ps[4] = {P0, P1, P2, P3};
for (int i = 0; i < 4; i++) {
  float pw = Ps[i];
  if (pw < 0.01) continue;
  float R = Cs[i].z;
  float top = 1400.0 + 10500.0 * saturate(pw);
  float u = (alt - 550.0) / (top - 550.0);
  if (u < 0.0 || u > 1.1) continue;
  float2 dv = p.xy - Cs[i].xy;
  float2 dva = dv - Shear.xy * u * u * R * 1.4;
  float r = length(dv), ra = length(dva);
  if (min(r, ra) > R * 5.0) continue;
  float3 r3 = float3(p.xy, alt) + float3(Cs[i].xy * 0.37, 0.0);
  // cauliflower turrets: big lumps, smaller lumps on them
  float nz = K.n(r3 / 1100.0) * 0.5 + K.n(r3 / 420.0) * 0.32 + K.n(r3 / 150.0) * 0.18 - 0.5;
  // a wide column swelling through the middle and rounding off into a dome at the top
  float colR = R * (1.0 + 0.45 * sin(saturate(u) * 3.1416)) * (0.65 + 0.35 * saturate(pw)) * sqrt(saturate((1.0 - u) * 3.5));
  float col = saturate((colR - r) / (0.3 * R) + nz * 3.2);
  float an = smoothstep(0.72, 0.9, u) * saturate((1.04 - u) * 12.0) * saturate((pw - 0.45) * 3.0);
  float anvil = saturate((R * (1.6 + 2.4 * saturate(pw)) - ra) / (0.6 * R) + nz * 2.0) * an;
  float d = max(col, anvil) * saturate(u * 25.0);
  dens = max(dens, d * 1.7);
  dark = max(dark, d * saturate(pw));
}
float glow = 0.0;
if (FlI > 0.001) glow = FlI * exp(-length(float3(p.xy, alt) - Fl) / 1800.0);
return float4(dens, glow, dark, 0.0);
"""


def _cloud(mat):
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_VOLUME)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    # without this the cloud passes' shaders are never compiled for it, and the renderer falls back and asserts
    mat.set_editor_property("used_with_volumetric_cloud", True)
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    F4 = unreal.CustomMaterialOutputType.CMOT_FLOAT4
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
    ins = [("WP", wp)]
    y = 100
    for nm, dv in (("Drift", (0, 0, 0)), ("Shear", (1, 0, 0)), ("Cell0", (0, 0, 1000)), ("Cell1", (0, 0, 1000)),
                   ("Cell2", (0, 0, 1000)), ("Cell3", (0, 0, 1000)), ("Flash", (0, 0, 0))):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1400, y)
        e.set_editor_property("parameter_name", nm)
        e.set_editor_property("default_value", unreal.LinearColor(dv[0], dv[1], dv[2], 0))
        ins.append(({"Cell0": "C0", "Cell1": "C1", "Cell2": "C2", "Cell3": "C3", "Flash": "Fl"}.get(nm, nm), e))
        y += 120
    for nm, dv in (("Cov", 0.4), ("P0", 0.0), ("P1", 0.0), ("P2", 0.0), ("P3", 0.0), ("FlashI", 0.0)):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, y)
        e.set_editor_property("parameter_name", nm)
        e.set_editor_property("default_value", dv)
        ins.append(("FlI" if nm == "FlashI" else nm, e))
        y += 100
    d = _custom(mat, -900, 0, CLOUD, F4, [n for n, _ in ins], "Cloud density")
    for n, e in ins:
        _link(e, "", d, n)
    ext = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 400)
    ext.set_editor_property("parameter_name", "Ext")
    ext.set_editor_property("default_value", 0.045)
    alb = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 500)
    alb.set_editor_property("parameter_name", "Albedo")
    alb.set_editor_property("default_value", unreal.LinearColor(0.93, 0.95, 1.0, 1))
    fc = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 650)
    fc.set_editor_property("parameter_name", "FlashCol")
    fc.set_editor_property("default_value", unreal.LinearColor(4.0, 4.2, 5.0, 1))
    e_ext = _custom(mat, -500, 0, "return D.x * Ext;", F3, ["D", "Ext"], "Extinction")
    _link(d, "", e_ext, "D")
    _link(ext, "", e_ext, "Ext")
    e_alb = _custom(mat, -500, 150, "return Alb * lerp(1.0, 0.82, saturate(D.z));", F3, ["D", "Alb"], "Albedo")
    _link(d, "", e_alb, "D")
    _link(alb, "", e_alb, "Alb")
    e_em = _custom(mat, -500, 300, "return FC * D.y * saturate(D.x);", F3, ["D", "FC"], "Lightning")
    _link(d, "", e_em, "D")
    _link(fc, "", e_em, "FC")
    mel.connect_material_property(e_alb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(e_ext, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    mel.connect_material_property(e_em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


# Rain and the things made of water in the air, translucent and unlit, one material in modes (KeysWeather):
# 0 falling streaks on the cylinders around the camera (UV in meters), 1 a rain shaft / spray curtain (soft, streaky,
# thinner at grazing angles), 2 a waterspout's twisting funnel, 3 a lightning bolt's ribbon (UV.y across it).
# Vertex alpha and Amount scale the opacity; Fade softens where it meets the scene (cm).
RAIN = """
struct KRN {
  float h(float2 p) { float3 p3 = frac(float3(p.xyx) * .1031); p3 += dot(p3, p3.yzx + 33.33); return frac((p3.x + p3.y) * p3.z); }
  float n(float2 x) { float2 i = floor(x); float2 f = frac(x); f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(h(i), h(i + float2(1, 0)), f.x), lerp(h(i + float2(0, 1)), h(i + float2(1, 1)), f.x), f.y); }
};
KRN K;
int mode = (int)round(Mode);
float a = 0.0;
float3 c = Tint;
float facing = saturate(abs(dot(normalize(N), normalize(CV))));
if (mode == 0) {
  float cw = 7.0;
  float col = floor(UV.x * cw);
  float h1 = K.h(float2(col, 1.7)), h2 = K.h(float2(col, 9.3));
  float y = UV.y * 0.55 + T * (4.2 + 1.8 * h1) + h1 * 10.0;
  float f = frac(y);
  float streak = smoothstep(0.0, 0.03, f) * (1.0 - smoothstep(0.1, 0.18, f));
  float xf = frac(UV.x * cw);
  float w = 1.0 - smoothstep(0.02, 0.09, abs(xf - 0.5 - (h2 - 0.5) * 0.7));
  a = streak * w * step(h2, Amount * 1.2) * 0.55;
} else if (mode == 1) {
  float n1 = K.n(float2(UV.x * 5.0, UV.y * 2.5 - T * 0.05));
  float n2 = K.n(float2(UV.x * 70.0, UV.y * 3.0 + T * 0.6));
  a = (0.5 + 0.5 * n1) * (0.82 + 0.18 * n2) * pow(facing, 0.8);
} else if (mode == 2) {
  float sw = K.n(float2(UV.x * 7.0 + T * 1.6 + UV.y * 4.0, UV.y * 16.0 - T * 0.9));
  float sw2 = K.n(float2(UV.x * 23.0 + T * 2.4, UV.y * 40.0));
  a = (0.45 + 0.4 * sw + 0.15 * sw2) * pow(facing, 0.5);
  c *= 0.8 + 0.35 * sw;
} else {
  float q = abs(UV.y - 0.5) * 2.0;
  float core = exp(-q * q * 5.0);
  a = core;
  c = lerp(c, float3(1.0, 1.0, 1.0), core * core);
}
a *= VA * Amount;
// a little haze with distance instead of the height fog (which would hide a shaft 8 km off completely)
float hz = saturate(PD / 60000.0) * 0.45;
c = lerp(c, float3(0.62, 0.68, 0.74), hz);
a *= 1.0 - hz * 0.5;
float fade = saturate((SD - PD) / max(Fade, 1.0));
return float4(c * Bright / max(Ex, 1e-6), saturate(a * fade));
"""


def _rain(mat):
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    # no height fog: the haze would swallow a rain shaft or a bolt a few km off whole (the material fades them itself)
    mat.set_editor_property("use_translucency_vertex_fog", False)
    F4 = unreal.CustomMaterialOutputType.CMOT_FLOAT4
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1200, 0)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 120)
    tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1200, 240)
    ex = mel.create_material_expression(mat, unreal.MaterialExpressionEyeAdaptation, -1200, 360)
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1200, 480)
    cv = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -1200, 600)
    sd = mel.create_material_expression(mat, unreal.MaterialExpressionSceneDepth, -1200, 720)
    pd = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1200, 840)
    sdm = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1000, 720)
    sdm.set_editor_property("r", True); sdm.set_editor_property("g", False)
    sdm.set_editor_property("b", False); sdm.set_editor_property("a", False)
    _link(sd, "", sdm, "")
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1200, 960)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(0.7, 0.73, 0.78, 1))
    pins = [("VA", vc), ("UV", uv), ("T", tm), ("Ex", ex), ("N", vn), ("CV", cv), ("SD", sdm), ("PD", pd), ("Tint", tint)]
    y = 1080
    for nm, dv in (("Mode", 0.0), ("Amount", 0.0), ("Bright", 0.5), ("Fade", 100.0)):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1200, y)
        e.set_editor_property("parameter_name", nm)
        e.set_editor_property("default_value", dv)
        pins.append((nm, e))
        y += 100
    c = _custom(mat, -600, 0, RAIN, F4, [n for n, _ in pins], "Rain")
    for n, e in pins:
        if n == "VA":
            _link(e, "A", c, n)
        else:
            _link(e, "", c, n)
    rgb = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -300, 0)
    rgb.set_editor_property("r", True); rgb.set_editor_property("g", True); rgb.set_editor_property("b", True); rgb.set_editor_property("a", False)
    _link(c, "", rgb, "")
    al = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -300, 150)
    al.set_editor_property("r", False); al.set_editor_property("g", False); al.set_editor_property("b", False); al.set_editor_property("a", True)
    _link(c, "", al, "")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(al, "", unreal.MaterialProperty.MP_OPACITY)


BUILDERS = {
    "M_KR_VertexColor": _vertex_color, "M_KR_Water": _water, "M_KR_Ocean": _ocean, "M_KR_Solid": _solid,
    "M_KR_Seafloor": _seafloor, "M_KR_Reef": _reef, "M_KR_ReefCard": _reef_card, "M_KR_Underwater": _underwater,
    "M_KR_WaterUnder": _water_under, "M_KR_Bubble": _bubble, "M_KR_Dash": _dash, "M_KR_Glass": _glass,
    "M_KR_GirlSkin": _girl(0), "M_KR_GirlHair": _girl(1), "M_KR_GirlSuit": _girl(2),
    "M_KR_Cloud": _cloud, "M_KR_Rain": _rain, "M_KR_Fish": _fish,
}


def _make_map():
    if eal.does_asset_exist(MAP_PATH):
        return
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les.new_level(MAP_PATH):
        les.save_current_level()
        unreal.log("Keys Run: created " + MAP_PATH)


def _setup(_delta):
    unreal.unregister_slate_post_tick_callback(_handle)
    try:
        made = False
        for name, build in BUILDERS.items():
            made = _make_material(name, build) or made
        _make_map()
        if made:
            unreal.log("Keys Run: setup done. Press Play.")
    except Exception as e:
        unreal.log_error("Keys Run setup failed: " + str(e))


# only in the editor itself: a -game launch or a commandlet has no asset tools, and touching them there crashes
_cmd = unreal.SystemLibrary.get_command_line().lower()
_handle = None
_args = _cmd.split()
# (the command line may start with -game itself: the project path isn't always passed through)
if "-game" not in _args and "-server" not in _args and not any(a.startswith("-run=") for a in _args):
    _handle = unreal.register_slate_post_tick_callback(_setup)
