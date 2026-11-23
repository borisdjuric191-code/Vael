# Soft shapes in the Eisenschlund world (Boris 2026-11-22, point 5): the mud puddles (flat cylinders) and the slag river
# (stretched cubes) of the blockout become uneven pieces from Blender (Tools/Welt/eisenschlund_formen.py). Only the mesh of the
# existing actors is swapped, their place, turn and size stay, so the world layout and the rest of L_Vael are untouched.
# BuildEisenschlund.py uses the same pieces on its next full run. With the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=".../Scripts/EisenschlundFormen.py" -unattended -nosplash
import math
import os

import unreal

WORLD = "/Game/Vael/Maps/L_Vael"
SOURCE = "C:/Projekte/Vael_Quellen/Hoehlen/export"
DEST = "/Game/Vael/Maps/Eisenschlund/Formen"
tools = unreal.AssetToolsHelpers.get_asset_tools()


def import_piece(name, scale=1.0):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    ui.static_mesh_import_data.set_editor_property("import_uniform_scale", scale)
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
    extent = mesh.get_bounds().box_extent
    unreal.log_warning("[Formen] {} extent {:.1f} x {:.1f} x {:.1f} cm".format(name, extent.x, extent.y, extent.z))
    return mesh, extent


def piece(name):
    """Imported so that the piece is ~100 cm across like the engine shapes it replaces"""
    mesh, extent = import_piece(name)
    if extent.x > 500.0 or extent.x < 20.0:
        mesh, extent = import_piece(name, 50.0 / extent.x)
    return mesh


suhle = piece("SM_ES_Suhle")
# real surfaces instead of the flat blockout colours: Megascans swamp water for the wallows, the lava material for the slag
LOOK = {"Suhle": unreal.load_asset("/Game/Fab/Megascans/Surfaces/Swamp_Water_tgmjffbqx/Raw/tgmjffbqx_tier_0/Materials/MI_tgmjffbqx"),
        "Glutstrom": unreal.load_asset("/Game/NiagaraExamples/Gallery/Materials/M_Lava")}
strom = piece("SM_ES_Glutstrom")

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level(WORLD)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
try:
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
except Exception as error:  # noqa: BLE001
    unreal.log_warning("[Formen] konnte nicht alle Actors laden: {}".format(error))

n = {"Suhle": 0, "Glutstrom": 0}
# one straight stretch per river segment (~200 m) cannot follow the terrain: cut each into pieces of ~20 m, each set on the ground
# below (they overlap by 4 m and taper, so they read as one winding flow). Pieces that are already short stay as they are.
PIECE = 2000.0
for actor in list(editor.get_all_level_actors()):
    if not isinstance(actor, unreal.StaticMeshActor) or not actor.get_actor_label().startswith("Glutstrom"):
        continue
    scale = actor.get_actor_scale3d()
    length = scale.x * 100.0
    if length <= PIECE * 1.5:
        continue
    loc, yaw = actor.get_actor_location(), actor.get_actor_rotation().yaw
    dx, dy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    folder, material = actor.get_folder_path(), actor.static_mesh_component.get_material(0)
    count = int(round(length / PIECE))
    step = length / count
    for k in range(count):
        t = -length / 2 + step * (k + 0.5)
        part = editor.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(loc.x + dx * t, loc.y + dy * t, loc.z),
                                             unreal.Rotator(0.0, 0.0, yaw + (k % 3 - 1) * 4.0))
        part.set_actor_label("Glutstrom")
        part.set_folder_path(folder)
        part.static_mesh_component.set_static_mesh(strom)
        part.static_mesh_component.set_material(0, material)
        part.static_mesh_component.set_collision_profile_name("NoCollision")
        part.set_actor_scale3d(unreal.Vector((step + 600.0) / 100.0, scale.y * (0.85 + 0.15 * ((k * 7) % 3)), scale.z))
    editor.destroy_actor(actor)

