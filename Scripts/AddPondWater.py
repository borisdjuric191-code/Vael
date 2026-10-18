# Gives the pond of L_Aschenmark (north of the camp) water of the water kit instead of the blockout look.
#
# - Shallow water: a flat plane of M_Water_Puddle whose soft, uneven shore comes from the mask T_Water_PondMask
#   (Scripts/Data/T_Water_PondMask.png: 1 in the middle, 0.5 at the shore). The ground shows through it.
# - Deep middle: a second, smaller plane of the same shape, nearly black-green, that fades softly into the shallow water.
# - Both are dark, cold and still, with fine waves and a little foam at the shore; unlike spell water they never glow.
# - Only adds: tag "VaelKulisse", Outliner folder "Teich/Kulisse/Wasser"; the script skips the pond while that folder exists.
#   The blue disc of "Seichtes Wasser" and the dark cylinder "Tiefes Wasser" only turn invisible: drawing water in the
#   shallows and the blocked deep middle play exactly as before.
# - Creates the mask texture and the two material instances under /Game/VFX/Water when they are missing.
#
# Run with the editor closed, after CreateWaterKit.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/AddPondWater.py" -EnablePlugins=PythonScriptPlugin

import os

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Aschenmark"
FOLDER = "Teich/Kulisse/Wasser"
TEXTURES = "/Game/VFX/Water/Textures"
MATERIALS = "/Game/VFX/Water/Materials"
PUDDLE_MATERIAL = MATERIALS + "/M_Water_Puddle"
MASK_NAME = "T_Water_PondMask"
MASK_SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", MASK_NAME + ".png")
PLANE = "/Engine/BasicShapes/Plane.Plane"

# Radius of the shore in the mask, as a share of its width; small enough that its bays and soft edge stay on the plane
MASK_SHORE = 0.38

# Heights of the two surfaces above the ground in cm; the deep one lies on top
SHALLOW_HEIGHT = 6.0
DEEP_HEIGHT = 7.0

DRESSING_TAG = "VaelKulisse"
HIDDEN_TAG = "VaelPlatzhalterVersteckt"

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lines = []

# Shared look of the pond: dark cold green, still, fine waves, no glow; reflects little so it stays dark under the open sky
POND_SCALARS = {"EmissiveStrength": 0.0, "WobbleStrength": 0.0, "WobbleSpeed": 0.1, "Refraction": 0.1, "FresnelPower": 5.0,
                "Wetness": 1.0, "SpecularScale": 0.2, "WaveStrength": 0.35, "WaveTiling": 1.0, "WaveSpeed": 0.04, "Sparkle": 0.2,
                "ContactFade": 4.0}

LOOKS = {
    # Shallow: the ground shimmers through, a thin line of grey foam along the shore
    "Shallow": {"colors": {"DeepColor": (0.01, 0.03, 0.025), "ShallowColor": (0.05, 0.1, 0.08)},
                "scalars": {"Murkiness": 0.3, "FoamAmount": 0.1, "EdgeSoftness": 0.06, "EdgeFoam": 0.2, "EdgeFoamWidth": 0.08}},
    # Deep: nearly black-green, its edge fades over about a metre and a half into the shallows, no foam there
    "Deep": {"colors": {"DeepColor": (0.002, 0.008, 0.007), "ShallowColor": (0.01, 0.03, 0.025)},
             "scalars": {"Murkiness": 0.95, "FoamAmount": 0.0, "EdgeSoftness": 0.18, "EdgeFoam": 0.0, "SpecularScale": 0.1}},
}


def import_mask():
    path = "{}/{}".format(TEXTURES, MASK_NAME)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", MASK_SOURCE)
    task.set_editor_property("destination_path", TEXTURES)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.load_asset(path)
    if texture is None:
        return None

    # The values are stored with sRGB so the plain colour sampler of the mask reads them back as the field values
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assets.save_loaded_asset(texture, False)
    lines.append("created " + path)
    return texture


