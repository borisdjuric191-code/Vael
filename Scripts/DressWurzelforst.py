# Brings textures into the Wurzelforst blockout (region II), like DressAschenmark did for Aschenmark.
#
# - The ground (M_WF_Zonenkarte, which wf_blockout.py lays on the landscape) is rebuilt: forest floor laid by world
#   position (each layer sampled twice, turned, against the tile look), mud at the waterline, rock on steep slopes,
#   green meadow and moss everywhere in the forest, all tinted by the zone map so the regions stay readable (the
#   Forstsaum's olive zone colour pulled towards green: Boris found the floor too brown, 2026-10-28). Hain (and partly Kammweg and moor) get the Fab root moss floor,
#   the Forstsaum a meadow of uncut grass with clover patches.
# - Plants grow by themselves near the camera (landscape grass, GT_WF_* in .../Blockout/Gras): ferns, sorrel and balsam
#   in Hain, Saum, Moor and Kammweg, reeds along the shore, dry grass at the ash edge. The ground material says where
#   (zone, not on the path, not on steep or wet ground). Needs grass.GrassMap.UseRuntimeGeneration=1 (DefaultEngine.ini).
#   Master materials of these plants get the instancing usage, otherwise the game shows them grey.
# - Small winding trails with the root moss floor and dense thickets around the trunks and along the trail edges, from
#   the kit's T_WF_Pfade.png / T_WF_Dickicht.png (wf_pfade.py), imported into .../Blockout on every run.
# - The campaign path (Boris 2026-10-28): no more grey box plates (Weg_*, switched off, not deleted) but the same root
#   floor as the trails along a gently winding line (T_WF_Hauptweg.png), 2.5-4.5 m wide and uneven, trodden down the
#   middle, with a green moss seam on both sides and ferns crowding it (thicket band from wf_pfade.py).
# - The grey box master M_WF_Graubox gets a texture laid over the world (bark, rock, moss ...); every MI_WF_* instance
#   picks its texture and how strongly it shows ("Muster", 0 = plain colour as before).
# - The level template's own sun, sky light, atmosphere and fog (folder "Lighting") are switched off, the blockout brings its own.
# - The sea gets MI_WF_Wasser_Teich: the pond water, darker and with waves sized for the 4 km water plane.
# - A post process volume "Stimmung_Wurzelforst" lowers the exposure (once; tune it by hand).
# Both materials keep their names and parameters, so rerunning wf_blockout.py keeps the textures.
# Only assets under /Game/Vael/Wurzelforst/Blockout, the water plane and the template lights of L_Wurzelforst_Blockout are touched.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/DressWurzelforst.py" -EnablePlugins=PythonScriptPlugin

import os

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
MATERIAL_FOLDER = "/Game/Vael/Wurzelforst/Blockout/Materialien"
ZONE_TEXTURE = "/Game/Vael/Wurzelforst/Blockout/T_WF_Zonenkarte"
WATER_PARENT = "/Game/VFX/Water/Materials/MI_Water_Pond"
WATER_NAME = "MI_WF_Wasser_Teich"
WATER_ACTOR = "Wasserspiegel_Ringsee_Westmeer"
TAG = "WF_Blockout"
OFF_TAG = "WF_VorlageAus"
DRESS_TAG = "WF_Kulisse"  # not WF_Blockout, so rerunning wf_blockout.py keeps it
MOOD_LABEL = "Stimmung_Wurzelforst"
DOUBLED = (unreal.DirectionalLight, unreal.SkyLight, unreal.SkyAtmosphere, unreal.ExponentialHeightFog)

# Landscape from WF_Hoehenkarte_2017.png: lower corner and size of the zone map in cm
MAP_ORIGIN = (-201600.0, -201600.0)
MAP_SIZE = 2017 * 200.0

TEXTURES = {
    "forest": "/Game/Megascans/Surfaces/ForestGround/T_ForestGround_D",
    "forest_normal": "/Game/Megascans/Surfaces/ForestGround/T_ForestGround_N",
    "mud": "/Game/Megascans/Surfaces/HeavyMud/T_HeavyMud_D",
    "sand": "/Game/MS/Surfaces/Dry_Sandy_Mud_wdunfjf/T_Dry_Sandy_Mud_wdunfjf_D",
    "rock": "/Game/Rock_Collection_04/Meshes/Rock_01/Textures/T_Rock_01_A",
    "bark": "/Game/Stylized_Tree_Pack/Textures/Stylized_Bark_Texture/T_Stylized_Bark_1_COLOR",
    "moss": "/Game/Megascans/3D_Assets/IcelandicMossClusters/T_IcelandicMossClusters_A",
    "rootmoss": "/Game/Fab/Megascans/Surfaces/Nordic_Forest_Ground_Root_Moss_Coarse_xiekec0/Low/xiekec0_tier_3/Textures/T_xiekec0_1K_B",
    "rootmoss_normal": "/Game/Fab/Megascans/Surfaces/Nordic_Forest_Ground_Root_Moss_Coarse_xiekec0/Low/xiekec0_tier_3/Textures/T_xiekec0_1K_N",
    "grass": "/Game/Fab/Megascans/Surfaces/Uncut_Grass_oeeb70/Low/oeeb70_tier_3/Textures/T_oeeb70_1K_B",
    "grass_normal": "/Game/Fab/Megascans/Surfaces/Uncut_Grass_oeeb70/Low/oeeb70_tier_3/Textures/T_oeeb70_1K_N",
    "clover": "/Game/Fab/Megascans/Surfaces/Clover_Patches_on_Grass_sgmkajak/Raw/sgmkajak_tier_0/Textures/T_sgmkajak_8K_B",
    "clover_normal": "/Game/Fab/Megascans/Surfaces/Clover_Patches_on_Grass_sgmkajak/Raw/sgmkajak_tier_0/Textures/T_sgmkajak_8K_N",
}

# Zone colours of T_WF_Zonenkarte.png (wf_generator.py); the ground and the plants read the zones from them
ZONE_COLORS = {
    "saum": "6e6448", "moor": "3e6b55", "hain": "3f7f45", "kamm": "3b6670", "grat": "6a6d60", "asche": "7a4a2a",
    "jaw": "4a5f78", "grat_hoch": "9a9c90", "lagune": "2f6f74", "nid": "5a3a7a", "weg": "b9a77a",
}
PLANT_ZONES = ("hain", "saum", "moor", "kamm", "asche")

