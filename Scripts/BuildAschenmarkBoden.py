# The ground of the Aschenmark without visible repetition: M_AM_Boden made anew from eight layers
# (Tools/Welt/aschenmark_boden.py: Asche, rissige Erde, Schlacke, Fels, Kies, Moos, verbranntes Gras, Weg), packed
# into two texture arrays (albedo + height, normal + roughness + occlusion).
#
# - Hex tiling: the ground is cut into small triangles of about 3 m; every corner turns and shifts the layers at random,
#   the three corners are blended, so no tile ever repeats in a row.
# - Layers by zone (T_AM_Wege B), slope, three large noises and the roads (T_AM_Wege R, the walked roads of
#   aschenmark_pfade.py); between layers the higher surface wins (height blend), so edges look like real ground.
# - Colour drifts over 50 to 150 m; glowing cracks in the Glutfelder follow the cracks of the earth layer.
#
# Run with the editor closed, after the Blender script:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildAschenmarkBoden.py" -EnablePlugins=PythonScriptPlugin -unattended -AllowCommandletRendering

import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Aschenmark/boden/export"
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
WORLD_FOLDER = "/Game/Vael/Welt"
FOLDER = WORLD_FOLDER + "/Boden"
LAYERS = 8
M = 100.0
SIZE_X, SIZE_Y = 2017, 2521
KIT_VERSION = 3.0
# The displacement is scaled by the landscape scale (100 in X, Y and Z): 0.12 = 12 cm
DISPLACEMENT_CM = 0.12

assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(message):
    unreal.log_warning("[BuildAschenmarkBoden] " + message)


def import_picture(filename, folder, name, srgb):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(folder + "/" + name)
    texture.set_editor_property("srgb", srgb)
    return texture


def texture_array(name, textures):
    path = FOLDER + "/" + name
    if assets.does_asset_exist(path):
        array = unreal.load_asset(path)
    else:
        array = tools.create_asset(name, FOLDER, unreal.Texture2DArray, unreal.Texture2DArrayFactory())
    if not unreal.VaelWorldBuilder.fill_texture_array(array, textures):
        log("could not fill " + name)
    assets.save_loaded_asset(array, False)
    return array


class Graph:
    def __init__(self, material):
        self.material = material
        self.row = 0

    def node(self, cls, **props):
        self.row += 1
        e = library.create_material_expression(self.material, cls, -1200 + 300 * (self.row % 4), 110 * self.row)
        for k, val in props.items():
            e.set_editor_property(k, val)
        return e

    def link(self, a, b, b_input="", a_output=""):
        if not library.connect_material_expressions(a, a_output, b, b_input):
            log("link failed {} -> {}".format(a.get_name(), b_input))

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=value)

    def mask(self, source, channels, output=""):
        e = self.node(unreal.MaterialExpressionComponentMask, r="R" in channels, g="G" in channels, b="B" in channels, a="A" in channels)
        self.link(source, e, "", output)
        return e

    def noise(self, world, metres, levels=3):
        e = self.node(unreal.MaterialExpressionNoise, scale=1.0 / (metres * M), noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU,
                      turbulence=False, levels=levels, output_min=0.0, output_max=1.0, level_scale=2.0, quality=1)
        self.link(world, e, "Position")
        return e


