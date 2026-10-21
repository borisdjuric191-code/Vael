# Makes the lagoons of the Pfuhlmoor in L_Wurzelforst_Blockout a murky, overgrown oasis with a hint of danger.
#
# Reads the kit's moor files (Blockout/Vael_Wurzelforst_Blockout/wf_moor.py writes them, with the ground height of every
# place from the height map: the commandlet doesn't load all landscape cells, so traces would miss) and builds:
# - Moor water: one plane over the whole moor with the new material M_WF_Moorwasser, lying just above the sea plane.
#   Its mask T_WF_Moor says where the lagoons are, how deep (dark olive at the shore, near black teal in the middle),
#   where duckweed and algae float and where the deep holes are. Slow waves, and now and then rings spread
#   on the water from nothing, more of them over the deep holes.
# - Rembrandt water lilies in loose rafts (the grey box lily discs are switched off, not deleted).
# - Reed belts along the shore with gaps (dry Megascans stalks and thatching grass, a few whole Megaplant reeds and
#   bulrushes), Matted Pratia and horsetail on the wet banks, burdock a little higher up, bog marshcress in the shallows,
#   swamp cypress root knots at the shore, black alders on the banks.
# - Clear water around the mangrove feet (T_WF_MoorKlar), so their roots show under the surface (BuildMangroven.py places them).
# - Dead wood: dead pines standing in the water, fallen trunks half sunk from the bank into the water, stumps and roots.
# - Low mist over every lagoon (local fog volumes) and a few pale will-o'-the-wisp lights over the deep holes.
# - Lurking eyes (AVaelLurkingEyes) at the holes and in deep water: they surface now and then and sink when someone comes close.
# - Shallow water (AVaelGroundArea, water, invisible disc) over the lagoons: whoever wades in gets wet, and the mage
#   can draw water there.
# Everything it adds is tagged WF_Moor (Outliner folder Wurzelforst/2_Pfuhlmoor/Moorwasser); rerunning replaces only that.
#
# Run with the editor closed, after the kit's wf_moor.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildMoorWasser.py" -EnablePlugins=PythonScriptPlugin

import json
import math
import os
import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
KIT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Blockout", "Vael_Wurzelforst_Blockout")
ASSET_FOLDER = "/Game/Vael/Wurzelforst/Blockout"
MATERIAL_FOLDER = ASSET_FOLDER + "/Materialien"
TAG = "WF_Moor"
HIDE_TAG = "WF_MoorVerdeckt"
FOLDER = "Wurzelforst/2_Pfuhlmoor/Moorwasser"
SEED = 1313
PLANE = "/Engine/BasicShapes/Plane.Plane"
WAVE_NORMAL = "/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n.water_n"
WATER_LIFT = 6.0  # cm above the sea plane, whose surface wobbles a few cm
MEGAPLANT_REEDS = 80  # whole skinned reed plants are costly: at most this many, the rest are stalks

FAB = "/Game/Fab/Megascans/Plants"
MEGASCANS = "/Game/Megascans"
PACKS = {
    "lily": FAB + "/Rembrandt_Water_Lily_uc2kajgia",
    "pratia": FAB + "/Matted_Pratia_uegjcflia",
    "klette": FAB + "/Greater_Burdock_xh5pdgiqx",
    "kresse": FAB + "/Bog_Marshcress_theqfivr",
    "schachtelhalm": "/Game/Fab/Horsetail_grass",
    "zypresse": "/Game/Fab/Photorealistic_Swamp_Cypress_Roots_-_Nanite_Ready__Free_",
}
ALDERS = ["/Game/BlackAlder/Geometry/SimpleWind/SM_BlackAlder_Forest_{:02d}".format(i) for i in range(1, 10)]
YOUNG_ALDERS = ["/Game/BlackAlder/Geometry/SimpleWind/SM_BlackAlder_Sapling_{:02d}".format(i) for i in range(1, 7)]
HORSETAIL_TEXTURE = "/Game/Fab/Horsetail_grass/horsetail_grass/Textures/horsetail_grass_texture_0"
BULRUSH = ["/Game/Megaplant_Library/Plant_Bulrush/Plant_Bulrush_01/Plant_Common_Bulrush_01_" + v for v in "ABCD"]
STALKS = [MEGASCANS + "/3D_Plants/DryPlantSetB/SM_DryPlantSet01_V09", MEGASCANS + "/3D_Plants/DryPlantSetB/SM_DryPlantSet01_V10",
          MEGASCANS + "/3D_Plants/DryPlantSetB/SM_DryPlantSet01_V11", MEGASCANS + "/3D_Plants/DryPlantSetB/SM_DryPlantSet01_V12",
          MEGASCANS + "/3D_Plants/ThatchingGrass/SM_ThatchingGrass00_V10", MEGASCANS + "/3D_Plants/ThatchingGrass/SM_ThatchingGrass00_V2",
          MEGASCANS + "/3D_Plants/DryPlantSetB/SM_DryPlantSet01_V01"]