# Plants the landscape grows by itself near the camera (LandscapeGrassType per zone).
# Per mesh group: density per 100 m2, scale range, cast shadow
GRASS_FOLDER = "/Game/Vael/Wurzelforst/Blockout/Gras"
FERN = "/Game/Fab/Megascans/Plants/Lady_Fern_wdvlditia/Raw/wdvlditia_tier_0/StaticMeshes/SM_wdvlditia_Var"
SORREL = "/Game/Fab/Megascans/Plants/Wood_Sorrel_wcwmchzja/Raw/wcwmchzja_tier_0/StaticMeshes/SM_wcwmchzja_Var"
BALSAM = "/Game/Fab/Megascans/Plants/Small_Balsam_xefmcgrqx/Raw/xefmcgrqx_tier_0/StaticMeshes/SM_xefmcgrqx_Var"
VIOLET = "/Game/Fab/Megascans/Plants/Violet_Wood_Sorrel_uchkajuia/Low/uchkajuia_tier_3/StaticMeshes/SM_uchkajuia_Var"
IVY = "/Game/Fab/Megascans/Plants/English_Ivy_xfcnebhqx/Low/xfcnebhqx_tier_3/StaticMeshes/SM_xfcnebhqx_Var"
ELDER = "/Game/Fab/Megascans/Plants/Elderberry_wfzobb2ia/Raw/wfzobb2ia_tier_0/StaticMeshes/SM_wfzobb2ia_Var"
PALM = "/Game/Fab/Megascans/Plants/Alexandra_Palm_ukgkcilia/Low/ukgkcilia_tier_3/StaticMeshes/SM_ukgkcilia_Var"
REED = "/Game/Megaplant_Library/Plant_Dry_Common_Reeds/Instances/"
THATCH = "/Game/Megascans/3D_Plants/ThatchingGrass/"
GRASS_MESHES = {
    "fern": [FERN + v for v in "ABCDEFGHI"],
    "fern_big": [FERN + v for v in "AEFH"],
    "violet": [VIOLET + v for v in "ABCDEFGH"],
    "ivy": [IVY + v for v in "AEFG"],  # the flat ground runners (B, C, D climb)
    "elder": [ELDER + v for v in "ACDEFG"],
    "palm": [PALM + v for v in "ABCD"],
    "sorrel": [SORREL + v for v in "ACDEFGH"],
    "balsam": [BALSAM + v for v in "BCDEFGH"],
    "reed": [REED + "Leaf_Dry_Common_Reed_0{}".format(i) for i in range(10)],
    "reed_flower": [REED + "Flower_Dry_Common_Reed_01", REED + "Flower_Dry_Common_Reed_02"],
    "thatch": [THATCH + "SM_ThatchingGrass00_V{}".format(v) for v in (2, 6, 10)],
    "dry": ["/Game/Megascans/3D_Plants/DryPlantSetB/SM_DryPlantSet01_V0{}".format(v) for v in (1, 2, 6)]
           + ["/Game/Megascans/3D_Plants/DryPlant/SM_DryPlant00_V1"],
}
GRASS_TYPES = {
    "Hain": [("fern", 22.0, (0.9, 1.5), True), ("sorrel", 50.0, (1.0, 1.8), False), ("violet", 30.0, (1.4, 2.4), False),
             ("balsam", 18.0, (1.0, 1.7), False), ("ivy", 14.0, (1.0, 1.6), False), ("elder", 1.2, (1.0, 1.8), True),
             ("palm", 0.5, (1.0, 1.5), True)],
    "Saum": [("thatch", 45.0, (0.8, 1.3), False), ("sorrel", 25.0, (1.0, 1.6), False), ("balsam", 16.0, (1.0, 1.6), False),
             ("violet", 12.0, (1.4, 2.2), False), ("fern", 4.0, (0.7, 1.2), True), ("elder", 0.6, (1.0, 1.6), True)],
    "Moor": [("fern", 6.0, (0.8, 1.3), True), ("sorrel", 20.0, (1.0, 1.6), False), ("violet", 10.0, (1.4, 2.2), False),
             ("ivy", 10.0, (1.0, 1.5), False)],
    "Kamm": [("balsam", 14.0, (1.0, 1.7), False), ("sorrel", 30.0, (1.0, 1.6), False), ("violet", 18.0, (1.4, 2.4), False),
             ("fern", 7.0, (0.8, 1.3), True), ("elder", 0.8, (1.0, 1.6), True)],
    # Dense undergrowth around the trunks and along the trail edges (T_WF_Dickicht from wf_pfade.py)
    "Dickicht": [("fern_big", 45.0, (1.3, 2.0), True), ("fern", 45.0, (1.1, 1.7), True), ("elder", 12.0, (1.3, 2.2), True),
                 ("palm", 5.0, (1.1, 1.8), True), ("balsam", 40.0, (1.2, 1.9), False), ("violet", 50.0, (1.6, 2.6), False),
                 ("ivy", 40.0, (1.2, 1.8), False)],
    "Ufer": [("reed", 120.0, (1.6, 2.6), False), ("reed_flower", 25.0, (1.8, 2.8), False)],
    # Ash edge towards the Aschenmark: dry grass and withered stalks, like over there
    "Asche": [("thatch", 30.0, (0.8, 1.2), False), ("dry", 6.0, (0.8, 1.2), False)],
}
GRASS_CULL = (5000, 7000)  # start and end of the fade-out in cm; the shared camera zooms out to 45 m for four players

# Small trails and thickets (Blockout kit, wf_pfade.py): imported as grey textures over the whole landscape
KIT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Blockout", "Vael_Wurzelforst_Blockout")
MASK_FOLDER = "/Game/Vael/Wurzelforst/Blockout"
MASKS = {"trails": ("T_WF_Pfade.png", "T_WF_Pfade"), "thicket": ("T_WF_Dickicht.png", "T_WF_Dickicht"),
         "mainpath": ("T_WF_Hauptweg.png", "T_WF_Hauptweg")}
PATH_PLATE_PREFIX = "Weg_"
PATH_PLATE_FOLDER = "Kampagnenweg"
PATH_OFF_TAG = "WF_WegplatteAus"
TRAIL_RANGE = 1000.0  # cm of distance T_WF_Pfade.png stores as 0..1 (PFAD_REICHWEITE_M)