def make_instance(name, look, mask, origin, size):
    path = "{}/{}".format(MATERIALS, name)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)

    instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, unreal.load_asset(PUDDLE_MATERIAL))
    for key, value in dict(POND_SCALARS, **look["scalars"]).items():
        library.set_material_instance_scalar_parameter_value(instance, key, value)
    for key, value in look["colors"].items():
        library.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))

    # The mask lies over the world: its corner and width in cm
    library.set_material_instance_vector_parameter_value(instance, "MaskOrigin", unreal.LinearColor(origin.x, origin.y, 0.0, 0.0))
    library.set_material_instance_scalar_parameter_value(instance, "MaskSize", size)
    library.set_material_instance_texture_parameter_value(instance, "PuddleMask", mask)
    library.update_material_instance(instance)
    assets.save_loaded_asset(instance, False)
    lines.append("created " + path)
    return instance


def add_plane(center, size, height, material, label, sort_priority):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(center.x, center.y, height), unreal.Rotator(0.0, 0.0, 0.0))
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_asset(PLANE))
    component.set_material(0, material)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("cast_shadow", False)
    component.set_editor_property("translucency_sort_priority", sort_priority)
    actor.set_actor_scale3d(unreal.Vector(size / 100.0, size / 100.0, 1.0))
    actor.set_folder_path(FOLDER)
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(DRESSING_TAG)])


def hide(actor, component):
    component.set_editor_property("visible", False)
    actor.set_editor_property("tags", list(actor.get_editor_property("tags")) + [unreal.Name(HIDDEN_TAG)])


def main():
    world_object = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    if world_object is None:
        unreal.log_error("Vael: could not load {}".format(LEVEL_PATH))
        return

    level_actors = list(actors.get_all_level_actors())
    changed = False

    if any(str(actor.get_folder_path()) == FOLDER for actor in level_actors):
        lines.append("skipped: the pond already has its water")
    else:
        shallow = next((actor for actor in level_actors if isinstance(actor, unreal.VaelGroundArea) and actor.get_actor_label() == "Seichtes Wasser"), None)
        deep = next((actor for actor in level_actors if isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label() == "Tiefes Wasser"), None)
        mask = import_mask()

        if shallow is None or deep is None or mask is None:
            lines.append("missing: shallow water {}, deep water {}, mask {}".format(shallow is not None, deep is not None, mask is not None))
        else:
            center = shallow.get_actor_location()
            ground = center.z - 2.0
            shallow_radius = shallow.get_editor_property("radius")
            deep_radius = deep.get_actor_scale3d().x * 50.0

            for kind, radius, height, priority in (("Shallow", shallow_radius, SHALLOW_HEIGHT, 0), ("Deep", deep_radius, DEEP_HEIGHT, 1)):
                size = radius / MASK_SHORE
                origin = unreal.Vector(center.x - size * 0.5, center.y - size * 0.5, 0.0)
                material = make_instance("MI_Water_Pond_{}_Aschenmark".format(kind), LOOKS[kind], mask, origin, size)
                add_plane(center, size, ground + height, material, "Teichwasser seicht" if kind == "Shallow" else "Teichwasser tief", priority)
                lines.append("{} water: radius {:.0f}, plane {:.0f} cm".format(kind, radius, size))

            hide(shallow, shallow.get_component_by_class(unreal.StaticMeshComponent))
            hide(deep, deep.static_mesh_component)
            deep.set_actor_hidden_in_game(True)
            changed = True

    saved = changed and unreal.EditorLoadingAndSavingUtils.save_map(world_object, LEVEL_PATH)
    lines.insert(0, "saved: {}".format(saved))
    with open(os.path.join(unreal.Paths.project_saved_dir(), "PondWaterLog.txt"), "w", encoding="utf-8") as file:
        file.write("\n".join(lines))


main()