REEDS = ["/Game/Megaplant_Library/Plant_Dry_Common_Reeds/Plant_Dry_Common_Reeds_01/Plant_Dry_Common_Reed_" + v for v in "ABCD"]
DEAD_PINES = ["/Game/Megaplant_Library/Tree_Dead_Pine/Tree_Dead_Pine_01/Tree_Dead_Pine_" + v for v in "ABCDE"]
FALLEN = [MEGASCANS + "/3D_Assets/FallenFirTree/SM_FallenFirTree", MEGASCANS + "/3D_Assets/FallenPineTree/SM_FallenPineTree",
          MEGASCANS + "/3D_Assets/TreeLog/SM_TreeLog", MEGASCANS + "/3D_Assets/MossyLog/SM_MossyLog"]
STUMPS = [MEGASCANS + "/3D_Assets/BrokenPineStump/SM_BrokenPineStump", MEGASCANS + "/3D_Assets/LichenStump/SM_LichenStump",
          MEGASCANS + "/3D_Assets/BrokenTreeStump/SM_BrokenTreeStump", MEGASCANS + "/3D_Assets/OldStump/SM_OldStump"]
ROOTS = [MEGASCANS + "/3D_Assets/ForestRootsA/SM_ForestRootsA", MEGASCANS + "/3D_Assets/ForestRootsB/SM_ForestRootsB"]

# Ring ripples from nothing: cells of 9 m, each with its own rhythm; a few cells per moment send out a ring.
# Out: float2 slope added to the wave normal.
RIPPLE_HLSL = """
float2 res = 0;
float cell = 900.0;
float2 c = floor(Pos / cell);
for (int i = -1; i <= 1; i++)
{
    for (int j = -1; j <= 1; j++)
    {
        float2 cc = c + float2(i, j);
        float r1 = frac(sin(dot(cc, float2(127.1, 311.7))) * 43758.5453);
        float r2 = frac(sin(dot(cc, float2(269.5, 183.3))) * 43758.5453);
        float period = 4.0 + 6.0 * r2;
        float cycle = T / period + r1;
        float t = frac(cycle);
        float pick = frac(sin(floor(cycle) * 12.9898 + r1 * 78.233) * 43758.5453);
        if (pick < Chance)
        {
            float2 centre = (cc + 0.15 + 0.7 * float2(r1, r2)) * cell;
            float2 d = Pos - centre;
            float dist = length(d);
            float x = (dist - t * 380.0) / 20.0;
            float ring = exp(-x * x) * (1.0 - t) * (1.0 - t);
            res += (d / max(dist, 1.0)) * sin(x * 2.5) * ring;
        }
    }
}
return res;
"""

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
notes = []


def log(message):
    unreal.log_warning("[BuildMoorWasser] " + message)


def try_set(target, key, value):
    try:
        target.set_editor_property(key, value)
        return True
    except Exception as error:  # property names differ between engine versions
        notes.append("{}.{} not set: {}".format(type(target).__name__, key, error))
        return False


# ---------------------------------------------------------------- Materials