# How much a texture brightens the colour it lies on (its grey is about 1 / this); the stylized bark is light
BRIGHTEN = {"bark": 1.1}

# Grey box instance: texture, size of one tile in cm, how strongly it shows
SURFACES = {
    "Rinde": ("bark", 250.0, 1.0), "Urstamm": ("bark", 500.0, 1.0), "Totholz": ("bark", 200.0, 1.0),
    "Mangrove": ("bark", 150.0, 1.0), "Holz": ("bark", 120.0, 0.8), "Wurzel": ("bark", 300.0, 1.0),
    "Fels": ("rock", 700.0, 1.0), "FelsMoos": ("rock", 600.0, 1.0), "Zahn": ("rock", 500.0, 0.7),
    "Moos": ("moss", 300.0, 0.8), "Krone": ("moss", 500.0, 0.7), "KroneHell": ("moss", 500.0, 0.7),
    "MangroveKrone": ("moss", 400.0, 0.7), "Seerose": ("moss", 150.0, 0.5),
    "Pilzstiel": ("mud", 200.0, 0.4), "Pilz_Orange": ("mud", 300.0, 0.35), "Pilz_Rot": ("mud", 300.0, 0.35),
    "Pilz_Tuerkis": ("mud", 300.0, 0.35), "Weg": ("forest", 300.0, 1.0),
}

# Colours that differ from what wf_blockout.py gives: the path stripes are brown forest floor, not sand
COLORS = {"Weg": (0.30, 0.21, 0.13)}

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
notes = []

WORLD_ALIGNED = "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"


def log(message):
    unreal.log_warning("[DressWurzelforst] " + message)


class Graph:
    """Small helper that places material nodes in columns and checks every link"""

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

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def mask(self, source, column, r=False, g=False, b=False):
        expression = self.node(unreal.MaterialExpressionComponentMask, column, r=r, g=g, b=b, a=False)
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

    def sample(self, texture, uvs, column):
        # Shared sampler: the ground uses more textures than the 16 own samplers a material may have
        expression = self.node(unreal.MaterialExpressionTextureSample, column, texture=texture,
                               sampler_type=sampler_of(texture), sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        self.link(uvs, expression, "UVs")
        return expression

    def world_aligned(self, texture_object, size, column):
        """Texture projected from the three axes, so walls and trunks don't stretch"""
        call = self.node(unreal.MaterialExpressionMaterialFunctionCall, column)
        call.set_material_function(unreal.load_asset(WORLD_ALIGNED))
        self.link(texture_object, call, "TextureObject")
        size3 = self.op(unreal.MaterialExpressionMultiply, column + 1, size,
                        self.node(unreal.MaterialExpressionConstant3Vector, column + 2, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0)))
        self.link(size3, call, "TextureSize")
        return call


def sampler_of(texture):
    compression = texture.get_editor_property("compression_settings")
    if compression == unreal.TextureCompressionSettings.TC_GRAYSCALE:
        return unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if texture.get_editor_property("srgb") else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    if compression == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    if compression == unreal.TextureCompressionSettings.TC_MASKS:
        return unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property("srgb") else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


def has_parameter(material, name):
    return name in [str(parameter) for parameter in library.get_scalar_parameter_names(material)]


# ---------------------------------------------------------------- Ground