# Layers: 0 Asche, 1 rissige Erde, 2 Schlacke, 3 Fels, 4 Kies, 5 Moos, 6 verbranntes Gras, 7 Weg
GROUND = r"""
struct VaelHexGrid
{
    float2 Hash(float2 p)
    {
        p = float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)));
        return frac(sin(p) * 43758.5453);
    }
    // Triangle grid of the hex tiling (Mikkelsen): the three corners of the triangle and their weights
    void Corners(float2 uv, out float3 w, out float2 v1, out float2 v2, out float2 v3)
    {
        uv *= 3.4641016;
        float2 sk = float2(uv.x, -0.57735027 * uv.x + 1.15470054 * uv.y);
        float2 b = floor(sk);
        float2 f = frac(sk);
        float z = 1.0 - f.x - f.y;
        if (z > 0.0) { w = float3(z, f.y, f.x); v1 = b; v2 = b + float2(0, 1); v3 = b + float2(1, 0); }
        else { w = float3(-z, 1.0 - f.y, 1.0 - f.x); v1 = b + 1.0; v2 = b + float2(1, 0); v3 = b + float2(0, 1); }
    }
};
VaelHexGrid Hex;

float Way = Masks.r, Clear = Masks.g, Zone = Masks.b;
float zK = saturate(1.0 - Zone * 3.0);
float zS = saturate(1.0 - abs(Zone - 0.333) * 3.0);
float zG = saturate(1.0 - abs(Zone - 0.667) * 3.0);
float zP = saturate(Zone * 3.0 - 2.0);
float Steep = smoothstep(0.92, 0.72, Up);
float Free = 1.0 - smoothstep(0.25, 0.85, Way);

float W[8];
W[0] = (0.95 * zK + 0.5 * zS + 0.55 * zG + 0.45 * zP + 0.6 * Clear) * (0.75 + 0.5 * N2);
W[1] = (0.45 * zK + 1.2 * zG + 0.2 * zS) * smoothstep(0.3, 0.75, N1);
W[2] = 0.95 * zS * smoothstep(0.25, 0.7, N2) + 0.35 * zG * smoothstep(0.5, 0.9, N2);
W[3] = Steep * 1.6 + 0.4 * zP * smoothstep(0.45, 0.85, N1) + 0.25 * zS * smoothstep(0.6, 0.9, N3);
W[4] = 0.25 * smoothstep(0.5, 0.9, N3) + 0.25 * zS + 0.3 * zP * smoothstep(0.35, 0.8, N3);
W[5] = 0.95 * zP * smoothstep(0.4, 0.8, N2) * (1.0 - Steep);
W[6] = 0.45 * zK * smoothstep(0.45, 0.85, N3) + 0.25 * zG * smoothstep(0.6, 0.95, N1);
for (int i = 0; i < 7; i++) { W[i] *= Free; }
W[3] = max(W[3], Steep * 1.6);
W[7] = smoothstep(0.25, 0.85, Way) * 2.0 * (1.0 - Steep);

float Scale[8] = { 4.0, 3.5, 3.0, 6.0, 2.5, 3.0, 3.5, 3.0 };
float3 HW;
float2 V[3];
Hex.Corners(UV / 3.0, HW, V[0], V[1], V[2]);
HW = pow(HW, 4.0);
HW /= (HW.x + HW.y + HW.z);
float Corner[3] = { HW.x, HW.y, HW.z };

float4 Bh[8];
float4 Nr[8];
for (int l = 0; l < 8; l++) { Bh[l] = 0; Nr[l] = 0; }
float2 Dx = ddx(UV), Dy = ddy(UV);
for (int k = 0; k < 3; k++)
{
    float2 h = Hex.Hash(V[k]);
    float a = h.x * 6.2831853;
    float c = cos(a), s = sin(a);
    float2x2 R = float2x2(c, -s, s, c);
    for (int l = 0; l < 8; l++)
    {
        [branch] if (W[l] > 0.003)
        {
            float2 uv = mul(R, UV / Scale[l]) + h * 7.31;
            float2 dx = mul(R, Dx / Scale[l]), dy = mul(R, Dy / Scale[l]);
            float4 b = BH.SampleGrad(BHSampler, float3(uv, l), dx, dy);
            float4 n = NR.SampleGrad(NRSampler, float3(uv, l), dx, dy);
            float2 nxy = mul(transpose(R), n.xy * 2.0 - 1.0);
            Bh[l] += b * Corner[k];
            Nr[l] += float4(nxy, n.zw) * Corner[k];
        }
    }
}

// Height blend: where a layer is strong and high it wins; the edge between two layers follows their surfaces
float Score[8];
float Best = -10.0;
for (int l = 0; l < 8; l++) { Score[l] = W[l] > 0.003 ? Bh[l].a * 0.5 + W[l] * 1.6 : -10.0; Best = max(Best, Score[l]); }
float Total = 0.0;
float F[8];
for (int l = 0; l < 8; l++) { F[l] = pow(max(Score[l] - Best + 0.6, 0.0), 1.5); Total += F[l]; }
float3 Colour = 0;
float2 Nxy = 0;
float Rough = 0, Occ = 0;
for (int l = 0; l < 8; l++)
{
    float f = F[l] / max(Total, 1e-4);
    Colour += Bh[l].rgb * f;
    Nxy += Nr[l].xy * f;
    Rough += Nr[l].z * f;
    Occ += Nr[l].w * f;
}

// Loose ash lies over everything flat, thicker in drifts; less of it on the roads and on steep rock
float Drift = smoothstep(0.35, 0.8, N2 * 0.6 + N3 * 0.4) * (1.0 - Steep) * (1.0 - 0.6 * smoothstep(0.25, 0.85, Way)) * (1.0 - zP * 0.5);
float3 LooseAsh = float3(0.34, 0.325, 0.31) * lerp(0.8, 1.05, N3) * lerp(0.85, 1.0, Bh[0].a);
Colour = lerp(Colour, LooseAsh, Drift * 0.45);
Rough = lerp(Rough, 0.95, Drift * 0.5);
// Colour drifts over 50 to 150 m: lighter and darker fields, warmer and cooler stretches
float3 Warm = float3(1.05, 1.0, 0.92), Cool = float3(0.93, 0.96, 1.03);
Colour *= lerp(0.8, 1.15, N0) * lerp(Warm, Cool, N4);
Colour *= lerp(float3(1, 1, 1), float3(1.06, 0.95, 0.88), zG * 0.7) * lerp(float3(1, 1, 1), float3(0.92, 0.94, 0.98), zS * 0.6);

// Real relief for the Nanite displacement: the blended heights of the layers, and in sand and ash small ripples the wind
// has drawn, all running the same way with a slow bend, broken up by noise; loose ash drifts fill the hollows a little
float Height = 0.0;
for (int l = 0; l < 8; l++) { Height += Bh[l].a * F[l] / max(Total, 1e-4); }
float Soft = saturate((F[0] + F[7] * 0.6 + F[6] * 0.4) / max(Total, 1e-4) + Drift * 0.5);
float2 RippleDir = normalize(float2(0.8, 0.6) + float2(N4 - 0.5, N0 - 0.5) * 0.9);
float Wave = lerp(0.24, 0.46, N2);
float RipplePhase = dot(UV, RippleDir) * 6.2831853 / Wave + N1 * 14.0 + N3 * 6.0;
float Ripple = (sin(RipplePhase) * 0.5 + 0.5);
// a weaker second set across, and the strength coming and going in patches, so it never looks combed
float2 CrossDir = float2(-RippleDir.y, RippleDir.x) * 0.6 + RippleDir * 0.8;
float Cross = sin(dot(UV, normalize(CrossDir)) * 6.2831853 / (Wave * 1.7) + N0 * 11.0) * 0.5 + 0.5;
float RippleAmount = (0.45 + 0.55 * smoothstep(0.2, 0.7, N1 * 0.6 + N4 * 0.4)) * (1.0 - smoothstep(0.6, 0.95, N3) * 0.6);
Ripple = lerp(0.5, Ripple * Ripple * 0.75 + Cross * 0.25, RippleAmount);
float Grain = Bh[0].a;
float Relief = lerp(Height, Height * 0.45 + Ripple * 0.4 + Grain * 0.15, Soft) * (1.0 - smoothstep(0.25, 0.85, Way) * 0.55);
// Footprints from the track map (UVaelFootprintSubsystem): R depth, G the clock when made; deep in ash and sand, shallow
// on earth, hardly on rock and roads; they fade over FadeSecs
float Inside = all(TrailUV > 0.0) && all(TrailUV < 1.0) ? 1.0 : 0.0;
float Texel = 1.0 / 2048.0;
float4 Tr = Trail.SampleLevel(TrailSampler, TrailUV, 0);
float Age = frac(Clock - Tr.g) * 600.0;
float Print = Tr.r * saturate(1.0 - Age / FadeSecs) * Inside;
float Give = saturate(0.15 + 0.85 * Soft + 0.3 * F[1] / max(Total, 1e-4)) * (1.0 - Steep) * (1.0 - smoothstep(0.4, 0.9, Way) * 0.7);
float PrintR = Trail.SampleLevel(TrailSampler, TrailUV + float2(Texel, 0), 0).r;
float PrintU = Trail.SampleLevel(TrailSampler, TrailUV + float2(0, Texel), 0).r;
Relief -= Print * Give * 0.45;
Colour *= 1.0 - Print * Give * 0.35;
Rough = lerp(Rough, 1.0, Print * Give * 0.5);
// a dent: the height is -Print, so the normal leans towards where the print gets deeper
Nxy += float2(PrintR - Tr.r, PrintU - Tr.r) * Give * saturate(1.0 - Age / FadeSecs) * Inside * 0.6;
HeightOut = saturate(Relief);
// The ripples also bend the light, so they read from above even where the relief is flat
float RippleSlope = cos(RipplePhase) * Soft * RippleAmount * 0.3;
Nxy += RippleDir * RippleSlope;
NormalOut = float3(Nxy, sqrt(saturate(1.0 - dot(Nxy, Nxy))));
// Ash and sand never shine: the rippled normals caught the low sun like wet sand
RoughOut = max(Rough, 0.82);
OccOut = Occ;
// Glowing cracks in the Glutfelder: deep in the cracks of the earth layer, in patches
float Crack = pow(saturate(1.0 - Bh[1].a * 1.8), 3.0) * F[1] / max(Total, 1e-4);
GlowOut = Crack * zG * Free * (1.0 - Clear * 0.8) * smoothstep(0.6, 0.78, N3) * smoothstep(0.5, 0.7, N2);
return Colour;
"""


