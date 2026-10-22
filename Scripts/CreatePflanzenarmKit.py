# Creates the plant arms of the Floromant (Pflanzenarm): the single stalk meshes, the material M_Halm and DA_Pflanzenarm_<Id>.
#
# - Stalks: FBX from Tools/Pflanzenarm/halme_zerlegen.py (cut out of the Tripo model, which stays untouched), standing on +Z with
#   the pivot at the base and a vertex colour ramp R from 0 at the base to 1 at the tip. Imported with vertex colours, no
#   collision, no Nanite (they are bent as spline meshes).
# - M_Halm: masked, two-sided. Silvery tone over the picture of the plant with a light glow; Wind sways the stalk more towards the
#   tip, Spin twirls the tips around the arm axis (ArmPunkt, ArmAchse, set by the game), Wuchs (0-1) shows the stalk from the
#   base to the tip, Schatten turns it into the violet shadow form (Flüsterhalm), Aufleuchten flashes the tip when it fires.
# - Only creates what is missing; M_Halm gets its graph rebuilt when it is older than KIT_VERSION.
# - Everything lives under /Game/Vael/Characters/Floromant/Pflanzenarm.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreatePflanzenarmKit.py"

import os
import unreal

FOLDER = "/Game/Vael/Characters/Floromant/Pflanzenarm"
SOURCES = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "Vael_Quellen")
PLANT_FOLDER = "/Game/Vael/Characters/Floromant/Pflanzen"
PROPS = "/Game/Vael/Requisiten/Aschenmark"

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
created = []
notes = []

KIT_VERSION = 1.4

FLAT_NORMAL = "/Engine/EngineMaterials/FlatNormal.FlatNormal"
GREY = "/Engine/ArtTools/RenderToTexture/Textures/127grey.127grey"

# Plant id: stalks of the fingers and the arm (numbers of SM_<Id>_Halm_<n>), a head on the fingertips (SM_<Id>_Kopf),
# values of the material instance (scalars, and colours as tuples) and of the data asset (only set when it is created)
ARMS = {
    "Windhalm": {
        "finger": [1, 2],
        "arm": [3],
        "head": False,
        "values": {},
        "data": {},
    },
    # Long thin thorny stems ending in the glowing heads; the heads fire seeds that burst in flames, like the plant in the garden
    "Glutdistel": {
        "finger": [1, 3],
        "arm": [1, 3],
        "head": True,
        "values": {"Silber": 0.3, "Grundton": (0.13, 0.11, 0.11), "Leuchten": 0.04, "Glut": 5.0, "Leuchtfarbe": (1.0, 0.33, 0.06),
                   "Metall": 0.1, "Rauheit": 0.6},
        "data": {"shot_element": "FIRE", "fire_interval": 0.2, "shot_damage": 5.0, "shot_speed": 1500.0, "shot_radius": 10.0,
                 "burst_radius": 90.0, "burst_damage": 3.0, "finger_length": 45.0, "finger_thickness": 0.2, "tip_size": 14.0,
                 "arm_thickness": 0.35,
                 "rest_wind": 0.2},
    },
}


def log(message):
    unreal.log_warning("[CreatePflanzenarmKit] " + message)


class Graph:
    """Places material nodes in columns and checks every link (as in BuildRiesenpilze.py)"""

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

    def vector(self, name, rgb, column):
        parameter = self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name,
                              default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(parameter, column - 1)

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def one(self, expression_class, column, a):
        expression = self.node(expression_class, column)
        self.link(a, expression)
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
        return self.one(unreal.MaterialExpressionSaturate, column, source)

    def grey(self, source, column):
        expression = self.node(unreal.MaterialExpressionDesaturation, column)
        self.link(source, expression)
        self.link(self.const(1.0, column + 1), expression, "Fraction")
        return expression

    def append(self, a, b, column):
        expression = self.node(unreal.MaterialExpressionAppendVector, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression


# ---------------------------------------------------------------- Stalk meshes

def import_stalk(plant, index):
    return import_mesh(plant, "SM_{}_Halm_{}".format(plant, index), 50.0)


def import_mesh(plant, name, min_height):
    path = "{}/{}/{}".format(FOLDER, plant, name)
    if assets.does_asset_exist(path):
        existing = unreal.load_asset(path)
        # Stalks are 100 cm long, heads 10 cm; the first export came out a hundred times too small
        if existing.get_bounding_box().max.z > min_height:
            return existing
        notes.append(name + " was imported at the wrong scale, imported again")
        assets.delete_asset(path)

    fbx = os.path.join(SOURCES, plant, "05_pflanzenarm", name + ".fbx")
    if not os.path.exists(fbx):
        notes.append("missing " + fbx + " (run Tools/Pflanzenarm/halme_zerlegen.py first)")
        return None

    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    data = ui.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    # UV channel 1 holds the ramp from base to tip; a lightmap UV must not be written over it
    data.set_editor_property("generate_lightmap_u_vs", False)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx)
    task.set_editor_property("destination_path", "{}/{}".format(FOLDER, plant))
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])

    mesh = unreal.load_asset(path)
    if mesh is None:
        notes.append("import failed: " + name)
        return None

    # Bent along splines, so no Nanite; nothing collides with them
    nanite = mesh.get_editor_property("nanite_settings")
    if nanite.get_editor_property("enabled"):
        nanite.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", nanite)
    # (EditorStaticMeshLibrary reports 0 UV channels and no vertex colours for every mesh in the commandlet, so they are not checked here)
    unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
    unreal.EditorStaticMeshLibrary.set_generate_lightmap_uv(mesh, False)

    # Interchange may bring a material along; the arm sets its own
    for material_path in assets.list_assets("{}/{}".format(FOLDER, plant), recursive=False):
        loaded = unreal.load_asset(material_path)
        if isinstance(loaded, (unreal.Material, unreal.MaterialInstance, unreal.Texture)) and loaded.get_name() not in ("MI_Halm_" + plant,):
            notes.append("imported along (unused): " + material_path)
    assets.save_loaded_asset(mesh, False)
    created.append(path)
    return mesh


