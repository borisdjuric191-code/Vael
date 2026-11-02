# Loot kit in Unreal: the models and icons of Tools/Loot/vael_loot_baukasten.py, the loot master material and the
# loot items as material data (Boris 2026-11-02: Nidkristall, Geläuterter Kristall, Nidtrunk, Klartrunk).
#
# - Imports Scripts/Data/Loot/Modelle/SM_Loot_*.fbx (simple collision from their UCX_ hulls, no Nanite) and
#   Scripts/Data/Loot/Icons/T_Icon_*.png (UI textures) to /Game/Vael/Items/Loot.
# - M_Vael_Loot: the master of all loot. Parameters Farbe, Leuchten (emissive from inside), Truebung (0 clear .. 1 milky),
#   Adern (glowing veins in object space), AderFarbe, AderSkala, AderBreite, Puls/PulsTempo (breathing glow), and
#   AmBoden (set to 1 by AVaelMaterialDrop: a glint along the rims and sparks, so loot is found in the iso view), Funkeln.
#   Opazitaet only counts in an instance that switches the blend mode to translucent.
#   M_Vael_Loot_Glas (translucent glass) and M_Vael_Loot_Korken (cork) serve the bottle.
# - Instances MI_Loot_Nidkristall, MI_Loot_GelaeuterterKristall, MI_Loot_Nidtrunk, MI_Loot_Klartrunk.
# - Renames DA_Material_Markkristall to DA_Material_Nidkristall (a redirector keeps old references and saves working,
#   Boris' go 2026-11-02) and fills the loot fields of the four items; creates the three new ones.
# - Renames "Mark" to "Nid" in every text players read in the data assets under /Game/Vael (Boris' go 2026-11-02).
#
# Materials are rebuilt when older than KIT_VERSION, models and icons are imported again on every run (they come from
# the Blender kit), item values are set every run. Safe to run again.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateLootKit.py"

import os
import re

import unreal

FOLDER = "/Game/Vael/Items/Loot"
MATERIAL_FOLDER = "/Game/Vael/Items/Materials"
SOURCES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "Loot")
KIT_VERSION = 1.1

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
made = []
notes = []


def log(message):
    unreal.log_warning("[CreateLootKit] " + message)