class Graph:
    """Places material nodes in columns and checks every link (as in BuildUrbaum.py)"""

    def __init__(self, material):
        self.material = material
        self.rows = {}

    def node(self, expression_class, column, **properties):
        row = self.rows.get(column, 0)
        self.rows[column] = row + 1
        expression = library.create_material_expression(self.material, expression_class, -300 * column, 140 * row)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def link(self, source, target, target_input="", source_output=""):
        if not library.connect_material_expressions(source, source_output, target, target_input):
            notes.append("link failed: {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))

    def scalar(self, name, value, column):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def const(self, value, column):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def color(self, name, rgb, column):
        parameter = self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name,
                              default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(parameter, column - 1)

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def mul(self, column, a, b):
        return self.op(unreal.MaterialExpressionMultiply, column, a, b)

    def add(self, column, a, b):
        return self.op(unreal.MaterialExpressionAdd, column, a, b)

    def sub(self, column, a, b):
        return self.op(unreal.MaterialExpressionSubtract, column, a, b)

    def mask(self, source, column, r=True, g=True, b=True, a=False):
        expression = self.node(unreal.MaterialExpressionComponentMask, column, r=r, g=g, b=b, a=a)
        self.link(source, expression)
        return expression

    def lerp(self, a, b, alpha, column):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression

    def saturate(self, source, column):
        expression = self.node(unreal.MaterialExpressionSaturate, column)
        self.link(source, expression)
        return expression


def new_material(name):
    path = "{}/{}".format(MATERIAL_FOLDER, name)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
        return material
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def import_mask(name="T_WF_Moor", compression=unreal.TextureCompressionSettings.TC_MASKS):
    """A map of the kit, imported anew on every run (linear; the moor map keeps its four channels apart)"""
    source = os.path.join(KIT, name + ".png")
    if not os.path.isfile(source):
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", ASSET_FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(ASSET_FOLDER + "/" + name)
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", compression)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", True)
    assets.save_loaded_asset(texture, False)
    return texture


