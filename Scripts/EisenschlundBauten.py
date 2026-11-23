# Buildings in the Eisenschlund world instead of grey blockout shapes (Boris 2026-11-23: finish the Eisenschlund). The pieces come
# from Tools/Welt/eisenschlund_bauten.py and fill a 1 m box like the engine shapes, so only the mesh of each placeholder is swapped:
# place, turn and size stay, the world layout and the rest of L_Vael are untouched. Only actors of the Eisenschlund (tag
# VaelEisenschlund) are touched; a swapped one gets the tag VaelBauGetauscht. Slots take the blockout colours (MI_Blockout_*).
# With the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=".../Scripts/EisenschlundBauten.py" -unattended -nosplash
import os

import unreal

WORLD = "/Game/Vael/Maps/L_Vael"
SOURCE = "C:/Projekte/Vael_Quellen/Hoehlen/export"
DEST = "/Game/Vael/Maps/Eisenschlund/Formen"
BLOCKOUT = "/Game/Vael/Maps/Blockout"
tools = unreal.AssetToolsHelpers.get_asset_tools()
TAG = unreal.Name("VaelEisenschlund")
DONE = unreal.Name("VaelBauGetauscht")

SLOT_MAT = {"Stein": "MI_Blockout_Stone", "Holz": "MI_Blockout_Wood", "Dach": "MI_Blockout_Dach_Rost", "Metall": "MI_Blockout_Metall",
            "Gilde": "MI_Blockout_Guild", "Glut": "MI_Blockout_Glut", "Zelt": "MI_Blockout_Tent", "Nid": "MI_Blockout_Nid",
            "Dunkel": "MI_Blockout_Dunkel"}

# label prefix, part of the folder, pieces (picked by the label)
SWAPS = [
    ("Turmdach", "Grubenstadt", ["SM_ES_Turmdach"]),
    ("Turm ", "Grubenstadt", ["SM_ES_Turm"]),
    ("Haus", "Grubenstadt", ["SM_ES_Haus_A", "SM_ES_Haus_B"]),
    ("Halle", "Schmelzhuetten", ["SM_ES_Halle"]),
    ("Schlot", "Schmelzhuetten", ["SM_ES_Schlot"]),
    ("Pfeiler", "Foerderturm", ["SM_ES_Grubenpfosten"]),
    ("Plattform", "Foerderturm", ["SM_ES_Plattform"]),
    ("Seilrad", "Foerderturm", ["SM_ES_Seilrad"]),
    ("Wachturm", "Wachturm", ["SM_ES_Wachturm"]),
    ("Feuerstelle", "Lager/", ["SM_ES_Feuerstelle"]),
    ("Zelt", "Lager/", ["SM_ES_Zelt"]),
    ("Nid-Quelle", "TiefsterSchacht", ["SM_ES_NidSiegel"]),
    ("Sondierbohrer", "TiefsterSchacht", ["SM_ES_Bohrer"]),
]