def zone_chroma(name, linear):
    """Zone colour divided by its brightness, so the hill shading baked into the map drops out"""
    hex_color = ZONE_COLORS[name]
    rgb = [int(hex_color[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    if linear:
        rgb = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in rgb]
    total = sum(rgb)
    return [c / total for c in rgb]


def zone_weights(g, zone, linear, column):
    """0..1 per plant zone: 1 near the zone's own colour, 0 halfway to the next zone colour and beyond"""
    total = g.node(unreal.MaterialExpressionDotProduct, column + 1)
    g.link(zone, total, "A")
    g.link(g.node(unreal.MaterialExpressionConstant3Vector, column + 2, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0)), total, "B")
    chroma = g.op(unreal.MaterialExpressionDivide, column, zone,
                  g.op(unreal.MaterialExpressionAdd, column + 1, total, g.const(0.001, column + 2)))
    weights = {}
    for name in PLANT_ZONES + ("jaw",):
        own = zone_chroma(name, linear)
        gap = min(sum((a - b) ** 2 for a, b in zip(own, zone_chroma(other, linear))) ** 0.5 for other in ZONE_COLORS if other != name)
        reach = max(gap * 0.5, 0.01)
        distance = g.node(unreal.MaterialExpressionDistance, column - 1)
        g.link(chroma, distance, "A")
        g.link(g.node(unreal.MaterialExpressionConstant3Vector, column, constant=unreal.LinearColor(own[0], own[1], own[2], 1.0)), distance, "B")
        weights[name] = g.saturate(g.op(unreal.MaterialExpressionSubtract, column - 2, g.const(1.5, column - 1),
                                        g.op(unreal.MaterialExpressionDivide, column - 1, distance, g.const(reach, column))), column - 3)
    return weights


def import_masks():
    """Trail distance and thicket maps from the kit, imported anew on every run (linear grey, no compression)"""
    loaded = {}
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for key, (file_name, asset_name) in MASKS.items():
        source = os.path.join(KIT, file_name)
        if not os.path.isfile(source):
            notes.append("missing {}: run wf_pfade.py in the kit first".format(file_name))
            loaded[key] = None
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", MASK_FOLDER)
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        texture = unreal.load_asset("{}/{}".format(MASK_FOLDER, asset_name))
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        # No mip maps: the grass weights are drawn at 2 m per pixel and would read a blurred mip, where a trail's
        # distance never gets near 0 and the ferns stand on it
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        assets.save_loaded_asset(texture, False)
        loaded[key] = texture
    return loaded


def grass_mesh(path):
    mesh = unreal.load_asset(path)
    if mesh is None:
        notes.append("missing grass mesh " + path)
    return mesh


def allow_instancing(meshes):
    """Grass and scattered pieces are drawn as instances; a master material without that usage shows the grey default
    material in the game (the editor would set the flag by itself when the level is opened). Returns the masters set."""
    changed = []
    for mesh in meshes:
        for slot in mesh.static_materials:
            if slot.material_interface is None:
                continue
            base = slot.material_interface.get_base_material()
            if base.get_editor_property("used_with_instanced_static_meshes"):
                continue
            result = library.set_material_usage(base, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
            if (result[0] if isinstance(result, tuple) else result) or base.get_editor_property("used_with_instanced_static_meshes"):
                library.recompile_material(base)
                assets.save_loaded_asset(base, False)
                changed.append(base.get_name())
    return changed


def make_grass_types():
    """One LandscapeGrassType per zone; the ground material says where each grows"""
    grass_types = {}
    instanced = []
    for name, groups in GRASS_TYPES.items():
        asset_name = "GT_WF_" + name
        path = "{}/{}".format(GRASS_FOLDER, asset_name)
        if assets.does_asset_exist(path):
            grass_type = unreal.load_asset(path)
        else:
            grass_type = unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name, GRASS_FOLDER, unreal.LandscapeGrassType,
                                                                                 unreal.LandscapeGrassTypeFactory())
        varieties = []
        for key, density, scale, shadow in groups:
            meshes = [m for m in (grass_mesh(p) for p in GRASS_MESHES[key]) if m is not None]
            instanced.extend(meshes)
            for mesh in meshes:
                variety = unreal.GrassVariety()
                variety.set_editor_property("grass_mesh", mesh)
                variety.set_editor_property("grass_density", unreal.PerPlatformFloat(default=density / len(meshes)))
                variety.set_editor_property("start_cull_distance", unreal.PerPlatformInt(default=GRASS_CULL[0]))
                variety.set_editor_property("end_cull_distance", unreal.PerPlatformInt(default=GRASS_CULL[1]))
                variety.set_editor_property("scaling", unreal.GrassScaling.UNIFORM)
                variety.set_editor_property("scale_x", unreal.FloatInterval(scale[0], scale[1]))
                variety.set_editor_property("random_rotation", True)
                variety.set_editor_property("align_to_surface", True)
                variety.set_editor_property("placement_jitter", 1.0)
                variety.set_editor_property("cast_dynamic_shadow", shadow)
                variety.set_editor_property("cast_contact_shadow", True)
                varieties.append(variety)
        grass_type.set_editor_property("grass_varieties", varieties)
        assets.save_loaded_asset(grass_type, False)
        grass_types[name] = grass_type
    for master in allow_instancing(instanced):
        notes.append("instancing allowed on " + master)
    return grass_types


def build_ground(material, textures, grass_types, masks):
    library.delete_all_material_expressions(material)
    # The grass output is not removed with the other nodes, and a material may hold only one
    for expression in list(unreal.ObjectIterator(unreal.MaterialExpressionLandscapeGrassOutput)):
        if expression.get_outer() == material:
            library.delete_material_expression(material, expression)
    g = Graph(material)

    position = g.node(unreal.MaterialExpressionWorldPosition, 14)
    plane = g.mask(position, 13, r=True, g=True)
    height = g.mask(position, 13, b=True)

    # Zone map over the whole landscape
    origin = g.mask(g.node(unreal.MaterialExpressionVectorParameter, 13, parameter_name="KartenUrsprung",
                           default_value=unreal.LinearColor(MAP_ORIGIN[0], MAP_ORIGIN[1], 0.0, 0.0)), 12, r=True, g=True)
    zone_uv = g.op(unreal.MaterialExpressionDivide, 11, g.op(unreal.MaterialExpressionSubtract, 12, plane, origin),
                   g.scalar("Kartengroesse", MAP_SIZE, 12))
    zone_texture = unreal.load_asset(ZONE_TEXTURE)
    zone = g.mask(g.sample(zone_texture, zone_uv, 10), 9, r=True, g=True, b=True)
    weights = zone_weights(g, zone, zone_texture.get_editor_property("srgb"), 12)

    # Ground textures laid by world position
    tile = g.scalar("KachelBoden", 400.0, 12)

    def uv(factor):
        size = g.op(unreal.MaterialExpressionMultiply, 11, tile, g.const(factor, 12))
        return g.op(unreal.MaterialExpressionDivide, 10, plane, size)

    # Against the tile look: every layer is sampled twice, once straight and once turned, shifted and a bit larger,
    # and the two blend in irregular patches, so the square repeat of the scan breaks up.
    # Unconnected, the noise nodes read the world position themselves.
    swap_noise = g.node(unreal.MaterialExpressionNoise, 10, scale=0.0011, levels=2, output_min=0.0, output_max=1.0, quality=1)
    swap = g.saturate(g.op(unreal.MaterialExpressionAdd, 8, g.op(unreal.MaterialExpressionMultiply, 9,
                                                                  g.op(unreal.MaterialExpressionSubtract, 10, swap_noise, g.const(0.5, 11)),
                                                                  g.const(3.0, 10)), g.const(0.5, 9)), 7)

    def detiled(key, factor):
        straight = uv(factor)
        turned = g.node(unreal.MaterialExpressionRotator, 9, speed=0.9, center_x=0.5, center_y=0.5)
        g.link(g.op(unreal.MaterialExpressionAdd, 10, g.op(unreal.MaterialExpressionMultiply, 11, straight, g.const(0.79, 12)),
                    g.node(unreal.MaterialExpressionConstant2Vector, 11, r=0.37, g=0.61)), turned, "Coordinate")
        g.link(g.const(1.0, 10), turned, "Time")
        return g.lerp(g.sample(textures[key], straight, 8), g.sample(textures[key], turned, 8), swap, 7), straight

    forest, forest_uv = detiled("forest", 1.0)
    # The heavy mud texture is pale grey: darkened to the wet brown of moor banks at the waterline (the trails keep the raw one)
    raw_mud = g.sample(textures["mud"], uv(0.9), 10)
    mud = g.op(unreal.MaterialExpressionMultiply, 8, g.mask(raw_mud, 9, r=True, g=True, b=True),
               g.mask(g.node(unreal.MaterialExpressionVectorParameter, 10, parameter_name="SchlammFarbe",
                             default_value=unreal.LinearColor(0.42, 0.36, 0.26, 1.0)), 9, r=True, g=True, b=True))
    rock_object = g.node(unreal.MaterialExpressionTextureObject, 10, texture=textures["rock"])
    rock = g.world_aligned(rock_object, g.scalar("KachelFels", 900.0, 11), 8)
    rootmoss, rootmoss_uv = detiled("rootmoss", 0.7)
    grass, meadow_uv = detiled("grass", 0.55)
    clover, clover_uv = detiled("clover", 0.65)

    # Large soft patches so the meadows and moss beds don't look like one carpet
    noise = g.node(unreal.MaterialExpressionNoise, 9, scale=0.0006, levels=3, output_min=0.0, output_max=1.0, quality=1)
    clover_patch = g.saturate(g.op(unreal.MaterialExpressionMultiply, 8, g.op(unreal.MaterialExpressionSubtract, 9, noise, g.const(0.5, 10)),
                                   g.const(5.0, 9)), 7)
    meadow = g.lerp(grass, clover, clover_patch, 7)
    # The uncut grass scan is dry and olive: tinted fresh green, so the forest floor doesn't read brown (Boris 2026-10-28)
    meadow = g.op(unreal.MaterialExpressionMultiply, 6, meadow,
                  g.mask(g.node(unreal.MaterialExpressionVectorParameter, 8, parameter_name="WiesenFarbe",
                                default_value=unreal.LinearColor(0.8, 1.12, 0.62, 1.0)), 7, r=True, g=True, b=True))

    # Green moss beds: the root moss turned green, in patches, mostly in the Hain and the moor
    moss_grey = g.node(unreal.MaterialExpressionDesaturation, 7)
    g.link(rootmoss, moss_grey)
    g.link(g.const(1.0, 8), moss_grey, "Fraction")
    moss_green = g.op(unreal.MaterialExpressionMultiply, 6, moss_grey,
                      g.mask(g.node(unreal.MaterialExpressionVectorParameter, 8, parameter_name="MoosFarbe",
                                    default_value=unreal.LinearColor(0.75, 1.25, 0.42, 1.0)), 7, r=True, g=True, b=True))
    mossy = g.lerp(rootmoss, moss_green, g.scalar("MoosGruen", 0.8, 7), 6)
    zone_moss = g.op(unreal.MaterialExpressionAdd, 8, g.op(unreal.MaterialExpressionMultiply, 9, weights["hain"], g.const(0.6, 10)),
                     g.op(unreal.MaterialExpressionAdd, 9, g.op(unreal.MaterialExpressionMultiply, 10, weights["moor"], g.const(0.5, 11)),
                          g.op(unreal.MaterialExpressionMultiply, 10, weights["kamm"], g.const(0.35, 11))))
    moss_share = g.op(unreal.MaterialExpressionMultiply, 7, g.saturate(zone_moss, 7),
                      g.saturate(g.op(unreal.MaterialExpressionSubtract, 8, g.op(unreal.MaterialExpressionMultiply, 9, noise, g.const(2.2, 10)),
                                      g.const(0.5, 9)), 8))
    green = g.lerp(meadow, mossy, moss_share, 6)

    # The whole forest is green; the brown forest floor is left for the paths, the ash edge, the fangs and the high
    # ridge tops. (Asked by what is NOT green: where two zone colours blend, the mix can look like a third zone.)
    not_green = g.saturate(g.op(unreal.MaterialExpressionAdd, 7, g.op(unreal.MaterialExpressionAdd, 8, weights["asche"], weights["jaw"]),
                                g.saturate(g.op(unreal.MaterialExpressionDivide, 8,
                                                g.op(unreal.MaterialExpressionSubtract, 9, height, g.scalar("GruenBis", 4500.0, 10)),
                                                g.const(1500.0, 9)), 8)), 7)
    green_share = g.op(unreal.MaterialExpressionMultiply, 6, one_minus(g, not_green, 7), g.scalar("GruenAnteil", 1.0, 7))
    plant_zones = g.op(unreal.MaterialExpressionAdd, 7, g.op(unreal.MaterialExpressionAdd, 8, weights["hain"], weights["saum"]),
                       g.op(unreal.MaterialExpressionAdd, 8, weights["moor"], weights["kamm"]))
    # Green ground that belongs to no plant zone (the blends) gets the meadow plants of the Forstsaum
    leftover = g.saturate(g.op(unreal.MaterialExpressionSubtract, 6, green_share, plant_zones), 5)
    ground = g.lerp(forest, green, green_share, 5)

    # Wet mud only right at the waterline
    wet = g.saturate(g.op(unreal.MaterialExpressionDivide, 8,
                          g.op(unreal.MaterialExpressionSubtract, 9, g.scalar("NassBis", 70.0, 10), height),
                          g.const(50.0, 9)), 7)
    ground = g.lerp(ground, mud, wet, 5)

    # Rock where the slope is steep
    normal_z = g.mask(g.node(unreal.MaterialExpressionVertexNormalWS, 9), 8, b=True)
    steep = g.saturate(g.op(unreal.MaterialExpressionMultiply, 7,
                            g.op(unreal.MaterialExpressionSubtract, 8, g.scalar("FelsAb", 0.8, 9), normal_z),
                            g.const(6.0, 8)), 6)
    ground = g.lerp(ground, g.mask(rock, 7, r=True, g=True, b=True), steep, 5)

    # Brown trodden forest floor on the campaign path (the zone map paints it in one light colour), with a ragged edge
    path_color = g.node(unreal.MaterialExpressionConstant3Vector, 8, constant=unreal.LinearColor(0.485, 0.381, 0.195, 1.0))
    distance = g.node(unreal.MaterialExpressionDistance, 7)
    g.link(zone, distance, "A")
    g.link(path_color, distance, "B")
    ragged = g.op(unreal.MaterialExpressionMultiply, 7, g.op(unreal.MaterialExpressionSubtract, 8, swap_noise, g.const(0.5, 9)), g.const(0.07, 8))
    old_path = g.saturate(g.op(unreal.MaterialExpressionDivide, 5,
                           g.op(unreal.MaterialExpressionAdd, 6, g.op(unreal.MaterialExpressionSubtract, 7, g.scalar("WegBreite", 0.19, 8), distance),
                                ragged),
                           g.const(0.05, 6)), 4)
    # The forest floor scan alone reads grey like gravel: warmed to earth brown
    path_grey = g.node(unreal.MaterialExpressionDesaturation, 5)
    g.link(g.lerp(forest, raw_mud, g.const(0.3, 6), 6), path_grey)
    g.link(g.const(1.0, 6), path_grey, "Fraction")
    earth = g.op(unreal.MaterialExpressionMultiply, 4, path_grey,
                 g.mask(g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="WegFarbe",
                               default_value=unreal.LinearColor(0.95, 0.62, 0.36, 1.0)), 5, r=True, g=True, b=True))

    # The campaign path: the root floor of the trails along the winding line, wider and unevenly so, trodden down the
    # middle, a seam of green moss on both sides
    if masks.get("mainpath") is not None:
        main_distance = g.op(unreal.MaterialExpressionMultiply, 7, g.mask(g.sample(masks["mainpath"], zone_uv, 9), 8, r=True),
                             g.const(TRAIL_RANGE, 8))
        wobble = g.node(unreal.MaterialExpressionNoise, 9, scale=0.008, levels=2, output_min=-1.0, output_max=1.0, quality=1)
        main_half = g.op(unreal.MaterialExpressionAdd, 6,
                         g.op(unreal.MaterialExpressionMultiply, 7, g.scalar("WegBreite", 165.0, 8),
                              g.op(unreal.MaterialExpressionAdd, 8, g.const(0.75, 9), g.op(unreal.MaterialExpressionMultiply, 9, swap, g.const(0.5, 10)))),
                         g.op(unreal.MaterialExpressionMultiply, 7, wobble, g.const(28.0, 8)))
        path = g.saturate(g.op(unreal.MaterialExpressionDivide, 5, g.op(unreal.MaterialExpressionSubtract, 6, main_half, main_distance),
                               g.const(35.0, 6)), 4)
        main_trodden = g.saturate(g.op(unreal.MaterialExpressionDivide, 5,
                                       g.op(unreal.MaterialExpressionSubtract, 6, g.op(unreal.MaterialExpressionMultiply, 7, main_half, g.const(0.4, 8)),
                                            main_distance), g.const(45.0, 6)), 4)
        seam = g.op(unreal.MaterialExpressionMultiply, 5, one_minus(g, path, 6),
                    g.saturate(g.op(unreal.MaterialExpressionDivide, 6,
                                    g.op(unreal.MaterialExpressionSubtract, 7, g.op(unreal.MaterialExpressionAdd, 8, main_half, g.scalar("WegMoosSaum", 140.0, 9)),
                                         main_distance), g.const(60.0, 7)), 6))
        ground = g.lerp(ground, moss_green, g.op(unreal.MaterialExpressionMultiply, 4, seam, g.scalar("WegMoos", 0.7, 5)), 4)
        main_floor = g.op(unreal.MaterialExpressionMultiply, 4, g.lerp(rootmoss, mossy, g.scalar("WegMoosAnteil", 0.25, 6), 5),
                          g.mask(g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="WegBodenFarbe",
                                        default_value=unreal.LinearColor(0.7, 0.72, 0.52, 1.0)), 5, r=True, g=True, b=True))
        main_floor = g.lerp(main_floor, earth, g.op(unreal.MaterialExpressionMultiply, 4, main_trodden, g.scalar("WegGetreten", 0.5, 5)), 3)
        ground = g.lerp(ground, main_floor, path, 3)
        # Plants right up to the edge: the ferns of the thicket band hug the path
        main_clear = g.saturate(g.op(unreal.MaterialExpressionDivide, 5,
                                     g.op(unreal.MaterialExpressionSubtract, 6, g.op(unreal.MaterialExpressionAdd, 7, main_half, g.scalar("WegFrei", 70.0, 8)),
                                          main_distance), g.const(40.0, 6)), 4)
    else:
        path = old_path
        main_clear = old_path
        ground = g.lerp(ground, earth, path, 4)

    # Small trails: the brown-green root moss floor, unevenly wide, a bit trodden down the middle
    if masks.get("trails") is not None:
        trail_distance = g.op(unreal.MaterialExpressionMultiply, 7, g.mask(g.sample(masks["trails"], zone_uv, 9), 8, r=True),
                              g.const(TRAIL_RANGE, 8))
        fine_noise = g.node(unreal.MaterialExpressionNoise, 9, scale=0.012, levels=2, output_min=-1.0, output_max=1.0, quality=1)
        half_width = g.op(unreal.MaterialExpressionAdd, 6,
                          g.op(unreal.MaterialExpressionMultiply, 7, g.scalar("PfadBreite", 90.0, 8),
                               g.op(unreal.MaterialExpressionAdd, 8, g.const(0.7, 9), g.op(unreal.MaterialExpressionMultiply, 9, swap, g.const(0.6, 10)))),
                          g.op(unreal.MaterialExpressionMultiply, 7, fine_noise, g.const(14.0, 8)))
        trail = g.saturate(g.op(unreal.MaterialExpressionDivide, 5, g.op(unreal.MaterialExpressionSubtract, 6, half_width, trail_distance),
                                g.const(25.0, 6)), 4)
        trodden = g.saturate(g.op(unreal.MaterialExpressionDivide, 5,
                                  g.op(unreal.MaterialExpressionSubtract, 6, g.op(unreal.MaterialExpressionMultiply, 7, half_width, g.const(0.45, 8)),
                                       trail_distance), g.const(30.0, 6)), 4)
        # The raw scan reads orange from above: a third of the greened moss in it and a cooler tint, so it stays brown-green
        trail_floor = g.op(unreal.MaterialExpressionMultiply, 4, g.lerp(rootmoss, mossy, g.scalar("PfadMoos", 0.25, 6), 5),
                           g.mask(g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="PfadFarbe",
                                         default_value=unreal.LinearColor(0.7, 0.72, 0.52, 1.0)), 5, r=True, g=True, b=True))
        trail_floor = g.lerp(trail_floor, earth, g.op(unreal.MaterialExpressionMultiply, 4, trodden, g.scalar("PfadGetreten", 0.45, 5)), 3)
        ground = g.lerp(ground, trail_floor, trail, 3)
        # Plants keep well off the trail floor: a fern grows up to 2 m to each side of where it stands, and the grass
        # weights are read on the 2 m landscape grid
        trail_clear = g.saturate(g.op(unreal.MaterialExpressionDivide, 5,
                                      g.op(unreal.MaterialExpressionSubtract, 6, g.op(unreal.MaterialExpressionAdd, 7, half_width,
                                                                                     g.scalar("PfadFrei", 120.0, 8)),
                                           trail_distance), g.const(60.0, 6)), 4)
    else:
        trail = g.const(0.0, 4)
        trail_clear = trail
    bare = g.node(unreal.MaterialExpressionMax, 3)
    g.link(g.op(unreal.MaterialExpressionMax, 4, path, old_path), bare, "A")
    g.link(trail, bare, "B")
    no_plants = g.node(unreal.MaterialExpressionMax, 3)
    g.link(main_clear, no_plants, "A")
    g.link(trail_clear, no_plants, "B")

    # Tinted by the zone colour, so Hain, Moor, Kammweg, Grat and the Nid spring stay what they are on the map
    grey = g.node(unreal.MaterialExpressionDesaturation, 4)
    g.link(ground, grey)
    g.link(g.scalar("Entsaettigung", 0.6, 5), grey, "Fraction")
    zone_tint = g.lerp(zone, g.node(unreal.MaterialExpressionConstant3Vector, 6, constant=unreal.LinearColor(0.3, 0.5, 0.24, 1.0)),
                       g.op(unreal.MaterialExpressionMultiply, 5, weights["saum"], g.scalar("SaumGruen", 0.7, 6)), 5)
    tinted = g.op(unreal.MaterialExpressionMultiply, 3, g.op(unreal.MaterialExpressionMultiply, 4, grey, zone_tint),
                  g.scalar("ZonenHelligkeit", 1.4, 4))
    albedo = g.lerp(ground, tinted, g.op(unreal.MaterialExpressionMultiply, 3, g.scalar("ZonenAnteil", 0.3, 4), one_minus(g, bare, 4)), 2)

    # Big light and dark areas over hundreds of metres, so wide views don't look even
    macro = g.node(unreal.MaterialExpressionNoise, 3, scale=0.00012, levels=2, output_min=0.8, output_max=1.12, quality=1)
    albedo = g.op(unreal.MaterialExpressionMultiply, 1, albedo, macro)

    green_normal = g.lerp(g.lerp(g.sample(textures["grass_normal"], meadow_uv, 5), g.sample(textures["clover_normal"], clover_uv, 5), clover_patch, 4),
                          g.sample(textures["rootmoss_normal"], rootmoss_uv, 5), moss_share, 3)
    normal = g.lerp(g.sample(textures["forest_normal"], forest_uv, 4), green_normal,
                    g.op(unreal.MaterialExpressionMultiply, 3, green_share, one_minus(g, path, 4)), 2)
    normal = g.lerp(normal, g.sample(textures["rootmoss_normal"], rootmoss_uv, 3), g.op(unreal.MaterialExpressionMax, 2, trail, path), 1)
    rough = g.node(unreal.MaterialExpressionLinearInterpolate, 3, const_a=0.92, const_b=0.45)
    g.link(wet, rough, "Alpha")

    library.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(g.const(0.25, 2), "", unreal.MaterialProperty.MP_SPECULAR)

    # Plants: only on dry, flat ground off the path; reeds in a band along the shore
    open_ground = g.op(unreal.MaterialExpressionMultiply, 4, one_minus(g, steep, 5), one_minus(g, no_plants, 5))
    dry = g.saturate(g.op(unreal.MaterialExpressionDivide, 5, g.op(unreal.MaterialExpressionSubtract, 6, height, g.const(20.0, 7)),
                          g.const(30.0, 6)), 4)
    plant_ground = g.op(unreal.MaterialExpressionMultiply, 3, open_ground, dry)
    shore = g.op(unreal.MaterialExpressionMultiply, 4,
                 g.saturate(g.op(unreal.MaterialExpressionDivide, 5, g.op(unreal.MaterialExpressionAdd, 6, height, g.const(40.0, 7)), g.const(30.0, 6)), 4),
                 g.saturate(g.op(unreal.MaterialExpressionDivide, 5, g.op(unreal.MaterialExpressionSubtract, 6, g.const(130.0, 7), height), g.const(60.0, 6)), 4))
    shore_zone = g.saturate(g.op(unreal.MaterialExpressionAdd, 4, weights["moor"],
                                 g.op(unreal.MaterialExpressionMultiply, 5,
                                      g.op(unreal.MaterialExpressionAdd, 6, weights["hain"], weights["saum"]), g.const(0.5, 6))), 3)
    grass_weights = {
        "Hain": weights["hain"], "Saum": g.op(unreal.MaterialExpressionAdd, 2, weights["saum"], leftover), "Moor": weights["moor"], "Kamm": weights["kamm"], "Asche": weights["asche"],
        "Dickicht": g.mask(g.sample(masks["thicket"], zone_uv, 3), 2, r=True) if masks.get("thicket") is not None else g.const(0.0, 2),
    }
    output = g.node(unreal.MaterialExpressionLandscapeGrassOutput, 0)
    inputs = []
    for name in GRASS_TYPES:
        grass_input = unreal.GrassInput()
        grass_input.set_editor_property("name", name)
        grass_input.set_editor_property("grass_type", grass_types[name])
        inputs.append(grass_input)
    output.set_editor_property("grass_types", inputs)
    for name in GRASS_TYPES:
        if name == "Ufer":
            weight = g.op(unreal.MaterialExpressionMultiply, 1, g.op(unreal.MaterialExpressionMultiply, 2, shore, shore_zone), open_ground)
        else:
            weight = g.op(unreal.MaterialExpressionMultiply, 1, grass_weights[name], plant_ground)
        g.link(weight, output, name)
    library.recompile_material(material)


