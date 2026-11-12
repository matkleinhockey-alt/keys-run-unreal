# Runs every time the editor opens this project. The first time, it creates the materials
# and the empty map the C++ game needs, so nothing has to be authored by hand.
import unreal

ROOT = "/Game/KeysRun"
MAT_DIR = ROOT + "/Materials"
MAP_PATH = ROOT + "/Maps/Keys"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def _const(mat, value, x, y):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", value)
    return c


def _make_material(name, build):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        return False
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    build(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log("Keys Run: created " + path)
    return True


def _vertex_color(mat):
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -500, 0)
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_const(mat, 0.75, -500, 220), "", unreal.MaterialProperty.MP_ROUGHNESS)


def _water(mat):
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -500, 0)
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(_const(mat, 0.05, -500, 220), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_const(mat, 0.5, -500, 320), "", unreal.MaterialProperty.MP_SPECULAR)


# The detailed sea. The C++ water mesh supplies the big swells and wakes, the depth color in vertex RGB,
# the foam amount in vertex alpha, and in UV0: x = crest height (for light glowing through thin crests), y = aerated prop wash.
# This material adds what the mesh is too coarse for: wind ripples that catch the sun, foam broken into
# real-looking patches, and turquoise light scattering through the wave tops.
OCEAN_FOAM = """
float2 p = WP.xy / 100.0;
float cover = saturate(A);
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
float m = smoothstep(th, th + 0.1, n) * saturate(cover * 3.0);
// thin lacy edge around each patch
m = max(m, 0.35 * smoothstep(th - 0.08, th, n) * (1.0 - smoothstep(th, th + 0.04, n)) * saturate(cover * 4.0));
// far away the noise would shimmer, so fade to the average
float far = saturate((D - 8000.0) / 40000.0);
return lerp(m, cover * 0.75, far);
"""

OCEAN_NORMAL = """
float2 p = WP.xy / 100.0;
float2 g = float2(0, 0);
// ripples blown by an easterly trade wind: 18 wave trains from 0.2 m to about 15 m long, spread unevenly
// around the wind so they never line up into a visible grid
const float wind = 3.4;
float foot = max(D, 1.0) / 100.0 * 0.0012;   // meters per pixel at this distance
for (int i = 0; i < 18; i++) {
  float fi = (float)i;
  float r1 = frac(sin(fi * 12.9898) * 43758.5453);
  float r2 = frac(sin(fi * 78.233) * 12543.123);
  float ang = wind + (r1 * 2.0 - 1.0) * 1.35;
  float2 dir = float2(cos(ang), sin(ang));
  float L = 0.2 * pow(1.29, fi) * (0.85 + 0.3 * r2);
  float k = 6.2831853 / L;
  float w = sqrt(9.81 * k);
  float steep = 0.055 * (0.6 + 0.8 * r2);
  float aa = saturate(L / (foot * 8.0) - 1.0);   // drop waves too small to see at this distance (no sparkle noise)
  float ph = k * dot(dir, p) - w * T + r1 * 6.2831853 + r2 * 3.1;
  g += dir * (steep * cos(ph)) * aa;
}
float s = Strength * (1.0 - 0.75 * saturate(F)) * (1.0 - 0.5 * saturate(Aer));
return normalize(VN + float3(-g.x, -g.y, 0.0) * s);
"""

OCEAN_BASE = """
// the chart colors are bright display colors; real water is dark and gets most of its brightness from
// reflecting the sky, so deepen and saturate them (pale sand flats stay bright, deep water turns cobalt)
float3 water = pow(saturate(VC), 1.55) * 0.82;
// the prop wash: churned water full of bubbles turns a pale milky turquoise
water = lerp(water, float3(0.2, 0.52, 0.5), saturate(Aer) * 0.55);
float3 foam = float3(0.84, 0.9, 0.93);
return lerp(water, foam, saturate(F));
"""

OCEAN_ROUGH = """
// glassy up close, a little rougher far out (the ripples there are too small to draw but still blur the reflection)
float r = lerp(0.035, 0.14, saturate(D / 120000.0));
return lerp(r, 0.6, saturate(F));
"""

OCEAN_SSS = """
// light glows through the thin tops of swells and wakes
// only near the very top of a wave, where the water is thin enough for sunlight to pass through.
// (In the Subsurface shading model, Opacity 1 means no light gets through, so return 1 minus the glow.)
return 1.0 - saturate((UV.x - 0.4) * 1.6) * 0.55 * (1.0 - saturate(F));
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


def _ocean(mat):
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)
    mat.set_editor_property("tangent_space_normal", False)
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
    mel.connect_material_expressions(uv, "", aer, "")
    strength = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1150, 760)
    strength.set_editor_property("parameter_name", "RippleStrength")
    strength.set_editor_property("default_value", 1.0)

    foam = _custom(mat, -900, 0, OCEAN_FOAM, F1, ["WP", "T", "A", "D"], "Foam")
    mel.connect_material_expressions(wp, "", foam, "WP")
    mel.connect_material_expressions(tm, "", foam, "T")
    mel.connect_material_expressions(vc, "A", foam, "A")
    mel.connect_material_expressions(dp, "", foam, "D")

    nrm = _custom(mat, -500, 300, OCEAN_NORMAL, F3, ["WP", "T", "VN", "D", "F", "Aer", "Strength"], "Ripples")
    for src, pin in ((wp, "WP"), (tm, "T"), (vn, "VN"), (dp, "D"), (foam, "F"), (aer, "Aer"), (strength, "Strength")):
        mel.connect_material_expressions(src, "", nrm, pin)

    base = _custom(mat, -500, 0, OCEAN_BASE, F3, ["VC", "F", "Aer"], "Base color")
    mel.connect_material_expressions(vc, "", base, "VC")
    mel.connect_material_expressions(foam, "", base, "F")
    mel.connect_material_expressions(aer, "", base, "Aer")

    rough = _custom(mat, -500, 150, OCEAN_ROUGH, F1, ["F", "D"], "Roughness")
    mel.connect_material_expressions(foam, "", rough, "F")
    mel.connect_material_expressions(dp, "", rough, "D")

    sss = _custom(mat, -500, 500, OCEAN_SSS, F1, ["UV", "F"], "Crest glow")
    mel.connect_material_expressions(uv, "", sss, "UV")
    mel.connect_material_expressions(foam, "", sss, "F")
    sss_col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, 650)
    sss_col.set_editor_property("constant", unreal.LinearColor(0.04, 0.42, 0.34, 1.0))

    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(_const(mat, 0.35, -500, 800), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(sss, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(sss_col, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)


def _solid(mat):
    col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(0.8, 0.8, 0.8, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 220)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.8)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)


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
        made = _make_material("M_KR_VertexColor", _vertex_color)
        made = _make_material("M_KR_Water", _water) or made
        made = _make_material("M_KR_Ocean", _ocean) or made
        made = _make_material("M_KR_Solid", _solid) or made
        _make_map()
        if made:
            unreal.log("Keys Run: setup done. Press Play.")
    except Exception as e:
        unreal.log_error("Keys Run setup failed: " + str(e))


# only in the editor itself: a -game launch or a commandlet has no asset tools, and touching them there crashes
_cmd = unreal.SystemLibrary.get_command_line().lower()
_handle = None
if " -game" not in _cmd and "-run=" not in _cmd:
    _handle = unreal.register_slate_post_tick_callback(_setup)