def import_piece(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", True)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SOURCE, name + ".fbx"))
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset("{}/{}".format(DEST, name))
    # the blockout colours on the slots of the mesh itself, so every actor using it shows them
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
        mat = unreal.load_asset("{}/{}".format(BLOCKOUT, SLOT_MAT.get(slot_name, "MI_Blockout_Stone")))
        if mat is not None:
            mesh.set_material(index, mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    ext = mesh.get_bounds().box_extent
    unreal.log_warning("[Bauten] {} extent {:.1f} x {:.1f} x {:.1f} cm".format(name, ext.x, ext.y, ext.z))
    return mesh


MESHES = {name: import_piece(name) for _, _, names in SWAPS for name in names}

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level(WORLD)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
try:
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
except Exception as error:  # noqa: BLE001
    unreal.log_warning("[Bauten] konnte nicht alle Actors laden: {}".format(error))

n = {}
for actor in editor.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    tags = list(actor.tags)
    if TAG not in tags:
        continue
    label, folder = actor.get_actor_label(), str(actor.get_folder_path())
    swap = next((s for s in SWAPS if label.startswith(s[0]) and s[1] in folder), None)
    if swap is None:
        continue
    names = swap[2]
    mesh = MESHES[names[sum(map(ord, label)) % len(names)]]
    actor.modify()
    component = actor.static_mesh_component
    component.modify()
    component.set_static_mesh(mesh)
    # the slot colours of the piece, not the one blockout colour of the placeholder
    component.set_editor_property("override_materials", [])
    if DONE not in tags:
        actor.set_editor_property("tags", tags + [DONE])
    n[swap[0].strip()] = n.get(swap[0].strip(), 0) + 1

# Each tower roof sits on its own tower (the blockout put every roof at 19 m, the towers are 14 to 22 m high: some floated, some sank)
tuerme, daecher = {}, {}
for actor in editor.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor) or TAG not in actor.tags or "Grubenstadt" not in str(actor.get_folder_path()):
        continue
    label = actor.get_actor_label()
    if label.startswith("Turmdach "):
        daecher[label.split()[-1]] = actor
    elif label.startswith("Turm "):
        tuerme[label.split()[-1]] = actor
for nummer, dach in daecher.items():
    turm = tuerme.get(nummer)
    if turm is None:
        continue
    t_loc, t_scale = turm.get_actor_location(), turm.get_actor_scale3d()
    d_loc, d_scale = dach.get_actor_location(), dach.get_actor_scale3d()
    oben = t_loc.z + t_scale.z * 50.0                   # top of the tower (1 m piece centred on its origin)
    z = oben - 60.0 + d_scale.z * 50.0                  # the roof's eaves just over the crenellations
    dach.modify()
    dach.set_actor_location(unreal.Vector(t_loc.x, t_loc.y, z), False, False)
    n["Dach auf Turm"] = n.get("Dach auf Turm", 0) + 1

# Rippenbrücke: a real rib of the Urdrachin over the way instead of three bone cubes. The rib has no collision (one walks under it;
# its simple hull would close the way); the two feet cubes stay where its legs stand, hidden, with their collision
RIPPE = unreal.load_asset("/Game/Vael/Maps/Hoehlen/Teile/SM_HK_Rippe")
bogen, fuesse = None, []
for actor in editor.get_all_level_actors():
    if isinstance(actor, unreal.StaticMeshActor) and TAG in actor.tags and "Rippenbr" in str(actor.get_folder_path()):
        if actor.get_actor_label() == "Bogen":
            bogen = actor
        elif actor.get_actor_label().startswith("Bogenfu"):
            fuesse.append(actor)
if bogen is not None and bogen.static_mesh_component.static_mesh == RIPPE:
    bogen = None                                        # done on an earlier run
if RIPPE is not None and bogen is not None and len(fuesse) == 2:
    unten = min(f.get_actor_location().z - f.get_actor_scale3d().z * 50.0 for f in fuesse)
    b_loc, b_scale = bogen.get_actor_location(), bogen.get_actor_scale3d()
    oben = b_loc.z + b_scale.z * 50.0
    a, c = fuesse[0].get_actor_location(), fuesse[1].get_actor_location()
    spann = ((a.x - c.x) ** 2 + (a.y - c.y) ** 2) ** 0.5 + 600.0
    bounds = RIPPE.get_bounds()
    ext, origin = bounds.box_extent, bounds.origin
    s = unreal.Vector(spann / (2 * ext.x), 1.6, (oben - unten) / (2 * ext.z))
    yaw = bogen.get_actor_rotation().yaw + 90.0
    bogen.modify()
    comp = bogen.static_mesh_component
    comp.modify()
    comp.set_static_mesh(RIPPE)
    comp.set_editor_property("override_materials", [])
    comp.set_collision_profile_name("NoCollision")
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    bogen.set_actor_scale3d(s)
    bogen.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
    bogen.set_actor_location(unreal.Vector(b_loc.x, b_loc.y, unten - (origin.z - ext.z) * s.z), False, False)
    for f in fuesse:
        f.modify()
        f.set_actor_hidden_in_game(True)
        f.set_editor_property("hidden", True)
    n["Rippenbruecke"] = 1

unreal.EditorLoadingAndSavingUtils.save_map(world, WORLD)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log_warning("[Bauten] getauscht: {}".format(n))