def one_minus(g, source, column):
    expression = g.node(unreal.MaterialExpressionOneMinus, column)
    g.link(source, expression)
    return expression


# ---------------------------------------------------------------- Grey box

def build_greybox(material, textures):
    """Same parameters as wf_blockout.py makes (Farbe, Leuchten), plus a texture over the world that tints the colour"""
    library.delete_all_material_expressions(material)
    g = Graph(material)

    color = g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="Farbe", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    glow = g.scalar("Leuchten", 0.0, 6)

    texture = g.node(unreal.MaterialExpressionTextureObjectParameter, 8, parameter_name="Textur", texture=textures["forest"])
    detail = g.world_aligned(texture, g.scalar("Kachel", 300.0, 9), 6)
    grey = g.node(unreal.MaterialExpressionDesaturation, 5)
    g.link(g.mask(detail, 6, r=True, g=True, b=True), grey)
    bright = g.op(unreal.MaterialExpressionMultiply, 4, grey, g.scalar("Aufhellung", 2.2, 5))
    strength = g.lerp(g.const(1.0, 4), bright, g.scalar("Muster", 0.0, 5), 3)
    base = g.op(unreal.MaterialExpressionMultiply, 2, g.mask(color, 3, r=True, g=True, b=True), strength)

    emissive = g.op(unreal.MaterialExpressionMultiply, 2, color, glow)

    library.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(g.const(0.85, 2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)


def dress_instances(textures):
    count = 0
    for name, (key, tile, strength) in SURFACES.items():
        path = "{}/MI_WF_{}".format(MATERIAL_FOLDER, name)
        instance = unreal.load_asset(path)
        if instance is None:
            notes.append("missing " + path)
            continue
        library.set_material_instance_texture_parameter_value(instance, "Textur", textures[key])
        library.set_material_instance_scalar_parameter_value(instance, "Kachel", tile)
        library.set_material_instance_scalar_parameter_value(instance, "Muster", strength)
        library.set_material_instance_scalar_parameter_value(instance, "Aufhellung", BRIGHTEN.get(key, 2.2))
        if name in COLORS:
            library.set_material_instance_vector_parameter_value(instance, "Farbe", unreal.LinearColor(*COLORS[name], 1.0))
        library.update_material_instance(instance)
        assets.save_loaded_asset(instance, False)
        count += 1
    return count


# ---------------------------------------------------------------- Water

def make_water():
    path = "{}/{}".format(MATERIAL_FOLDER, WATER_NAME)
    if assets.does_asset_exist(path):
        instance = unreal.load_asset(path)
    else:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(WATER_NAME, MATERIAL_FOLDER, unreal.MaterialInstanceConstant,
                                                                           unreal.MaterialInstanceConstantFactoryNew())
        library.set_material_instance_parent(instance, unreal.load_asset(WATER_PARENT))
    # The plane spans 4 km on one texture repeat: about one wave every 4 m, and a wide soft edge at the shore.
    # Most of the sea is only 60 cm deep over light ground, which the pond look turns milky: darker and murkier here.
    for name, value in (("WaveTiling", 1000.0), ("ContactFade", 150.0), ("Murkiness", 0.93)):
        library.set_material_instance_scalar_parameter_value(instance, name, value)
    library.set_material_instance_vector_parameter_value(instance, "DeepColor", unreal.LinearColor(0.003, 0.012, 0.011, 1.0))
    library.set_material_instance_vector_parameter_value(instance, "ShallowColor", unreal.LinearColor(0.03, 0.075, 0.06, 1.0))
    library.update_material_instance(instance)
    assets.save_loaded_asset(instance, False)
    return instance


def add_mood(actors, level_actors):
    """Darker exposure like in Aschenmark, so the textures don't wash out; placed once, then tuned by hand"""
    if any(MOOD_LABEL == actor.get_actor_label() for actor in level_actors):
        return 0
    volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 2000.0), unreal.Rotator(0.0, 0.0, 0.0))
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", -0.8)
    settings.set_editor_property("override_vignette_intensity", True)
    settings.set_editor_property("vignette_intensity", 0.45)
    volume.set_editor_property("settings", settings)
    volume.set_folder_path("Wurzelforst/_Licht_Atmosphaere")
    volume.set_actor_label(MOOD_LABEL)
    volume.set_editor_property("tags", [unreal.Name(DRESS_TAG)])
    return 1