def color(hex_code):
    """Linear color from an sRGB hex code like 'ff7a2e'"""
    def to_linear(channel):
        value = int(channel, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4

    return unreal.LinearColor(r=to_linear(hex_code[0:2]), g=to_linear(hex_code[2:4]), b=to_linear(hex_code[4:6]), a=1.0)


# ---------------------------------------------------------------------------------------------------------------
# Import
# ---------------------------------------------------------------------------------------------------------------

def import_task(filename, destination, name, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    if options is not None:
        task.set_editor_property("options", options)
    tools.import_asset_tasks([task])
    return unreal.load_asset("{}/{}".format(destination, name))


def import_mesh(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    data = ui.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("one_convex_hull_per_ucx", True)
    data.set_editor_property("generate_lightmap_u_vs", False)

    mesh = import_task(os.path.join(SOURCES, "Modelle", name + ".fbx"), FOLDER + "/Modelle", name, ui)
    if mesh is None:
        notes.append("import failed: " + name)
        return None

    # Small things: no Nanite
    nanite = mesh.get_editor_property("nanite_settings")
    if nanite.get_editor_property("enabled"):
        nanite.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", nanite)
    made.append(mesh.get_path_name())
    return mesh


def import_icon(name):
    texture = import_task(os.path.join(SOURCES, "Icons", name + ".png"), FOLDER + "/Icons", name)
    if texture is None:
        notes.append("import failed: " + name)
        return None
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assets.save_loaded_asset(texture, False)
    made.append(texture.get_path_name())
    return texture


# ---------------------------------------------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------------------------------------------

class Graph:
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
            notes.append("link failed: {}.{} -> {}.{}".format(source.get_name(), source_output, target.get_name(), target_input))

    def scalar(self, name, value, column):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def const(self, value, column):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def vector(self, name, rgb, column):
        parameter = self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(parameter, column - 1, "RGB")

    def mask(self, source, column, channels, source_output=""):
        expression = self.node(unreal.MaterialExpressionComponentMask, column, r="R" in channels, g="G" in channels, b="B" in channels, a="A" in channels)
        self.link(source, expression, "", source_output)
        return expression

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def mul(self, c, a, b): return self.op(unreal.MaterialExpressionMultiply, c, a, b)
    def add(self, c, a, b): return self.op(unreal.MaterialExpressionAdd, c, a, b)
    def sub(self, c, a, b): return self.op(unreal.MaterialExpressionSubtract, c, a, b)
    def div(self, c, a, b): return self.op(unreal.MaterialExpressionDivide, c, a, b)

    def one(self, expression_class, c, a, **properties):
        expression = self.node(expression_class, c, **properties)
        self.link(a, expression)
        return expression

    def sat(self, c, a): return self.one(unreal.MaterialExpressionSaturate, c, a)
    def abs(self, c, a): return self.one(unreal.MaterialExpressionAbs, c, a)
    def sin(self, c, a): return self.one(unreal.MaterialExpressionSine, c, a, period=6.283185)

    def ramp(self, c, x, low, high):
        return self.sat(c, self.mul(c + 1, self.sub(c + 2, x, self.const(low, c + 3)), self.const(1.0 / (high - low), c + 2)))

    def lerp(self, c, a, b, alpha):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, c)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression

    def noise(self, c, position, **properties):
        expression = self.node(unreal.MaterialExpressionNoise, c, **properties)
        self.link(position, expression, "")
        return expression

    def fresnel(self, c, exponent):
        return self.node(unreal.MaterialExpressionFresnel, c, exponent=exponent, base_reflect_fraction=0.0)


def new_material(name):
    path = FOLDER + "/Materialien/" + name
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(p) for p in library.get_scalar_parameter_names(material)] and \
                library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
            return None
        library.delete_all_material_expressions(material)
        for expression in list(unreal.ObjectIterator(unreal.MaterialExpression)):
            if expression.get_outer() == material:
                library.delete_material_expression(material, expression)
        made.append(path + " (graph rebuilt)")
        return material
    made.append(path)
    return tools.create_asset(name, FOLDER + "/Materialien", unreal.Material, unreal.MaterialFactoryNew())


def finish(g, slots):
    """Connects every slot explicitly and stamps the kit version"""
    g.scalar("KitVersion", KIT_VERSION, 30)
    for prop, node in slots.items():
        library.connect_material_property(node, "", prop)
    library.recompile_material(g.material)
    assets.save_loaded_asset(g.material, False)


def local_position(g, column):
    """Position in the space of the model in cm, so veins stay on the crystal when it turns"""
    if hasattr(unreal, "MaterialExpressionLocalPosition"):
        return g.node(unreal.MaterialExpressionLocalPosition, column)
    world = g.node(unreal.MaterialExpressionWorldPosition, column)
    origin = g.node(unreal.MaterialExpressionObjectPositionWS, column)
    return g.sub(column - 1, world, origin)


def build_master():
    material = new_material("M_Vael_Loot")
    if material is None:
        return
    g = Graph(material)
    P = unreal.MaterialProperty

    farbe = g.vector("Farbe", (0.09, 0.012, 0.2), 20)
    ader_farbe = g.vector("AderFarbe", (0.8, 0.35, 1.0), 20)
    leuchten = g.scalar("Leuchten", 2.0, 20)
    truebung = g.scalar("Truebung", 0.3, 20)
    adern = g.scalar("Adern", 1.0, 20)
    ader_skala = g.scalar("AderSkala", 0.35, 20)
    ader_breite = g.scalar("AderBreite", 0.07, 20)
    puls = g.scalar("Puls", 0.15, 20)
    puls_tempo = g.scalar("PulsTempo", 0.4, 20)
    am_boden = g.scalar("AmBoden", 0.0, 20)
    funkeln = g.scalar("Funkeln", 1.5, 20)
    opazitaet = g.scalar("Opazitaet", 0.6, 20)
    time = g.node(unreal.MaterialExpressionTime, 20)

    # Veins: thin ridges of a noise in model space, broken by a coarser noise into single threads
    position = g.mul(18, local_position(g, 19), ader_skala)
    ridge_noise = g.noise(17, position, scale=1.0, levels=3, quality=1, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU,
                          turbulence=False, output_min=-1.0, output_max=1.0)
    ridge = g.sat(15, g.sub(16, g.const(1.0, 17), g.div(16, g.abs(16, ridge_noise), ader_breite)))
    breaks_noise = g.noise(17, g.mul(18, position, g.const(0.45, 19)), scale=1.0, levels=2, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU,
                           output_min=0.0, output_max=1.0)
    threads = g.ramp(14, breaks_noise, 0.45, 0.6)
    veins = g.mul(13, g.mul(14, ridge, threads), adern)

    # Inner glow: strongest where the eye looks into the body, weaker at grazing facets, breathing slowly
    rim = g.fresnel(16, 2.5)
    core = g.sub(14, g.const(1.0, 15), g.mul(15, rim, g.const(0.7, 16)))
    breath = g.add(12, g.const(1.0, 13), g.mul(13, puls, g.sin(14, g.mul(15, time, g.mul(16, puls_tempo, g.const(6.283185, 17))))))
    body_glow = g.mul(11, g.mul(12, farbe, core), leuchten)
    vein_glow = g.mul(11, g.mul(12, ader_farbe, veins), g.mul(12, leuchten, g.const(4.0, 13)))
    glow = g.mul(9, g.add(10, body_glow, vein_glow), breath)

    # On the ground: a glint along the rims and sparks wandering over the facets
    spark_noise = g.noise(15, g.add(16, g.mul(17, position, g.const(3.0, 18)), g.mul(17, time, g.const(0.6, 18))), scale=1.0, levels=1,
                          noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU, output_min=0.0, output_max=1.0)
    sparks = g.ramp(12, spark_noise, 0.72, 0.85)
    glint = g.mul(9, g.add(10, g.mul(11, rim, farbe), g.mul(11, sparks, ader_farbe)), g.mul(10, am_boden, g.mul(11, funkeln, g.const(3.0, 12))))
    emissive = g.add(7, glow, glint)

    # Clear: dark and glossy, the color comes from inside; milky: pale and soft
    milky = g.lerp(10, farbe, g.const(0.85, 11), g.const(0.45, 11))
    base = g.lerp(8, g.mul(9, farbe, g.const(0.35, 10)), milky, truebung)
    roughness = g.lerp(8, g.const(0.08, 9), g.const(0.45, 9), truebung)
    opacity = g.lerp(8, opazitaet, g.const(1.0, 9), truebung)

    finish(g, {P.MP_BASE_COLOR: base, P.MP_EMISSIVE_COLOR: emissive, P.MP_ROUGHNESS: roughness,
               P.MP_SPECULAR: g.const(0.7, 7), P.MP_OPACITY: opacity})


def build_glass():
    material = new_material("M_Vael_Loot_Glas")
    if material is None:
        return
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    material.set_editor_property("two_sided", False)
    g = Graph(material)
    P = unreal.MaterialProperty

    tint = g.vector("Farbe", (0.85, 0.92, 0.86), 12)
    opazitaet = g.scalar("Opazitaet", 0.16, 12)
    am_boden = g.scalar("AmBoden", 0.0, 12)
    rim = g.fresnel(10, 3.0)
    # The rims of the glass catch more light and grow opaque, as real glass does at grazing angles
    opacity = g.lerp(6, opazitaet, g.const(0.7, 7), rim)
    glint = g.mul(6, g.mul(7, rim, tint), g.mul(7, am_boden, g.const(0.8, 8)))

    finish(g, {P.MP_BASE_COLOR: tint, P.MP_OPACITY: opacity, P.MP_ROUGHNESS: g.const(0.04, 6),
               P.MP_SPECULAR: g.const(1.0, 6), P.MP_METALLIC: g.const(0.0, 6), P.MP_EMISSIVE_COLOR: glint})


def build_cork():
    material = new_material("M_Vael_Loot_Korken")
    if material is None:
        return
    g = Graph(material)
    P = unreal.MaterialProperty

    dark = g.vector("Dunkel", (0.16, 0.09, 0.045), 12)
    light = g.vector("Hell", (0.42, 0.27, 0.14), 12)
    g.scalar("AmBoden", 0.0, 12)
    pits = g.noise(10, g.mul(11, local_position(g, 12), g.const(2.5, 12)), scale=1.0, levels=4,
                   noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU, output_min=0.0, output_max=1.0)
    base = g.lerp(7, dark, light, pits)
    finish(g, {P.MP_BASE_COLOR: base, P.MP_ROUGHNESS: g.const(0.85, 6)})


# Looks: instance → parameters (same values as LOOKS in the Blender kit, tune them here or in the editor)
LOOKS = {
    "MI_Loot_Nidkristall": {"Farbe": (0.09, 0.012, 0.2), "AderFarbe": (0.8, 0.35, 1.0), "Leuchten": 2.4, "Truebung": 0.3, "Adern": 1.0, "Puls": 0.2, "PulsTempo": 0.45},
    "MI_Loot_GelaeuterterKristall": {"Farbe": (0.85, 0.42, 0.06), "AderFarbe": (1.0, 0.86, 0.5), "Leuchten": 0.8, "Truebung": 0.08, "Adern": 0.2, "Puls": 0.05, "PulsTempo": 0.2},
    "MI_Loot_Nidtrunk": {"Farbe": (0.2, 0.025, 0.32), "AderFarbe": (0.6, 0.2, 1.0), "Leuchten": 0.8, "Truebung": 0.85, "Adern": 0.0, "Puls": 0.3, "PulsTempo": 0.6},
    "MI_Loot_Klartrunk": {"Farbe": (1.0, 0.7, 0.28), "AderFarbe": (1.0, 0.9, 0.7), "Leuchten": 0.9, "Truebung": 0.05, "Adern": 0.0, "Puls": 0.08, "PulsTempo": 0.3},
}


def build_instances():
    parent = unreal.load_asset(FOLDER + "/Materialien/M_Vael_Loot")
    for name, values in LOOKS.items():
        path = FOLDER + "/Materialien/" + name
        instance = unreal.load_asset(path) if assets.does_asset_exist(path) else \
            tools.create_asset(name, FOLDER + "/Materialien", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        library.set_material_instance_parent(instance, parent)
        for key, value in values.items():
            if isinstance(value, tuple):
                library.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
            else:
                library.set_material_instance_scalar_parameter_value(instance, key, value)
        library.update_material_instance(instance)
        assets.save_loaded_asset(instance, False)
        made.append(path)


def dress_mesh(mesh, slot_materials):
    """Default materials of the model by slot name, so it looks right wherever it is placed"""
    if mesh is None:
        return
    static_materials = mesh.get_editor_property("static_materials")
    for index, slot in enumerate(static_materials):
        name = str(slot.get_editor_property("material_slot_name"))
        for key, material_path in slot_materials.items():
            if name.startswith(key):
                slot.set_editor_property("material_interface", unreal.load_asset(material_path))
                static_materials[index] = slot
    mesh.set_editor_property("static_materials", static_materials)
    assets.save_loaded_asset(mesh, False)


# ---------------------------------------------------------------------------------------------------------------
# Items
# ---------------------------------------------------------------------------------------------------------------

C = unreal.VaelMaterialCategory
R = unreal.VaelRarity
W = unreal.VaelWay
U = unreal.VaelMaterialUse
E = unreal.VaelElement
M = FOLDER + "/Materialien/"

ITEMS = [
    {"asset": "DA_Material_Nidkristall", "display_name": "Nidkristall", "color": "a24dff", "region": "Nid",
     "description": "Erstarrtes Blut der Schlafenden. Man kann es opfern, läutern oder nutzen.",
     "category": C.CRYSTAL, "rarity": R.COMMON, "element": E.MARK, "way": W.SHADOW, "stack": 20, "use": U.NONE,
     "icon": "T_Icon_Nidkristall", "mesh": "SM_Loot_Kristall_Mittel", "material": M + "MI_Loot_Nidkristall"},
    {"asset": "DA_Material_GelaeuterterKristall", "display_name": "Geläuterter Kristall", "color": "ffb43c", "region": "Nid",
     "description": "Von Albrun geläutert. Das Nid darin schläft.",
     "category": C.CRYSTAL, "rarity": R.MAGIC, "element": None, "way": W.LIGHT, "stack": 20, "use": U.NONE,
     "icon": "T_Icon_GelaeuterterKristall", "mesh": "SM_Loot_Kristall_Mittel", "material": M + "MI_Loot_GelaeuterterKristall"},
    {"asset": "DA_Material_Nidtrunk", "display_name": "Nidtrunk", "color": "8a3ad6", "region": "Nid",
     "description": "Macht stark und zeichnet dich. Über eine Pflanze gegossen, treibt sie in Schattenform nach.",
     "category": C.POTION, "rarity": R.MAGIC, "element": E.MARK, "way": W.SHADOW, "stack": 5, "use": U.DRINK,
     "icon": "T_Icon_Nidtrunk", "mesh": "SM_Loot_Flasche", "material": None,
     "inner_mesh": "SM_Loot_Flasche_Fluessigkeit", "inner_material": M + "MI_Loot_Nidtrunk"},
    {"asset": "DA_Material_Klartrunk", "display_name": "Klartrunk", "color": "ffcf6a", "region": "Nid",
     "description": "Klärt Leib und Boden. Über eine Pflanze gegossen, treibt sie in Lichtform nach.",
     "category": C.POTION, "rarity": R.MAGIC, "element": None, "way": W.LIGHT, "stack": 5, "use": U.DRINK,
     "icon": "T_Icon_Klartrunk", "mesh": "SM_Loot_Flasche", "material": None,
     "inner_mesh": "SM_Loot_Flasche_Fluessigkeit", "inner_material": M + "MI_Loot_Klartrunk"},
]


def rename_crystal():
    old = MATERIAL_FOLDER + "/DA_Material_Markkristall"
    new = MATERIAL_FOLDER + "/DA_Material_Nidkristall"
    if assets.does_asset_exist(old) and not assets.does_asset_exist(new):
        if assets.rename_asset(old, new):
            made.append("renamed {} -> {} (redirector stays)".format(old, new))
        else:
            notes.append("could not rename " + old)


def soft(path):
    return unreal.load_asset(path) if path else None


def build_items():
    for item in ITEMS:
        path = MATERIAL_FOLDER + "/" + item["asset"]
        asset = unreal.load_asset(path) if assets.does_asset_exist(path) else \
            tools.create_asset(item["asset"], MATERIAL_FOLDER, unreal.VaelMaterial, unreal.DataAssetFactory())
        if asset is None:
            notes.append("could not create " + path)
            continue

        asset.set_editor_property("display_name", unreal.Text(item["display_name"]))
        asset.set_editor_property("description", unreal.Text(item["description"]))
        asset.set_editor_property("region", unreal.Text(item["region"]))
        asset.set_editor_property("color", color(item["color"]))
        asset.set_editor_property("category", item["category"])
        asset.set_editor_property("rarity", item["rarity"])
        asset.set_editor_property("way", item["way"])
        asset.set_editor_property("has_element", item["element"] is not None)
        if item["element"] is not None:
            asset.set_editor_property("element", item["element"])
        asset.set_editor_property("stack_size", item["stack"])
        asset.set_editor_property("use", item["use"])
        asset.set_editor_property("icon", soft(FOLDER + "/Icons/" + item["icon"]))
        asset.set_editor_property("mesh", soft(FOLDER + "/Modelle/" + item["mesh"]))
        asset.set_editor_property("mesh_material", soft(item["material"]))
        asset.set_editor_property("inner_mesh", soft(FOLDER + "/Modelle/" + item["inner_mesh"]) if item.get("inner_mesh") else None)
        asset.set_editor_property("inner_material", soft(item.get("inner_material")))
        assets.save_loaded_asset(asset, False)
        made.append(path)


# ---------------------------------------------------------------------------------------------------------------
# Mark → Nid in the texts of the data assets
# ---------------------------------------------------------------------------------------------------------------

def nid(text):
    text = re.sub(r"\bMarks\b", "Nids", text)
    return re.sub(r"\bMark(?=[a-zäöüß\- .,;:!?)]|$)", "Nid", text)


def renamed(value, depth):
    """The value with Mark renamed in every text inside it, or None if nothing changed. Other assets are not followed."""
    if depth > 5:
        return None
    if isinstance(value, unreal.Text):
        before = str(value)
        after = nid(before)
        return unreal.Text(after) if after != before else None
    if isinstance(value, unreal.Array):
        changed = False
        for index in range(len(value)):
            new = renamed(value[index], depth + 1)
            if new is not None:
                value[index] = new
                changed = True
        return value if changed else None
    if isinstance(value, unreal.StructBase):
        return value if rename_properties(value, depth + 1) else None
    return None


def rename_properties(owner, depth):
    changed = False
    for name in dir(owner):
        if name.startswith("_"):
            continue
        try:
            value = owner.get_editor_property(name)
        except Exception:
            continue
        new = renamed(value, depth)
        if new is not None:
            try:
                owner.set_editor_property(name, new)
                changed = True
            except Exception as error:
                notes.append("could not set {}.{}: {}".format(owner.get_name() if hasattr(owner, "get_name") else owner, name, error))
    return changed


def rename_texts():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    count = 0
    for data in registry.get_assets_by_path("/Game/Vael", recursive=True):
        class_name = str(data.asset_class_path.asset_name)
        if not class_name.startswith("Vael"):
            continue
        asset = data.get_asset()
        if asset is not None and rename_properties(asset, 0):
            assets.save_loaded_asset(asset, False)
            count += 1
            made.append("Nid texts: " + str(data.package_name))
    log("Mark -> Nid in {} data assets".format(count))


def main():
    meshes = {name: import_mesh(name) for name in ("SM_Loot_Kristall_Klein", "SM_Loot_Kristall_Mittel", "SM_Loot_Kristall_Gross",
                                                   "SM_Loot_Flasche", "SM_Loot_Flasche_Fluessigkeit")}
    for icon in ("T_Icon_Nidkristall", "T_Icon_GelaeuterterKristall", "T_Icon_Nidtrunk", "T_Icon_Klartrunk"):
        import_icon(icon)

    build_master()
    build_glass()
    build_cork()
    build_instances()

    for name in ("SM_Loot_Kristall_Klein", "SM_Loot_Kristall_Mittel", "SM_Loot_Kristall_Gross"):
        dress_mesh(meshes[name], {"Kristall": M + "MI_Loot_Nidkristall"})
    dress_mesh(meshes["SM_Loot_Flasche"], {"Glas": M + "M_Vael_Loot_Glas", "Korken": M + "M_Vael_Loot_Korken"})
    dress_mesh(meshes["SM_Loot_Flasche_Fluessigkeit"], {"Fluessigkeit": M + "MI_Loot_Nidtrunk"})

    rename_crystal()
    build_items()
    rename_texts()

    for line in made:
        log("made " + line)
    for line in notes:
        log("NOTE " + line)
    log("done: {} made, {} notes".format(len(made), len(notes)))


main()