def track_resources():
    """RT_AM_Spuren (+ the spare one for moving it), the print shapes and MPC_AM_Spuren for UVaelFootprintSubsystem"""
    for name in ("RT_AM_Spuren", "RT_AM_Spuren_Umzug"):
        path = FOLDER + "/" + name
        target = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
            name, FOLDER, unreal.TextureRenderTarget2D, unreal.TextureRenderTargetFactoryNew())
        target.set_editor_property("size_x", 2048)
        target.set_editor_property("size_y", 2048)
        target.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA16F)
        target.set_editor_property("clear_color", unreal.LinearColor(0, 0, 0, 0))
        target.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        target.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        assets.save_loaded_asset(target, False)
    shapes = import_picture(os.path.join(SOURCE, "T_AM_Fussabdruecke.png"), FOLDER, "T_AM_Fussabdruecke", False)
    shapes.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    shapes.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    shapes.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assets.save_loaded_asset(shapes, False)
    path = FOLDER + "/MPC_AM_Spuren"
    collection = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        "MPC_AM_Spuren", FOLDER, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    vector = unreal.CollectionVectorParameter()
    vector.set_editor_property("parameter_name", "SpurenUrsprung")
    vector.set_editor_property("default_value", unreal.LinearColor(0, 0, 4800, 0))
    scalar = unreal.CollectionScalarParameter()
    scalar.set_editor_property("parameter_name", "SpurenUhr")
    scalar.set_editor_property("default_value", 0.0)
    collection.set_editor_property("vector_parameters", [vector])
    collection.set_editor_property("scalar_parameters", [scalar])
    assets.save_loaded_asset(collection, False)