def main():
    textures = {key: unreal.load_asset(path) for key, path in TEXTURES.items()}
    missing = [path for key, path in TEXTURES.items() if textures[key] is None]
    if missing or unreal.load_asset(ZONE_TEXTURE) is None:
        log("missing textures, nothing done: {}".format(missing or [ZONE_TEXTURE]))
        return

    ground = unreal.load_asset(MATERIAL_FOLDER + "/M_WF_Zonenkarte")
    greybox = unreal.load_asset(MATERIAL_FOLDER + "/M_WF_Graubox")
    if ground is None or greybox is None:
        log("run wf_blockout.py first, its materials are missing")
        return

    build_ground(ground, textures, make_grass_types(), import_masks())
    assets.save_loaded_asset(ground, False)
    build_greybox(greybox, textures)
    assets.save_loaded_asset(greybox, False)
    dressed = dress_instances(textures)
    water = make_water()

    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    watered = 0
    doubled = 0
    plates = 0
    level_actors = actors.get_all_level_actors()
    mood = add_mood(actors, level_actors)
    for actor in level_actors:
        if actor.get_actor_label() == WATER_ACTOR:
            actor.static_mesh_component.set_material(0, water)
            watered += 1
        # The level template brought its own sun, sky light, atmosphere and fog next to the blockout's: two suns
        # make everything glare. The template's are switched off (not deleted); turning their root visible again undoes it.
        elif str(actor.get_folder_path()) == "Lighting" and isinstance(actor, DOUBLED) and not {TAG, OFF_TAG} & {str(t) for t in actor.tags}:
            actor.root_component.set_visibility(False, True)
            actor.set_actor_hidden_in_game(True)
            actor.set_editor_property("tags", list(actor.tags) + [unreal.Name(OFF_TAG)])
            doubled += 1
        # The grey box plates of the campaign path: the ground draws the path now (switched off, not deleted)
        elif (actor.get_actor_label().startswith(PATH_PLATE_PREFIX) and PATH_PLATE_FOLDER in str(actor.get_folder_path())
              and unreal.Name(PATH_OFF_TAG) not in actor.tags):
            actor.root_component.set_visibility(False, True)
            actor.set_actor_hidden_in_game(True)
            actor.set_actor_enable_collision(False)
            actor.set_editor_property("tags", list(actor.tags) + [unreal.Name(PATH_OFF_TAG)])
            plates += 1
    if watered or doubled or mood or plates:
        unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)

    log("ground and grey box rebuilt, {} instances textured, water on {} plane(s), {} doubled template lights off, {} path plates off".format(dressed, watered, doubled, plates))
    for note in notes:
        log("NOTE " + note)


main()