for actor in editor.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    label = actor.get_actor_label()
    folder = str(actor.get_folder_path())
    component = actor.static_mesh_component
    if label.startswith("Suhle") and "Suhlen" in folder:
        mesh, key = suhle, "Suhle"
    elif label.startswith("Glutstrom") and "Schlackenfluss" in folder:
        mesh, key = strom, "Glutstrom"
    else:
        continue
    actor.modify()            # world partition saves an actor only when it is marked as changed
    component.modify()
    component.set_static_mesh(mesh)
    component.set_material(0, LOOK[key])
    n[key] += 1
    # onto the ground, tilted with the slope: the slag river lay ~1.5 m under the terrain (its z came from the lowest of the line
    # points) and only its glow light showed; flat pieces on a slope floated on the downhill side.
    loc, rot, scale = actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d()
    fx = unreal.Vector(math.cos(math.radians(rot.yaw)), math.sin(math.radians(rot.yaw)), 0.0)
    fy = unreal.Vector(-fx.y, fx.x, 0.0)

    def ground(px, py):
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(px, py, loc.z + 5000.0), unreal.Vector(px, py, loc.z - 5000.0),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [actor], unreal.DrawDebugTrace.NONE, True)
        return None if hit is None else hit.to_tuple()[4].z

    ax, ay = scale.x * 40.0, scale.y * 40.0            # sample at 80 % of the half size
    g = [ground(loc.x + fx.x * ax * sx + fy.x * ay * sy, loc.y + fx.y * ax * sx + fy.y * ay * sy) for sx, sy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1))]
    if None in g:
        unreal.log_warning("[Formen] {} at {:.0f} {:.0f} m: no ground found".format(label, loc.x / 100, loc.y / 100))
        continue
    along = unreal.Vector(fx.x * 2 * ax, fx.y * 2 * ax, g[1] - g[2])
    across = unreal.Vector(fy.x * 2 * ay, fy.y * 2 * ay, g[3] - g[4])
    up = unreal.Vector(along.y * across.z - along.z * across.y, along.z * across.x - along.x * across.z, along.x * across.y - along.y * across.x)
    actor.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(along, up), False)
    centre = (g[0] + (g[1] + g[2]) / 2 + (g[3] + g[4]) / 2) / 3
    # the river: its rim (at 20-30 % of the piece height) a few cm into the ground; a wallow sinks in, only its flat top shows
    new_z = centre + scale.z * 50.0 * (0.3 if key == "Glutstrom" else -0.25)
    actor.set_actor_location(unreal.Vector(loc.x, loc.y, new_z), False, False)
    tilt = math.degrees(math.atan2(math.hypot(up.x, up.y), up.z))
    unreal.log_warning("[Formen] {} at {:.0f} {:.0f} m: z {:.0f} -> {:.0f} cm, tilt {:.1f} deg".format(label, loc.x / 100, loc.y / 100, loc.z, new_z, tilt))


# more blockout shapes that have a real piece already: label prefix, folder part, mesh, random turn (deg), sink (share of the height)
# - Funkenerz: the glowing ore cubes become the Funkenerz ore pieces from the caves
# - Nackenwirbel: the "big round bone rocks" (flat cylinders) become real giant vertebrae
SWAPS = [("Funkenerz", "Orte/Funkenerz", ["/Game/Vael/Maps/Hoehlen/Erze/SM_HK_ErzWand_Funkenerz_A", "/Game/Vael/Maps/Hoehlen/Erze/SM_HK_ErzWand_Funkenerz_B",
                                          "/Game/Vael/Maps/Hoehlen/Erze/SM_HK_ErzWand_Funkenerz_C"], 360.0, 0.15),
         ("Wirbel", "Orte/Nackenwirbel", ["/Game/Vael/Requisiten/Knochen/SM_Knochen_Wirbel"], 360.0, 0.2)]
