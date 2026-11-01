# Imports the floating grimoire of the mage (Tools/Grimoire/vael_grimoire_bauen.py) and makes its material.
# Run headless with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=Scripts/CreateGrimoireKit.py
# Rerun-safe: parts and textures are imported anew, the material graph only when KIT_VERSION grew.

import os

import unreal

FOLDER = "/Game/Vael/Characters/Grimoire"
SOURCES = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Grimoire")
KIT_VERSION = 1.0

PARTS = ["SM_Grimoire_Vorderdeckel", "SM_Grimoire_Rueckdeckel", "SM_Grimoire_Ruecken", "SM_Grimoire_Block"]

INSTANCES = {
    "Leder": {"Textur": "T_Grimoire_Leder", "Tonung": (0.6, 0.6, 0.7), "Metall": 0.0, "Rauheit": 0.55},
    "Silber": {"Tonung": (0.74, 0.78, 0.86), "Metall": 0.45, "Rauheit": 0.38},
    "Stein": {"Tonung": (0.02, 0.4, 0.75), "Metall": 0.0, "Rauheit": 0.12, "Leuchten": 3.0, "Leuchtfarbe": (0.08, 0.55, 1.0)},
    "Seite": {"Textur": "T_Grimoire_Seite", "Tonung": (0.4, 0.33, 0.26), "Metall": 0.0, "Rauheit": 0.85, "Maske": 1.0, "Leuchten": 1.5, "Leuchtfarbe": (0.25, 0.6, 1.0)},
}

assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
notes = []


def log(message):
    unreal.log_warning("[CreateGrimoireKit] " + message)


def import_task(filename, name, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    if options is not None:
        task.set_editor_property("options", options)
    tools.import_asset_tasks([task])
    return unreal.load_asset("{}/{}".format(FOLDER, name))


def import_texture(name):
    texture = import_task(os.path.join(SOURCES, name + ".png"), name)
    if texture is None:
        notes.append("import failed: " + name)
        return None
    texture.set_editor_property("srgb", True)
    assets.save_loaded_asset(texture, False)
    return texture


def import_part(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    data = ui.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("generate_lightmap_u_vs", False)

    mesh = import_task(os.path.join(SOURCES, name + ".fbx"), name, ui)
    if mesh is None:
        notes.append("import failed: " + name)
        return None

    # Small and always close to the mage: no Nanite, nothing collides with it
    nanite = mesh.get_editor_property("nanite_settings")
    if nanite.get_editor_property("enabled"):
        nanite.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", nanite)
    unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
    return mesh


def build_material():
    path = "{}/M_Grimoire".format(FOLDER)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(parameter) for parameter in library.get_scalar_parameter_names(material)]:
            if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
                return material
        library.delete_all_material_expressions(material)
        for expression in list(unreal.ObjectIterator(unreal.MaterialExpression)):
            if expression.get_outer() == material:
                library.delete_material_expression(material, expression)
    else:
        material = tools.create_asset("M_Grimoire", FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("two_sided", True)
    row = [0]

    def node(expression_class, **properties):
        row[0] += 1
        expression = library.create_material_expression(material, expression_class, -400, 150 * row[0])
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def scalar(name, value):
        return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)

    def vector(name, rgb):
        return node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def link(source, target, target_input="", source_output=""):
        if not library.connect_material_expressions(source, source_output, target, target_input):
            notes.append("link failed: {} -> {}.{}".format(source.get_name(), target_input, target.get_name()))

    def multiply(a, b, a_output="", b_output=""):
        expression = node(unreal.MaterialExpressionMultiply)
        link(a, expression, "A", a_output)
        link(b, expression, "B", b_output)
        return expression

    # A white default: the silver and the stones only use their tint
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    texture = node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name="Textur", texture=white)
    tint = vector("Tonung", (1.0, 1.0, 1.0))
    library.connect_material_property(multiply(texture, tint, "RGB", "RGB"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(scalar("Metall", 0.0), "", unreal.MaterialProperty.MP_METALLIC)
    library.connect_material_property(scalar("Rauheit", 0.6), "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Glow everywhere, or with Maske 1 only where the alpha of the texture says
    mask = node(unreal.MaterialExpressionLinearInterpolate)
    link(node(unreal.MaterialExpressionConstant, r=1.0), mask, "A")
    link(texture, mask, "B", "A")
    link(scalar("Maske", 0.0), mask, "Alpha")
    glow = multiply(multiply(vector("Leuchtfarbe", (0.25, 0.6, 1.0)), scalar("Leuchten", 0.0), "RGB"), mask)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Only a marker for reruns, not wired
    scalar("KitVersion", KIT_VERSION)

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def make_instance(parent, name, values):
    path = "{}/MI_Grimoire_{}".format(FOLDER, name)
    if assets.does_asset_exist(path):
        instance = unreal.load_asset(path)
    else:
        instance = tools.create_asset("MI_Grimoire_" + name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, parent)
    for key, value in values.items():
        if key == "Textur":
            library.set_material_instance_texture_parameter_value(instance, key, unreal.load_asset("{}/{}".format(FOLDER, value)))
        elif isinstance(value, tuple):
            library.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
        else:
            library.set_material_instance_scalar_parameter_value(instance, key, value)
    library.update_material_instance(instance)
    assets.save_loaded_asset(instance, False)
    return instance


def main():
    assets.make_directory(FOLDER)
    import_texture("T_Grimoire_Seite")
    import_texture("T_Grimoire_Leder")
    parent = build_material()
    instances = {name: make_instance(parent, name, values) for name, values in INSTANCES.items()}

    for name in PARTS:
        mesh = import_part(name)
        if mesh is None:
            continue
        slots = mesh.get_editor_property("static_materials")
        for index, entry in enumerate(slots):
            slot_name = str(entry.get_editor_property("material_slot_name"))
            instance = next((value for key, value in instances.items() if key in slot_name), None)
            if instance is None:
                notes.append("{}: no material for slot {}".format(name, slot_name))
                continue
            mesh.set_material(index, instance)
        log("{}: slots {}".format(name, [str(entry.get_editor_property("material_slot_name")) for entry in slots]))
        assets.save_loaded_asset(mesh, False)

    # Whatever the importer brought along that nothing uses is listed, not deleted
    for path in assets.list_assets(FOLDER, recursive=False):
        short = path.split("/")[-1].split(".")[0]
        if not (short.startswith("SM_") or short.startswith("T_Grimoire") or short.startswith("MI_Grimoire") or short == "M_Grimoire"):
            notes.append("imported along: " + path)

    for note in notes:
        log("NOTE " + note)
    log("done, {} notes".format(len(notes)))


main()