# ---------------------------------------------------------------- M_Halm

def new_material(name):
    path = "{}/{}".format(FOLDER, name)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(parameter) for parameter in library.get_scalar_parameter_names(material)]:
            if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
                return None
        library.delete_all_material_expressions(material)
        created.append(path + " (graph rebuilt)")
        return material
    created.append(path)
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def build_stalk_material():
    material = new_material("M_Halm")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Halm")

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.5)
    material.set_editor_property("used_with_spline_meshes", True)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 12)

    # The ramp from 0 at the base to 1 at the tip: UV channel 1 (the meshes carry it as vertex colour R too, which not every importer keeps)
    ramp_uv = g.node(unreal.MaterialExpressionTextureCoordinate, 11, coordinate_index=1)
    ramp = g.mask(ramp_uv, 10, r=True, g=False, b=False)
    ramp2 = g.mul(9, ramp, ramp)

    # Colour: the picture of the plant, pulled towards a silvery tone; the shadow form is violet
    picture = g.node(unreal.MaterialExpressionTextureSampleParameter2D, 10, parameter_name="Textur",
                     texture=unreal.load_asset(PROPS + "/Windhalm/T_Windhalm_basecolor"))
    picture_rgb = g.mask(picture, 9)
    grey = g.grey(picture_rgb, 8)
    silver = g.mul(7, g.mul(8, grey, g.vector("Grundton", (0.80, 0.90, 0.95), 9)), g.const(1.35, 8))
    light = g.lerp(picture_rgb, silver, g.scalar("Silber", 0.35, 8), 6)
    shadow_tone = g.mul(6, grey, g.vector("Schattenton", (0.48, 0.28, 0.70), 7))
    shadow = g.scalar("Schatten", 0.0, 6)
    albedo = g.lerp(light, g.mul(5, shadow_tone, g.const(1.4, 6)), shadow, 4)

    # Glow: faint along the stalk, more towards the tip; the firing tip flashes
    glow_color = g.lerp(g.vector("Leuchtfarbe", (0.55, 0.95, 1.0), 8), g.vector("Schattenleuchten", (0.70, 0.35, 1.0), 8), shadow, 6)
    along = g.add(7, g.const(0.3, 8), g.mul(8, ramp, g.const(0.7, 9)))
    resting = g.mul(5, g.mul(6, glow_color, along), g.scalar("Leuchten", 0.12, 6))
    tip = g.mul(7, ramp2, ramp2)
    flash = g.mul(5, g.mul(6, glow_color, tip), g.mul(6, g.scalar("Aufleuchten", 0.0, 7), g.const(8.0, 7)))
    # Glut: the reddish parts of the picture shine in their own colour (the embers of the Glutdistel); 0 for other plants
    red = g.mask(picture, 8, r=True, g=False, b=False)
    green = g.mask(picture, 8, r=False, g=True, b=False)
    ember = g.saturate(g.mul(6, g.sub(7, g.sub(8, red, green), g.const(0.18, 8)), g.const(4.0, 7)), 5)
    embers = g.mul(4, g.mul(5, picture_rgb, ember), g.scalar("Glut", 0.0, 5))
    emissive = g.add(3, g.add(4, resting, flash), embers)

    # Growing from the base to the tip
    grow = g.mul(5, g.scalar("Wuchs", 1.0, 6), g.const(1.06, 6))
    opacity = g.saturate(g.mul(3, g.sub(4, grow, ramp), g.const(40.0, 4)), 2)

    # Sway: more towards the tip, each stalk in its own time
    world = g.node(unreal.MaterialExpressionWorldPosition, 11)
    world_xy = g.mask(world, 10, r=True, g=True, b=False)
    dot = g.node(unreal.MaterialExpressionDotProduct, 9)
    g.link(world_xy, dot, "A")
    g.link(g.node(unreal.MaterialExpressionConstant2Vector, 10, r=1.0, g=1.0), dot, "B")
    place = g.mul(8, dot, g.const(0.004, 9))
    time = g.node(unreal.MaterialExpressionTime, 10)
    phase = g.add(7, g.add(8, g.mul(9, time, g.scalar("WindTempo", 1.6, 10)), g.scalar("Zeitversatz", 0.0, 9)), place)
    sway_x = g.one(unreal.MaterialExpressionSine, 6, phase)
    sway_y = g.mul(5, g.one(unreal.MaterialExpressionCosine, 6, g.mul(7, phase, g.const(0.8, 8))), g.const(0.6, 6))
    sway = g.append(g.append(sway_x, sway_y, 5), g.const(0.0, 5), 4)
    sway_amount = g.mul(4, g.mul(5, g.scalar("Wind", 0.35, 6), g.scalar("Schwingweite", 7.0, 6)), ramp2)
    sway_offset = g.mul(3, sway, sway_amount)

    # Spin: the tips twirl around the arm axis, more the farther out they are
    rotate = g.node(unreal.MaterialExpressionRotateAboutAxis, 4)
    g.link(g.vector("ArmAchse", (0.0, 0.0, 1.0), 6), rotate, "NormalizedRotationAxis")
    g.link(g.mul(5, g.scalar("Spin", 0.0, 6), ramp), rotate, "RotationAngle")
    g.link(g.vector("ArmPunkt", (0.0, 0.0, 0.0), 6), rotate, "PivotPoint")
    g.link(world, rotate, "Position")
    offset = g.add(2, sway_offset, rotate)

    normal_map = g.node(unreal.MaterialExpressionTextureSampleParameter2D, 4, parameter_name="Normalkarte",
                        texture=unreal.load_asset(PROPS + "/Windhalm/T_Windhalm_normal") or unreal.load_asset(FLAT_NORMAL),
                        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)

    library.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal_map, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(g.scalar("Metall", 0.35, 2), "", unreal.MaterialProperty.MP_METALLIC)
    library.connect_material_property(g.scalar("Rauheit", 0.4, 2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    library.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def make_instance(parent, plant, values):
    name = "MI_Halm_" + plant
    path = "{}/{}".format(FOLDER, name)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    instance = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, parent)
    for key, value in values.items():
        if isinstance(value, tuple):
            library.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
        else:
            library.set_material_instance_scalar_parameter_value(instance, key, value)
    picture = unreal.load_asset("{}/{}/T_{}_basecolor".format(PROPS, plant, plant))
    if picture is not None:
        library.set_material_instance_texture_parameter_value(instance, "Textur", picture)
    normal = unreal.load_asset("{}/{}/T_{}_normal".format(PROPS, plant, plant))
    if normal is not None:
        library.set_material_instance_texture_parameter_value(instance, "Normalkarte", normal)
    assets.save_loaded_asset(instance, False)
    created.append(path)
    return instance


# ---------------------------------------------------------------- DA_Pflanzenarm_<Id>

def make_arm(plant, finger_meshes, arm_meshes, head, vine, material, values):
    path = "{}/DA_Pflanzenarm_{}".format(FOLDER, plant)
    if assets.does_asset_exist(path):
        # Fields that came later are filled in while they are still empty
        data = unreal.load_asset(path)
        if vine is not None and len(data.get_editor_property("vine_meshes")) == 0:
            data.set_editor_property("vine_meshes", [vine])
            assets.save_loaded_asset(data, False)
            created.append(path + " (vines)")
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.VaelPlantArmData)
    data = tools.create_asset("DA_Pflanzenarm_" + plant, FOLDER, unreal.VaelPlantArmData, factory)
    plant_data = unreal.load_asset("{}/DA_Pflanze_{}".format(PLANT_FOLDER, plant))
    if plant_data is not None:
        data.set_editor_property("plant", plant_data)
    data.set_editor_property("finger_meshes", [m for m in finger_meshes if m is not None])
    data.set_editor_property("arm_meshes", [m for m in arm_meshes if m is not None])
    data.set_editor_property("material", material)
    if head is not None:
        data.set_editor_property("tip_mesh", head)
    if vine is not None:
        data.set_editor_property("vine_meshes", [vine])
    for key, value in values.items():
        data.set_editor_property(key, getattr(unreal.VaelElement, value) if key == "shot_element" else value)
    assets.save_loaded_asset(data, False)
    created.append(path)


def main():
    stalk_material = build_stalk_material()
    for plant, arm in ARMS.items():
        fingers = [import_stalk(plant, i) for i in arm["finger"]]
        others = [import_stalk(plant, i) for i in arm["arm"]]
        head = import_mesh(plant, "SM_{}_Kopf".format(plant), 5.0) if arm["head"] else None
        # The vine for the overgrowth while aiming, from Tools/Pflanzenarm/ranke_bauen.py
        vine = import_mesh(plant, "SM_{}_Ranke".format(plant), 50.0)
        instance = make_instance(stalk_material, plant, arm["values"])
        for mesh in fingers + others + [head, vine]:
            if mesh is not None:
                mesh.set_material(0, instance)
                assets.save_loaded_asset(mesh, False)
        make_arm(plant, fingers, others, head, vine, instance, arm["data"])

    for line in created:
        log("created " + line)
    for line in notes:
        log("NOTE " + line)
    log("done")


main()
