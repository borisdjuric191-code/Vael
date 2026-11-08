# The tinkerer's workshop of the engineer in the Glutkammer (goal 8.11. evening, step 6): imports the pieces of
# Tools/Ingenieur/vael_werkstatt.py to /Game/Vael/Refugium/Glutkammer/Werkstatt with the materials of the Glutkammer kit
# (plus brass), and sets them round his workbench in /Game/Vael/Maps/Refugium/L_Glutkammer. Idempotent (tag VaelWerkstatt);
# run it again after Scripts/BuildGlutkammer.py, which rebuilds the level.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildWerkstatt.py" -EnablePlugins=PythonScriptPlugin -unattended

import math
import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Ingenieur/export"
FOLDER = "/Game/Vael/Refugium/Glutkammer/Werkstatt"
KIT = "/Game/Vael/Refugium/Glutkammer/Probe"
LEVEL = "/Game/Vael/Maps/Refugium/L_Glutkammer"
TAG = "VaelWerkstatt"
assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

PIECES = ["SM_IW_Esse", "SM_IW_Blasebalg", "SM_IW_Amboss", "SM_IW_Loeschfass", "SM_IW_Werkzeugwand", "SM_IW_Schraubstock",
          "SM_IW_Schleifstein", "SM_IW_Zeichentisch", "SM_IW_Regal", "SM_IW_Munitionskiste", "SM_IW_Schrottkiste"]
# Where they stand round the workbench (profile 745/600 of the room plan): offset in cm (X north, Y east) and yaw;
# the working side (+X) of each faces the bench and the hall in the west
BENCH = unreal.Vector((560 - 600) * 100 / 14, (745 - 520) * 100 / 14, 0.0)
LAYOUT = {
    # (dx, dy) in cm from the bench; the rock wall is behind it (east), so the workshop opens to the hall; each faces the bench
    "SM_IW_Esse": (340, -80), "SM_IW_Blasebalg": (400, 70), "SM_IW_Werkzeugwand": (520, -60), "SM_IW_Loeschfass": (300, -260),
    "SM_IW_Amboss": (150, -240), "SM_IW_Munitionskiste": (-20, -300), "SM_IW_Schraubstock": (-170, -210), "SM_IW_Schleifstein": (-320, -190),
    "SM_IW_Zeichentisch": (-300, 60), "SM_IW_Regal": (-470, -80), "SM_IW_Schrottkiste": (120, -420),
}


def log(message):
    unreal.log_warning("[BuildWerkstatt] " + message)


def brass():
    path = FOLDER + "/M_IW_Messing"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    m = tools.create_asset("M_IW_Messing", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_static_lighting", True)
    noise = library.create_material_expression(m, unreal.MaterialExpressionNoise, -600, 0)
    noise.set_editor_property("scale", 0.08)
    noise.set_editor_property("output_min", 0.75)
    noise.set_editor_property("output_max", 1.05)
    colour = library.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -600, 200)
    colour.set_editor_property("constant", unreal.LinearColor(0.62, 0.42, 0.16, 1.0))
    mul = library.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 100)
    library.connect_material_expressions(noise, "", mul, "A")
    library.connect_material_expressions(colour, "", mul, "B")
    library.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    metal = library.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 300)
    metal.set_editor_property("r", 1.0)
    library.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    rough = library.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 400)
    rough.set_editor_property("r", 0.42)
    library.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(m)
    assets.save_loaded_asset(m, False)
    return m


def import_pieces():
    kit = lambda name: unreal.load_asset("{}/{}".format(KIT, name))  # noqa: E731
    slots = {"Holz": kit("MI_GK_Holz"), "Metall": brass(), "Leder": kit("M_GK_Leder"), "Glut": kit("M_GK_Glut"), "Stein": kit("MI_GK_Fels_Brocken"),
             "Glas": kit("M_GK_Glas"), "Papier": kit("M_GK_Papier"), "Eisen": kit("M_GK_Metall"), "Fluessigkeit": kit("M_GK_Fluessigkeit"),
             "Faden": kit("M_GK_Faden")}
    for slot, material in slots.items():
        if material is None:
            log("material missing: " + slot)
    meshes = {}
    for name in PIECES:
        fbx = os.path.join(SOURCE, name + ".fbx")
        if not os.path.isfile(fbx):
            log("missing " + fbx)
            continue
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
        ui.static_mesh_import_data.set_editor_property("auto_generate_collision", True)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", fbx)
        task.set_editor_property("destination_path", FOLDER)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
        mesh = unreal.load_asset(FOLDER + "/" + name)
        if mesh is None:
            log("import failed: " + name)
            continue
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
            if slots.get(slot_name) is not None:
                mesh.set_material(index, slots[slot_name])
        assets.save_loaded_asset(mesh, False)
        meshes[name] = mesh
    return meshes


def place(meshes):
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
        log("could not load " + LEVEL)
        return
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old = [a for a in actors.get_all_level_actors() if unreal.Name(TAG) in a.get_editor_property("tags")]
    for actor in old:
        actors.destroy_actor(actor)
    for name, (dx, dy) in LAYOUT.items():
        yaw = math.degrees(math.atan2(-dy, -dx))
        mesh = meshes.get(name)
        if mesh is None:
            continue
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(BENCH.x + dx, BENCH.y + dy, -2.0), unreal.Rotator(0.0, 0.0, yaw))
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_folder_path("Stufe I/Werkstatt Ingenieur")
        actor.set_actor_label(name.replace("SM_IW_", ""))
        actor.set_editor_property("tags", [unreal.Name(TAG)])
    # the glow of the forge and a warm lamp over the drawing desk
    for (dx, dy, z), colour, intensity, radius, label in (((340, -80, 90), (1.0, 0.42, 0.12), 60.0, 520.0, "Essenglut"),
                                                          ((-300, 60, 140), (1.0, 0.75, 0.45), 12.0, 260.0, "Tischlampe")):
        light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(BENCH.x + dx, BENCH.y + dy, z), unreal.Rotator(0, 0, 0))
        light.light_component.set_light_color(unreal.LinearColor(*colour, 1.0))
        light.light_component.set_intensity(intensity)
        light.light_component.set_attenuation_radius(radius)
        light.set_folder_path("Stufe I/Werkstatt Ingenieur")
        light.set_actor_label(label)
        light.set_editor_property("tags", [unreal.Name(TAG)])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    log("{} old removed, {} pieces placed round the bench at {}".format(len(old), len(LAYOUT), BENCH))


place(import_pieces())
