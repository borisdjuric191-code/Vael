import unreal

MAP = "/Game/Vael/Maps/Test/L_Kreaturentest"
SHOT_DIR = r"C:/Users/boris/AppData/Local/Temp/claude/C--Projekte-Vael/9e55aad8-5a70-4321-a224-aeb084b7ae61/scratchpad/shots"
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    level.load_level(MAP)
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label() == "Boden":
            actor.set_actor_location(unreal.Vector(0, 0, 0), False, False)
    level.save_current_level()
else:
    level.new_level(MAP)
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    plane = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    floor.static_mesh_component.set_static_mesh(plane)
    floor.set_actor_scale3d(unreal.Vector(30, 30, 1))
    floor.set_actor_label("Boden")
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(0, -50, 30))
    sun.set_actor_label("Sonne")
    actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300)).set_actor_label("Himmelslicht")
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("Atmosphaere")
    # 1 m reference cube beside the creature
    ref = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, -160, 50))
    ref.static_mesh_component.set_static_mesh(cube)
    ref.set_actor_label("Referenz 1 m")
    skm = unreal.load_asset("/Game/Vael/Kreaturen/Wurzelforst/Spannhornkaefer/SK_Spannhornkaefer")
    beetle = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, -90))
    beetle.skeletal_mesh_component.set_skeletal_mesh_asset(skm)
    beetle.set_actor_label("Spannhornkaefer")
    level.save_current_level()

cams = []
for label, loc, rot in (("vorne_links", unreal.Vector(260, -220, 170), unreal.Rotator(0, -20, 140)),
                        ("isometrisch", unreal.Vector(-380, -380, 420), unreal.Rotator(0, -40, 45)),
                        ("oben", unreal.Vector(0, 0, 520), unreal.Rotator(0, -90, 0))):
    cam = actors.spawn_actor_from_class(unreal.CameraActor, loc, rot)
    cam.camera_component.set_editor_property("field_of_view", 60.0)
    cams.append((label, cam))

state = {"start": None, "index": 0}

def tick(delta):
    frame = unreal.SystemLibrary.get_frame_count()
    if state["start"] is None:
        state["start"] = frame
    t = frame - state["start"]
    i = state["index"]
    if i < len(cams) and t == 240 + i * 60:
        label, cam = cams[i]
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, "{}/kaefer_{}.png".format(SHOT_DIR, label), camera=cam)
        state["index"] += 1
    if t >= 240 + len(cams) * 60 + 60:
        for _, cam in cams:
            actors.destroy_actor(cam)
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()

handle = unreal.register_slate_post_tick_callback(tick)