def build_material(bh_array, nr_array, masks):
    path = WORLD_FOLDER + "/M_AM_Boden"
    material = unreal.load_asset(path)
    library.delete_all_material_expressions(material)
    g = Graph(material)
    g.node(unreal.MaterialExpressionScalarParameter, parameter_name="KitVersion", default_value=KIT_VERSION)
    world = g.node(unreal.MaterialExpressionWorldPosition)

    # Masks over the whole land: u runs east (world Y), v north (world X)
    mask_u, mask_v = g.node(unreal.MaterialExpressionDivide), g.node(unreal.MaterialExpressionDivide)
    g.link(g.mask(world, "G"), mask_u, "A")
    g.link(g.const((SIZE_Y - 1) * M), mask_u, "B")
    g.link(g.mask(world, "R"), mask_v, "A")
    g.link(g.const((SIZE_X - 1) * M), mask_v, "B")
    mask_uv = g.node(unreal.MaterialExpressionAppendVector)
    g.link(mask_u, mask_uv, "A")
    g.link(mask_v, mask_uv, "B")
    masks_sample = g.node(unreal.MaterialExpressionTextureSample, texture=masks, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(mask_uv, masks_sample, "UVs")

    coords = g.node(unreal.MaterialExpressionLandscapeLayerCoords, mapping_scale=1.0)
    up = g.mask(g.node(unreal.MaterialExpressionVertexNormalWS), "B")
    bh = g.node(unreal.MaterialExpressionTextureObjectParameter, parameter_name="Schichten", texture=bh_array,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    nr = g.node(unreal.MaterialExpressionTextureObjectParameter, parameter_name="SchichtenNormal", texture=nr_array,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    # The track map: its place and size and the clock from MPC_AM_Spuren, the map itself as a texture
    spuren = unreal.load_asset(FOLDER + "/MPC_AM_Spuren")
    origin = g.node(unreal.MaterialExpressionCollectionParameter, collection=spuren, parameter_name="SpurenUrsprung")
    clock = g.node(unreal.MaterialExpressionCollectionParameter, collection=spuren, parameter_name="SpurenUhr")
    trail_offset = g.node(unreal.MaterialExpressionSubtract)
    g.link(g.mask(world, "RG"), trail_offset, "A")
    g.link(g.mask(origin, "RG"), trail_offset, "B")
    trail_uv = g.node(unreal.MaterialExpressionDivide)
    g.link(trail_offset, trail_uv, "A")
    g.link(g.mask(origin, "B"), trail_uv, "B")
    trail = g.node(unreal.MaterialExpressionTextureObjectParameter, parameter_name="Spuren", texture=unreal.load_asset(FOLDER + "/RT_AM_Spuren"),
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    inputs = {"UV": coords, "Masks": masks_sample, "Up": up, "BH": bh, "NR": nr,
              "Trail": trail, "TrailUV": trail_uv, "Clock": clock, "FadeSecs": g.const(100.0),
              "N0": g.noise(world, 70.0, 2), "N1": g.noise(world, 22.0, 3), "N2": g.noise(world, 38.0, 3),
              "N3": g.noise(world, 11.0, 2), "N4": g.noise(world, 150.0, 2)}
    custom = g.node(unreal.MaterialExpressionCustom, code=GROUND, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                    description="Aschenmark ground")
    entries = []
    for name in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        entries.append(entry)
    custom.set_editor_property("inputs", entries)
    outputs = []
    for name, kind in (("NormalOut", unreal.CustomMaterialOutputType.CMOT_FLOAT3), ("RoughOut", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                       ("OccOut", unreal.CustomMaterialOutputType.CMOT_FLOAT1), ("GlowOut", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                       ("HeightOut", unreal.CustomMaterialOutputType.CMOT_FLOAT1)):
        out = unreal.CustomOutput()
        out.set_editor_property("output_name", name)
        out.set_editor_property("output_type", kind)
        outputs.append(out)
    custom.set_editor_property("additional_outputs", outputs)
    library.update_material_expression_custom(custom) if hasattr(library, "update_material_expression_custom") else None
    for name, source in inputs.items():
        g.link(source, custom, name)

    library.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(custom, "NormalOut", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(custom, "RoughOut", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(g.const(0.25), "", unreal.MaterialProperty.MP_SPECULAR)
    library.connect_material_property(custom, "OccOut", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    glow = g.node(unreal.MaterialExpressionMultiply)
    g.link(custom, glow, "A", "GlowOut")
    g.link(g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(4.0, 1.3, 0.28, 1.0)), glow, "B")
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Nanite displacement: 12 cm of relief, the middle stays where the landscape is
    if not unreal.VaelWorldBuilder.connect_displacement(material, custom, "HeightOut", DISPLACEMENT_CM, 0.5):
        log("displacement could not be connected")

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


LEVEL = "/Game/Vael/Maps/L_Vael"
DECAL_TAG = "VaelBodenAbdruck"
# name: glows, (depth, width, length) factor of the size
DECALS = {"Brandfleck": False, "Glutriss": True, "Aschewehe": False, "Lehmboden": False}


def decal_material(name, texture, glows):
    path = FOLDER + "/M_AM_" + name
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_AM_" + name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g = Graph(material)
    sample = g.node(unreal.MaterialExpressionTextureSample, texture=texture)
    library.connect_material_property(sample, "A", unreal.MaterialProperty.MP_OPACITY)
    if glows:
        # A crack: scorched dark round a thin glowing seam
        library.connect_material_property(g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.035, 0.03, 0.028, 1.0)), "",
                                          unreal.MaterialProperty.MP_BASE_COLOR)
        glow = g.node(unreal.MaterialExpressionMultiply)
        g.link(sample, glow, "A", "R")
        g.link(g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(9.0, 2.6, 0.5, 1.0)), glow, "B")
        library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        library.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.const(0.92), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def lay_decals():
    """The marks of AM_Abdruecke.json as decals in L_Vael; the marks of the last run go first"""
    import json
    marks = json.load(open(os.path.join(DATA, "AM_Abdruecke.json"), encoding="utf-8"))["abdruecke"]
    materials = {}
    for name, glows in DECALS.items():
        texture = import_picture(os.path.join(SOURCE, "D_{}.png".format(name)), FOLDER, "T_AM_" + name, True)
        assets.save_loaded_asset(texture, False)
        materials[name] = decal_material(name, texture, glows)
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
        log("could not load " + LEVEL)
        return
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old = [a for a in actors.get_all_level_actors() if unreal.Name(DECAL_TAG) in a.get_editor_property("tags")]
    for actor in old:
        actors.destroy_actor(actor)
    for mark in marks:
        size = mark["groesse"] * M
        decal = actors.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(mark["x"] * M, mark["y"] * M, mark["z"] * M),
                                              unreal.Rotator(0.0, -90.0, mark["yaw"]))
        component = decal.decal
        component.set_decal_material(materials[mark["art"]])
        long_side = size * (0.5 if mark["art"] != "Aschewehe" else 0.5)
        short_side = size * (0.5 if mark["art"] == "Brandfleck" else 0.28)
        component.set_editor_property("decal_size", unreal.Vector(250.0, short_side, long_side))
        component.set_editor_property("sort_order", 1 if mark["art"] == "Glutriss" else 0)
        decal.set_folder_path("Boden/Abdruecke/" + mark["art"])
        decal.set_actor_label(mark["art"])
        decal.set_editor_property("tags", [unreal.Name(DECAL_TAG)])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("{} old marks removed, {} laid".format(len(old), len(marks)))


def nanite_landscape():
    """Skipped with VAEL_NANITE_FERTIG=1 once it is on (the material alone changed).
    The landscape of L_Vael as Nanite, so the displacement of the ground material really moves it"""
    if os.environ.get("VAEL_NANITE_FERTIG") == "1":
        return
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
        log("could not load " + LEVEL)
        return
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    count = unreal.VaelWorldBuilder.enable_landscape_nanite(world)
    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("Nanite on for {} landscape actors".format(count))


def main():
    bh, nr = [], []
    for index in range(LAYERS):
        bh.append(import_picture(os.path.join(SOURCE, "L{}_BH.png".format(index)), FOLDER, "T_AM_Schicht{}_BH".format(index), True))
        nr.append(import_picture(os.path.join(SOURCE, "L{}_NR.png".format(index)), FOLDER, "T_AM_Schicht{}_NR".format(index), False))
    for texture in bh + nr:
        assets.save_loaded_asset(texture, False)
    bh_array = texture_array("T_AM_Boden_BH", bh)
    nr_array = texture_array("T_AM_Boden_NR", nr)
    log("arrays: {} x {}, {} x {}".format(bh_array.get_name(), len(bh), nr_array.get_name(), len(nr)))

    masks = import_picture(os.path.join(DATA, "T_AM_Wege.png"), WORLD_FOLDER, "T_AM_Wege", False)
    masks.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    masks.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    masks.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    masks.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assets.save_loaded_asset(masks, False)

    track_resources()
    material = build_material(bh_array, nr_array, masks)
    log("material {} built, kit {}".format(material.get_name(), KIT_VERSION))
    nanite_landscape()
    lay_decals()


main()
