# Imports the Tripo pieces of the engineer made 26.11.: the Aschenjaeger set as its own body (Tools/Ingenieur/vael_aschenjaeger_rig.py:
# SKM_Ing_AJ_Kopf/Brust/Arme/Beine on one skeleton) to /Game/Vael/Characters/Ingenieur/Aschenjaeger, and the static Doppelsichel
# and Quarzbrille (Tools/Ingenieur/vael_tripo_werkstuecke.py) to /Game/Vael/Characters/Ingenieur/Werkstuecke as SM_Ing_*_Tripo.
# One master material M_Ing_Tripo (colour, normal map and the glTF occlusion/roughness/metal map) with an instance per model.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportTripoIngenieur.py" -unattended -nosplash -nullrhi
import os

import unreal

RIG = "C:/Projekte/Vael_Quellen/Ingenieur/Aschenjaeger/05_rig"
PROPS = "C:/Projekte/Vael_Quellen/Ingenieur/Tripo_Werkstuecke"
AJ = "/Game/Vael/Characters/Ingenieur/Aschenjaeger"
WS = "/Game/Vael/Characters/Ingenieur/Werkstuecke"
MASTER = "/Game/Vael/Characters/Ingenieur/Materialien/M_Ing_Tripo"
KIT_VERSION = 1.2

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary


def log(message):
    unreal.log_warning("[ImportTripoIngenieur] " + message)


def expr(m, cls, x, y, **props):
    n = library.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def master():
    if assets.does_asset_exist(MASTER):
        m = unreal.load_asset(MASTER)
        if library.get_material_default_scalar_parameter_value(m, "KitVersion") >= KIT_VERSION - 0.001:
            return m
        library.delete_all_material_expressions(m)
    else:
        folder, name = MASTER.rsplit("/", 1)
        m = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_skeletal_mesh", True)
    colour = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0, parameter_name="Farbe")
    normal = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 300, parameter_name="Normal",
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 600, parameter_name="ORM",
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    # the defaults must match the sampler types, or the master does not compile (the engine's default texture is a colour)
    for node, key in ((colour, "Farbe"), (normal, "Normal"), (orm, "ORM")):
        default = "{}/Materialien/Texturen/T_Ing_AJ_{}".format(AJ, key)
        if assets.does_asset_exist(default):
            node.set_editor_property("texture", unreal.load_asset(default))
    library.connect_material_property(colour, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    library.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    expr(m, unreal.MaterialExpressionScalarParameter, -300, 800, parameter_name="KitVersion", default_value=KIT_VERSION)
    library.recompile_material(m)
    assets.save_loaded_asset(m, False)
    return m


def import_texture(png, folder, name, normal):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(folder + "/" + name)
    if tex is None:
        return None
    if normal:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    elif name.endswith("_ORM"):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    # worn and carried at runtime: always fully loaded, not left on a blurred low mip
    tex.set_editor_property("never_stream", True)
    assets.save_loaded_asset(tex, False)
    return tex


def instance(parent, source_dir, prefix, folder, mi_name):
    """T_<prefix>_{Farbe,Normal,ORM}.png -> textures and MI_<mi_name> in folder"""
    mi_path = "{}/{}".format(folder, mi_name)
    mi = unreal.load_asset(mi_path) if assets.does_asset_exist(mi_path) else tools.create_asset(
        mi_name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(mi, parent)
    for key in ("Farbe", "Normal", "ORM"):
        png = os.path.join(source_dir, "{}_{}.png".format(prefix, key))
        if not os.path.exists(png):
            log("missing " + png)
            continue
        tex = import_texture(png, folder + "/Texturen", "{}_{}".format(prefix, key), key == "Normal")
        if tex is not None:
            library.set_material_instance_texture_parameter_value(mi, key, tex)
    assets.save_loaded_asset(mi, False)
    return mi


def set_materials(mesh, mi):
    materials = list(mesh.get_editor_property("materials" if isinstance(mesh, unreal.SkeletalMesh) else "static_materials"))
    for slot in materials:
        slot.set_editor_property("material_interface", mi)
    mesh.set_editor_property("materials" if isinstance(mesh, unreal.SkeletalMesh) else "static_materials", materials)


def import_rig(mi, rig=RIG, folder=AJ, prefix="AJ"):
    skeleton = None
    # the Glutkrone's sting has a chain of its own (26.11.), with its own skeleton
    extra = ("Stachel",) if os.path.exists(os.path.join(rig, "SKM_Ing_{}_Stachel.fbx".format(prefix))) else ()
    for piece in ("Beine", "Brust", "Arme", "Kopf") + extra:
        name = "SKM_Ing_{}_{}".format(prefix, piece)
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", True)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("import_animations", False)
        ui.set_editor_property("create_physics_asset", False)
        if skeleton is not None and piece != "Stachel":
            ui.set_editor_property("skeleton", skeleton)
        ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(rig, name + ".fbx"))
        task.set_editor_property("destination_path", folder + "/Koerper")
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
        mesh = unreal.load_asset("{}/Koerper/{}".format(folder, name))
        if mesh is None:
            log("import failed: " + name)
            continue
        build = unreal.SkeletalMeshEditorSubsystem.get_lod_build_settings(mesh, 0)
        build.set_editor_property("use_full_precision_u_vs", True)
        unreal.SkeletalMeshEditorSubsystem.set_lod_build_settings(mesh, 0, build)
        if skeleton is None and piece != "Stachel":
            skeleton = mesh.get_editor_property("skeleton")
        set_materials(mesh, mi)
        assets.save_loaded_asset(mesh, False)
        log("{}: skeleton {}".format(name, mesh.get_editor_property("skeleton").get_name()))


def import_prop(name, mi):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    target = "SM_Ing_{}_Tripo".format(name)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(PROPS, "SM_Ing_{}.fbx".format(name)))
    task.set_editor_property("destination_path", WS)
    task.set_editor_property("destination_name", target)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset("{}/{}".format(WS, target))
    if mesh is None:
        log("import failed: " + target)
        return
    set_materials(mesh, mi)
    assets.save_loaded_asset(mesh, False)
    log("{}: ok".format(target))


parent = master()
if not os.environ.get("VAEL_NUR_STUFE2"):
    import_rig(instance(parent, os.path.join(RIG, "Texturen"), "T_Ing_AJ", AJ + "/Materialien", "MI_Ing_Aschenjaeger"))
for prop in (() if os.environ.get("VAEL_NUR_STUFE2") else ("Doppelsichel", "Quarzbrille")):
    import_prop(prop, instance(parent, PROPS, "T_Ing_" + prop, WS + "/Materialien", "MI_Ing_{}_Tripo".format(prop)))
log("fertig")

# Stage II (26.11.): the Aschenjaeger II and the Glutkrone (Tools/Ingenieur/vael_aschenjaeger_rig.py RIG_PROFILE=AJ2 / GK)
GK = "/Game/Vael/Characters/Ingenieur/Glutkrone"
STAGE2 = [
    ("C:/Projekte/Vael_Quellen/Ingenieur/Aschenjaeger/05_rig_stufe2", AJ, "AJ2", "MI_Ing_Aschenjaeger2"),
    ("C:/Projekte/Vael_Quellen/Ingenieur/Glutkrone/05_rig", GK, "GK", "MI_Ing_Glutkrone"),
]
for rig_dir, folder, prefix, mi_name in STAGE2:
    import_rig(instance(parent, os.path.join(rig_dir, "Texturen"), "T_Ing_" + prefix, folder + "/Materialien", mi_name), rig_dir, folder, prefix)
log("Stufe II fertig")