def build_water_material(mask_texture, origin, size, clear_texture=None):
    """Murky moor water over the lagoons: colour by depth, near black holes, duckweed floating on top, slow waves and rings"""
    material = new_material("M_WF_Moorwasser")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    try_set(material, "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    g = Graph(material)

    world = g.node(unreal.MaterialExpressionWorldPosition, 12)
    world_xy = g.mask(world, 11, r=True, g=True, b=False)
    origin_xy = g.mask(g.node(unreal.MaterialExpressionVectorParameter, 12, parameter_name="MaskOrigin",
                              default_value=unreal.LinearColor(origin[0], origin[1], 0.0, 0.0)), 11, r=True, g=True, b=False)
    uv = g.op(unreal.MaterialExpressionDivide, 10, g.sub(10, world_xy, origin_xy), g.scalar("MaskSize", size, 11))
    sample = g.node(unreal.MaterialExpressionTextureSample, 9, texture=mask_texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    g.link(uv, sample, "UVs")
    depth = g.mask(sample, 8, r=True, g=False, b=False)
    weed = g.mask(sample, 8, r=False, g=True, b=False)
    hole = g.mask(sample, 8, r=False, g=False, b=True)
    # (the sample's default output is RGB: the water channel comes from its own A output)
    water_here = g.node(unreal.MaterialExpressionComponentMask, 8, r=True, g=False, b=False, a=False)
    g.link(sample, water_here, "", "A")

    # Colour: olive brown at the shore, dark teal in the middle, almost black over the holes
    water = g.lerp(g.color("Seicht", (0.05, 0.048, 0.018), 8), g.color("Tief", (0.008, 0.014, 0.007), 8), depth, 6)
    water = g.lerp(water, g.color("Loch", (0.001, 0.004, 0.005), 7), hole, 5)
    # Duckweed and algae: the map's patches, frayed by noise, in two greens
    fine = g.node(unreal.MaterialExpressionNoise, 8, scale=0.02, levels=3, output_min=0.0, output_max=1.0,
                  noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX)
    g.link(world, fine)
    fray = g.sub(7, g.mul(8, fine, g.const(0.6, 9)), g.const(0.55, 8))  # -0.55..0.05: bare water stays bare, the patch edges fray
    weed_alpha = g.saturate(g.mul(6, g.add(7, weed, fray), g.scalar("AlgenDichte", 2.2, 7)), 5)
    weed_color = g.lerp(g.color("AlgeDunkel", (0.03, 0.07, 0.008), 8), g.color("AlgeHell", (0.12, 0.2, 0.02), 8), fine, 6)
    base = g.lerp(water, weed_color, weed_alpha, 4)

    # Waves: two slow layers of the water normal map; rings from nothing on top, stronger over the holes; none in the duckweed
    wave_texture = unreal.load_asset(WAVE_NORMAL)
    wave_uv = g.mul(9, world_xy, g.const(1.0 / 420.0, 10))
    time = g.node(unreal.MaterialExpressionTime, 10)
    samples = []
    for scale, direction in ((1.0, (0.012, 0.004)), (1.8, (-0.007, 0.01))):
        drift = g.mul(8, time, g.node(unreal.MaterialExpressionConstant2Vector, 9, r=direction[0], g=direction[1]))
        layer_uv = g.add(7, g.mul(8, wave_uv, g.const(scale, 9)), drift)
        layer = g.node(unreal.MaterialExpressionTextureSample, 6, texture=wave_texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        g.link(layer_uv, layer, "UVs")
        samples.append(layer)
    waves = g.add(5, samples[0], samples[1])
    flat = g.node(unreal.MaterialExpressionConstant3Vector, 5, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    calm = g.mul(5, g.scalar("Wellen", 0.22, 6), g.sub(6, g.const(1.0, 7), weed_alpha))
    wave_normal = g.lerp(flat, waves, calm, 4)

    ripple = g.node(unreal.MaterialExpressionCustom, 6)
    ripple.set_editor_property("code", RIPPLE_HLSL)
    ripple.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    ripple.set_editor_property("description", "Ringe")
    inputs = []
    for name in ("Pos", "T", "Chance"):
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    ripple.set_editor_property("inputs", inputs)
    g.link(world_xy, ripple, "Pos")
    g.link(time, ripple, "T")
    g.link(g.scalar("RingeHaeufig", 0.2, 7), ripple, "Chance")
    ring_strength = g.mul(4, g.add(5, g.scalar("Ringe", 0.35, 6), g.mul(6, hole, g.const(0.6, 7))), g.sub(5, g.const(1.0, 6), weed_alpha))
    ring_xy = g.mul(3, ripple, ring_strength)
    ring3 = g.node(unreal.MaterialExpressionAppendVector, 3)
    g.link(ring_xy, ring3, "A")
    g.link(g.const(0.0, 4), ring3, "B")
    normal = g.node(unreal.MaterialExpressionNormalize, 2)
    g.link(g.add(2, wave_normal, ring3), normal)

    # Murky: hardly see-through even at the shore; soft where it meets ground, logs and plants
    contact = g.node(unreal.MaterialExpressionDepthFade, 4)
    g.link(g.scalar("Kontakt", 25.0, 5), contact, "FadeDistance")
    body = g.lerp(g.scalar("TruebUfer", 0.88, 6), g.const(0.96, 6), depth, 5)
    opacity = g.mul(2, g.mul(3, g.lerp(body, g.const(1.0, 4), weed_alpha, 3), water_here), contact)
    if clear_texture is not None:
        # Clear around the mangrove feet: the roots show under the surface, the water there a little greener
        clear_sample = g.node(unreal.MaterialExpressionTextureSample, 9, texture=clear_texture,
                              sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
        g.link(uv, clear_sample, "UVs")
        clear = g.mul(4, g.mask(clear_sample, 8, r=True, g=False, b=False), g.sub(5, g.const(1.0, 6), weed_alpha))
        opacity = g.lerp(opacity, g.mul(2, opacity, g.scalar("KlarDeckkraft", 0.72, 3)), clear, 1)
        base = g.lerp(base, g.color("KlarFarbe", (0.012, 0.03, 0.018), 4), g.mul(3, clear, g.const(0.6, 4)), 2)

    roughness = g.lerp(g.scalar("Glanz", 0.25, 4), g.const(0.75, 4), weed_alpha, 3)
    for expression, target in ((base, unreal.MaterialProperty.MP_BASE_COLOR), (normal, unreal.MaterialProperty.MP_NORMAL),
                               (opacity, unreal.MaterialProperty.MP_OPACITY), (roughness, unreal.MaterialProperty.MP_ROUGHNESS),
                               (g.scalar("Spiegelung", 0.1, 3), unreal.MaterialProperty.MP_SPECULAR)):
        if not library.connect_material_property(expression, "", target):
            notes.append("could not connect {}".format(target))
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_horsetail_material():
    """The horsetail pack comes with the engine's unlit glTF material: an own lit one with its texture"""
    source = unreal.load_asset(HORSETAIL_TEXTURE)
    if source is None:
        return None
    material = new_material("M_WF_Schachtelhalm")
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)
    color = g.mul(2, g.mask(g.node(unreal.MaterialExpressionTextureSample, 4, texture=source), 3), g.color("Farbe", (0.85, 0.95, 0.8), 3))
    library.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.const(0.8, 2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_cypress_material():
    """The cypress root pack's material sits on the engine's FBX material: an own one with the pack's textures"""
    folder = PACKS["zypresse"]
    base_color, normal, rough = (unreal.load_asset(folder + "/T_SwampCypress_" + suffix) for suffix in ("BC", "N", "R"))
    if base_color is None:
        return None
    material = new_material("M_WF_Zypressenwurzel")
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)
    library.connect_material_property(g.mask(g.node(unreal.MaterialExpressionTextureSample, 4, texture=base_color), 3), "", unreal.MaterialProperty.MP_BASE_COLOR)
    if normal is not None:
        library.connect_material_property(g.node(unreal.MaterialExpressionTextureSample, 3, texture=normal, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
                                          "RGB", unreal.MaterialProperty.MP_NORMAL)
    if rough is not None:
        library.connect_material_property(g.mask(g.node(unreal.MaterialExpressionTextureSample, 4, texture=rough,
                                                        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR), 3, g=False, b=False),
                                          "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_eye_material():
    """Eyes of something in the water: pale yellow green glow, faded by the actor through Glow"""
    material = new_material("M_WF_Nixenauge")
    g = Graph(material)
    glow = g.mul(2, g.mul(3, g.color("Augenfarbe", (0.75, 1.0, 0.25), 5), g.scalar("Leuchten", 10.0, 4)), g.scalar("Glow", 1.0, 4))
    library.connect_material_property(g.const(0.0, 2), "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(g.const(0.1, 2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


# ---------------------------------------------------------------- Meshes and placement

def pack_meshes(folder):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = [d.get_asset() for d in registry.get_assets_by_path(folder, recursive=True)
             if str(d.asset_class_path.asset_name) == "StaticMesh"]
    if not found:
        notes.append("no meshes in " + folder)
    return sorted(found, key=lambda m: m.get_name())


def load_all(paths):
    loaded = []
    for path in paths:
        asset = unreal.load_asset(path)
        if asset is None:
            notes.append("missing " + path)
        else:
            loaded.append(asset)
    return loaded


def allow_instancing(meshes):
    for mesh in meshes:
        for slot in mesh.static_materials:
            if slot.material_interface is None:
                continue
            base = slot.material_interface.get_base_material()
            # Only project materials: engine and plugin content must never be changed (give such meshes an own material)
            if not base.get_path_name().startswith("/Game/"):
                notes.append("not instancing-enabled (outside /Game): " + base.get_path_name())
                continue
            if not base.get_editor_property("used_with_instanced_static_meshes"):
                library.set_material_usage(base, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
                library.recompile_material(base)
                assets.save_loaded_asset(base, False)
                notes.append("instancing allowed on " + base.get_name())


class Placer:
    """Collects instances per mesh and spawns one AVaelScatterField per mesh at the end"""

    def __init__(self):
        self.instances = {}
        self.settings = {}

    def add(self, kind, mesh, location, rotation, scale, shadow=True, material=None):
        key = mesh.get_path_name()
        self.instances.setdefault(key, (mesh, kind, []))[2].append(unreal.Transform(location, rotation, unreal.Vector(scale, scale, scale)))
        self.settings[key] = (shadow, material)

    def spawn(self):
        total = 0
        for key, (mesh, kind, transforms) in sorted(self.instances.items()):
            field = actors.spawn_actor_from_class(unreal.VaelScatterField, transforms[0].translation, unreal.Rotator(0.0, 0.0, 0.0))
            shadow, material = self.settings[key]
            field.setup_field(mesh, 0.0, shadow, False)
            if material is not None:
                field.instances.set_material(0, material)
            field.add_scatter_instances(transforms)
            field.set_actor_label("Moor_{}_{}".format(kind, mesh.get_name()))
            field.set_folder_path(FOLDER)
            field.set_editor_property("tags", [unreal.Name(TAG)])
            total += len(transforms)
        return total


def tag(actor, label, folder=FOLDER):
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    return actor


def mesh_height(mesh):
    box = mesh.get_bounding_box()
    return max(box.max.z - box.min.z, 1.0)


def spawn_skeletal(mesh, location, yaw, scale, label):
    actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, location, unreal.Rotator(0.0, 0.0, yaw))
    component = actor.skeletal_mesh_component
    if not try_set(component, "skeletal_mesh_asset", mesh):
        component.set_skeletal_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    return tag(actor, label)


def main():
    data_file = os.path.join(KIT, "WF_Moor.json")
    if not os.path.isfile(data_file):
        log("WF_Moor.json missing: run wf_moor.py in the kit first")
        return
    data = json.load(open(data_file, encoding="utf-8"))
    water_z = data["wasserspiegel_cm"]

    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    level_actors = actors.get_all_level_actors()

    removed = 0
    for actor in level_actors:
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1
    # The grey box lily discs make way for the real lilies: switched off, not deleted
    hidden = 0
    for actor in level_actors:
        if actor.get_actor_label().startswith("Seerose_") and unreal.Name(HIDE_TAG) not in actor.tags:
            actor.root_component.set_visibility(False, True)
            actor.set_actor_hidden_in_game(True)
            actor.set_actor_enable_collision(False)
            actor.set_editor_property("tags", list(actor.tags) + [unreal.Name(HIDE_TAG)])
            hidden += 1

    # Water
    mask = import_mask()
    if mask is None:
        log("T_WF_Moor.png missing: run wf_moor.py in the kit first")
        return
    origin = data["maske"]["ursprung_cm"]
    size = data["maske"]["groesse_cm"]
    water_material = build_water_material(mask, origin, size, import_mask("T_WF_MoorKlar", unreal.TextureCompressionSettings.TC_GRAYSCALE))
    plane = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(origin[0] + size / 2, origin[1] + size / 2, water_z + WATER_LIFT),
                                          unreal.Rotator(0.0, 0.0, 0.0))
    component = plane.static_mesh_component
    component.set_static_mesh(unreal.load_asset(PLANE))
    component.set_material(0, water_material)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("cast_shadow", False)
    component.set_editor_property("translucency_sort_priority", 2)
    plane.set_actor_scale3d(unreal.Vector(size / 100.0, size / 100.0, 1.0))
    tag(plane, "Moorwasser_Spiegel")

    meshes = {key: pack_meshes(folder) for key, folder in PACKS.items()}
    stalks = load_all(STALKS)
    fallen = load_all(FALLEN)
    stumps = load_all(STUMPS)
    roots = load_all(ROOTS)
    reeds = load_all(REEDS)
    dead_pines = load_all(DEAD_PINES)
    bulrush = load_all(BULRUSH)
    alders = load_all(ALDERS)
    young_alders = load_all(YOUNG_ALDERS)
    allow_instancing([m for group in meshes.values() for m in group] + stalks + fallen + stumps + roots + alders + young_alders)
    place = Placer()
    counts = {}
    horsetail_material = build_horsetail_material()
    cypress_material = build_cypress_material()

    def count(kind):
        counts[kind] = counts.get(kind, 0) + 1

    # Lilies float on the water, their stems hanging below
    for x, y, _, scale, yaw, pick in data["seerosen"]:
        if meshes["lily"]:
            place.add("Seerose", meshes["lily"][pick % len(meshes["lily"])], unreal.Vector(x, y, water_z + WATER_LIFT + 1.0),
                      unreal.Rotator(0.0, 0.0, yaw), scale, shadow=False)
            count("Seerosen")

    # Reed belts: stalks everywhere, whole reed plants here and there
    whole = 0
    for x, y, z, size_value, yaw, kind in data["schilf"]:
        if kind == "megaplant" and reeds and whole < MEGAPLANT_REEDS:
            # Every other whole plant a bulrush with its brown cobs
            if bulrush and whole % 2 == 1:
                spawn_skeletal(rng.choice(bulrush), unreal.Vector(x, y, z - 5.0), yaw, size_value * 1.4, "Moor_Rohrkolben_{:03d}".format(whole + 1))
            else:
                spawn_skeletal(rng.choice(reeds), unreal.Vector(x, y, z - 5.0), yaw, size_value, "Moor_Schilf_{:03d}".format(whole + 1))
            whole += 1
            count("Schilfpflanzen")
        elif stalks:
            # A clump of stalks around each spot, so the belt reads as reed from the camera
            for _ in range(rng.randint(3, 6)):
                mesh = stalks[rng.randrange(len(stalks))]
                height = (size_value if kind != "megaplant" else rng.uniform(150, 230)) * rng.uniform(0.8, 1.15)
                a, r = rng.uniform(0, 2 * math.pi), rng.uniform(0, 130)
                place.add("Halme", mesh, unreal.Vector(x + math.cos(a) * r, y + math.sin(a) * r, z - 4.0),
                          unreal.Rotator(rng.uniform(-8, 8), rng.uniform(-8, 8), rng.uniform(0, 360)), height / mesh_height(mesh), shadow=True)
                count("Halme")

    # Banks
    for x, y, z, scale, yaw, kind in data["ufer"]:
        if not meshes.get(kind):
            continue
        mesh = meshes[kind][rng.randrange(len(meshes[kind]))]
        place.add(kind.capitalize(), mesh, unreal.Vector(x, y, z - 2.0), unreal.Rotator(0.0, 0.0, yaw), scale,
                  shadow=(kind == "klette"), material=horsetail_material if kind == "schachtelhalm" else None)
        count(kind)

    # Bog marshcress in the shallows: rooted on the floor, its leaves just above the surface
    for x, y, z, scale, yaw in data.get("kresse", []):
        if meshes["kresse"]:
            mesh = meshes["kresse"][rng.randrange(len(meshes["kresse"]))]
            place.add("Kresse", mesh, unreal.Vector(x, y, max(z, water_z - 12.0)), unreal.Rotator(0.0, 0.0, yaw), scale, shadow=False)
            count("Sumpfkresse")

    # Swamp cypress root knots at the shore and around the mangrove feet, half in the water
    for x, y, z, scale, yaw in data.get("zypressenwurzeln", []):
        if meshes["zypresse"]:
            place.add("Zypressenwurzel", meshes["zypresse"][0], unreal.Vector(x, y, z - 25.0 * scale),
                      unreal.Rotator(rng.uniform(-6, 6), rng.uniform(-6, 6), yaw), scale, material=cypress_material)
            count("Zypressenwurzeln")

    # Black alders on the banks, young ones nearer the water
    for x, y, z, scale, yaw, kind in data.get("erlen", []):
        pool = alders if kind == "erle" else young_alders
        if pool:
            place.add("Erle", pool[rng.randrange(len(pool))], unreal.Vector(x, y, z - 10.0), unreal.Rotator(0.0, 0.0, yaw), scale)
            count("Erlen" if kind == "erle" else "junge Erlen")

    # Dead wood
    pines = 0
    for wood in data["totholz"]:
        x, y, z = wood["ort"]
        kind, yaw, chance = wood["art"], wood["drehung"], wood["zufall"]
        if kind == "toter_baum" and dead_pines:
            pines += 1
            spawn_skeletal(dead_pines[int(chance * len(dead_pines)) % len(dead_pines)], unreal.Vector(x, y, z - 50.0), yaw,
                           0.55 + chance * 0.35, "Moor_ToteKiefer_{:02d}".format(pines))
            count("tote Kiefern")
        elif kind == "liegender_stamm" and fallen:
            mesh = fallen[int(chance * len(fallen)) % len(fallen)]
            box = mesh.get_bounding_box()
            scale = 1.2 + chance * 0.8
            length = (box.max.x - box.min.x) * scale
            # From the bank into the water: the middle a third of its length out, the water end sunk deeper
            direction = unreal.Vector(math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0)
            middle = unreal.Vector(x, y, 0.0) + direction * (length * 0.3)
            place.add("Stamm", mesh, unreal.Vector(middle.x, middle.y, min(z, water_z) - 35.0 * scale),
                      unreal.Rotator(rng.uniform(-4, 4), -rng.uniform(3, 7), yaw), scale)
            count("liegende Stämme")
        elif kind == "stumpf" and stumps:
            mesh = stumps[int(chance * len(stumps)) % len(stumps)]
            place.add("Stumpf", mesh, unreal.Vector(x, y, z - 15.0), unreal.Rotator(rng.uniform(-8, 8), rng.uniform(-8, 8), yaw), 1.0 + chance * 1.2)
            count("Stümpfe")
        elif kind == "wurzeln" and roots:
            place.add("Wurzeln", roots[int(chance * len(roots)) % len(roots)], unreal.Vector(x, y, z - 5.0),
                      unreal.Rotator(0.0, 0.0, yaw), 1.5 + chance * 1.0)
            count("Wurzeln")

    total = place.spawn()

    # Mist lying low over every lagoon
    mists = 0
    for i, mist in enumerate(data["nebel"]):
        x, y, _ = mist["ort"]
        sx, sy = mist["groesse_cm"]
        fog = actors.spawn_actor_from_class(unreal.LocalFogVolume, unreal.Vector(x, y, water_z + 60.0), unreal.Rotator(0.0, 0.0, 0.0))
        # The volume is a sphere of 1 m radius at scale 1: as wide as the lagoon, its height fog keeps it low
        fog.set_actor_scale3d(unreal.Vector(max(sx, 2000.0) / 200.0 * 1.1, max(sy, 2000.0) / 200.0 * 1.1, 6.0))
        component = fog.get_component_by_class(unreal.LocalFogVolumeComponent)
        if component is not None:
            try_set(component, "radial_fog_extinction", 0.0)
            try_set(component, "height_fog_extinction", 0.01)
            try_set(component, "height_fog_falloff", 1200.0)
            try_set(component, "height_fog_offset", 0.0)
            try_set(component, "fog_albedo", unreal.LinearColor(0.62, 0.7, 0.62, 1.0))
        tag(fog, "Moor_Nebel_{}".format(i + 1))
        mists += 1

    # Lurking eyes, and pale wisps over the holes
    eye_material = build_eye_material()
    eyes = 0
    wisps = 0
    for i, (x, y, _) in enumerate(data["lauerstellen"]):
        actor = actors.spawn_actor_from_class(unreal.VaelLurkingEyes, unreal.Vector(x, y, water_z + WATER_LIFT), unreal.Rotator(0.0, 0.0, rng.uniform(0, 360)))
        actor.set_editor_property("eye_material", eye_material)
        tag(actor, "Moor_Augen_{:02d}".format(i + 1))
        eyes += 1
        if i % 3 == 0:
            light = actors.spawn_actor_from_class(unreal.VaelFireLight, unreal.Vector(x + rng.uniform(-400, 400), y + rng.uniform(-400, 400), water_z + 90.0),
                                                  unreal.Rotator(0.0, 0.0, 0.0))
            light.set_editor_property("base_intensity", 18.0)
            light.set_editor_property("flicker_strength", 0.55)
            light.set_editor_property("flicker_speed", 1.2)
            point = light.get_component_by_class(unreal.PointLightComponent)
            point.set_editor_property("light_color", unreal.Color(r=150, g=255, b=170, a=255))
            point.set_editor_property("attenuation_radius", 700.0)
            point.set_editor_property("cast_shadows", False)
            tag(light, "Moor_Irrlicht_{:02d}".format(wisps + 1))
            wisps += 1

    # Shallow water: wet, and a water source for spells; the disc of each area stays invisible
    radius = data["seichtwasser_radius_cm"]
    areas = 0
    for x, y, _ in data["seichtwasser"]:
        area = actors.spawn_actor_from_class(unreal.VaelGroundArea, unreal.Vector(x, y, water_z + 2.0), unreal.Rotator(0.0, 0.0, 0.0))
        area.set_editor_property("element", unreal.VaelElement.WATER)
        area.set_editor_property("radius", radius)
        area.set_editor_property("extinguishable", False)
        disc = area.get_component_by_class(unreal.StaticMeshComponent)
        if disc is not None:
            disc.set_editor_property("visible", False)
        tag(area, "Moor_Seichtwasser_{:03d}".format(areas + 1))
        areas += 1

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("moor built: {} instances ({}), {} reed plants, {} dead pines, {} mists, {} lurking eyes, {} wisps, {} shallow areas, "
        "{} grey box lilies hidden, {} old pieces removed".format(total, ", ".join("{} {}".format(v, k) for k, v in sorted(counts.items())),
                                                               whole, pines, mists, eyes, wisps, areas, hidden, removed))
    for note in notes:
        log("NOTE " + note)


main()