DONE = unreal.Name("VaelFormGetauscht")
for actor in editor.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor) or DONE in actor.tags:
        continue
    label, folder = actor.get_actor_label(), str(actor.get_folder_path())
    swap = next((s for s in SWAPS if label.startswith(s[0]) and s[1] in folder), None)
    if swap is None:
        continue
    _, _, paths, turn, sink = swap
    mesh = unreal.load_asset(paths[sum(map(ord, label)) % len(paths)])
    if mesh is None:
        unreal.log_warning("[Formen] mesh missing for " + label)
        continue
    loc, scale = actor.get_actor_location(), actor.get_actor_scale3d()
    foot = (scale.x + scale.y) * 25.0                      # half footprint of the old shape in cm (100 cm piece, scale in m)
    bounds = mesh.get_bounds()
    ext, origin = bounds.box_extent, bounds.origin
    s = foot / max(ext.x, ext.y, 1.0)
    actor.modify()
    component = actor.static_mesh_component
    component.modify()
    component.set_static_mesh(mesh)
    component.set_editor_property("override_materials", [])
    actor.set_actor_scale3d(unreal.Vector(s, s, s))
    yaw = (sum(map(ord, label)) * 47.0) % turn
    actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(loc.x, loc.y, loc.z + 5000.0), unreal.Vector(loc.x, loc.y, loc.z - 5000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [actor], unreal.DrawDebugTrace.NONE, True)
    ground = hit.to_tuple()[4].z if hit is not None else loc.z
    height = ext.z * 2 * s
    z = ground - (origin.z - ext.z) * s - sink * height     # the bottom of the piece a little into the ground
    actor.set_actor_location(unreal.Vector(loc.x, loc.y, z), False, False)
    actor.set_editor_property("tags", list(actor.tags) + [DONE])
    n[swap[0]] = n.get(swap[0], 0) + 1
    unreal.log_warning("[Formen] {} at {:.0f} {:.0f} m -> {} x{:.2f} ({:.1f} m high)".format(label, loc.x / 100, loc.y / 100, mesh.get_name(), s, height / 100))

# the cave mouths: pillars and lintel were grey cubes. Rough rock wall pieces from the caves fill the same box (rock takes the
# stretch), so the opening, its collision and the walk through it stay as they were.
ROCK = [unreal.load_asset("/Game/Vael/Maps/Hoehlen/Teile/" + n) for n in ("SM_HK_Wand_A", "SM_HK_Wand_B", "SM_HK_Wand_C")]
ROCK_MAT = unreal.load_asset("/Game/Vael/Maps/Hoehlen/Teile/MI_HK_Fels_Eisenschlund")
for actor in editor.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor) or DONE in actor.tags:
        continue
    label, folder = actor.get_actor_label(), str(actor.get_folder_path())
    if label not in ("Pfeiler", "Sturz") or not ("Orte/Stollen" in folder or "Hoehlen/" in folder):
        continue
    mesh = ROCK[sum(map(ord, folder + str(round(actor.get_actor_location().y)))) % len(ROCK)]
    loc, rot, scale = actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d()
    bounds = mesh.get_bounds()
    ext, origin = bounds.box_extent, bounds.origin
    new = unreal.Vector(scale.x * 50.0 / ext.x, scale.y * 50.0 / ext.y, scale.z * 50.0 / ext.z)
    # the piece's box centre sits where the cube's centre was
    off = unreal.Vector(origin.x * new.x, origin.y * new.y, origin.z * new.z)
    cy, sy = math.cos(math.radians(rot.yaw)), math.sin(math.radians(rot.yaw))     # the cubes are only turned about Z
    off = unreal.Vector(off.x * cy - off.y * sy, off.x * sy + off.y * cy, off.z)
    actor.modify()
    component = actor.static_mesh_component
    component.modify()
    component.set_static_mesh(mesh)
    component.set_editor_property("override_materials", [])
    component.set_material(0, ROCK_MAT)
    actor.set_actor_scale3d(new)
    actor.set_actor_location(unreal.Vector(loc.x - off.x, loc.y - off.y, loc.z - off.z), False, False)
    actor.set_editor_property("tags", list(actor.tags) + [DONE])
    n["Portal"] = n.get("Portal", 0) + 1
    unreal.log_warning("[Formen] {} ({}) at {:.0f} {:.0f} m -> {}".format(label, folder, loc.x / 100, loc.y / 100, mesh.get_name()))

unreal.EditorLoadingAndSavingUtils.save_map(world, WORLD)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log_warning("[Formen] getauscht: {}".format(n))
