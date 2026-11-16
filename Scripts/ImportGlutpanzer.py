# Imports the Glutpanzer (Tools/Ingenieur/vael_glutpanzer_bauen.py, goal 16.11.): the 17 pieces of the engineer's armour of
# Glutkriecher and Aschenroller to /Game/Vael/Characters/Ingenieur/Glutpanzer, the textures of the 59 Tripo parts, a master
# material M_Ing_Glutpanzer (the texture as colour; where it is orange-red, the seams and cracks, it glows: GlutStaerke, and a
# breathing pulse) and an instance per part. Idempotent: the master is rebuilt when its KitVersion is older than KIT_VERSION.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportGlutpanzer.py" -unattended -nosplash
import glob
import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/04_export"
FOLDER = "/Game/Vael/Characters/Ingenieur/Glutpanzer"
KIT_VERSION = 1.2

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary


def log(message):
    unreal.log_warning("[ImportGlutpanzer] " + message)


def expr(m, cls, x, y, **props):
    n = library.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def op(m, cls, x, y, a, b=None, ai="A", bi="B"):
    n = expr(m, cls, x, y)
    library.connect_material_expressions(a, "", n, ai)
    if b is not None:
        library.connect_material_expressions(b, "", n, bi)
    return n


def master():
    path = FOLDER + "/M_Ing_Glutpanzer"
    if assets.does_asset_exist(path):
        m = unreal.load_asset(path)
        if library.get_material_default_scalar_parameter_value(m, "KitVersion") >= KIT_VERSION - 0.001:
            return m
        library.delete_all_material_expressions(m)
    else:
        m = tools.create_asset("M_Ing_Glutpanzer", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    # worn as skeletal meshes (the Glutpanzer as its own body)
    m.set_editor_property("used_with_skeletal_mesh", True)
    tex = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -1400, 0, parameter_name="Farbe")
    rgb = tex
    r = expr(m, unreal.MaterialExpressionComponentMask, -1150, 200, r=True, g=False, b=False, a=False)
    library.connect_material_expressions(tex, "RGB", r, "")
    bl = expr(m, unreal.MaterialExpressionComponentMask, -1150, 320, r=False, g=False, b=True, a=False)
    library.connect_material_expressions(tex, "RGB", bl, "")
    # glow where red stands well over blue: the orange-red seams; grey crust (red ~ blue) stays dark
    diff = op(m, unreal.MaterialExpressionSubtract, -950, 250, r, bl)
    k = op(m, unreal.MaterialExpressionMultiply, -800, 250, diff, expr(m, unreal.MaterialExpressionScalarParameter, -950, 400, parameter_name="GlutSchaerfe", default_value=3.5))
    k = op(m, unreal.MaterialExpressionSubtract, -650, 250, k, expr(m, unreal.MaterialExpressionScalarParameter, -800, 400, parameter_name="GlutSchwelle", default_value=0.75))
    mask = expr(m, unreal.MaterialExpressionSaturate, -500, 250)
    library.connect_material_expressions(k, "", mask, "")
    # slow breathing of the coals
    time = expr(m, unreal.MaterialExpressionTime, -950, 550)
    sine = expr(m, unreal.MaterialExpressionSine, -800, 550, period=5.0)
    library.connect_material_expressions(time, "", sine, "")
    pulse = op(m, unreal.MaterialExpressionAdd, -650, 550, op(m, unreal.MaterialExpressionMultiply, -700, 650, sine, expr(m, unreal.MaterialExpressionConstant, -850, 700, r=0.15)),
               expr(m, unreal.MaterialExpressionConstant, -800, 750, r=1.0))
    glow_color = op(m, unreal.MaterialExpressionMultiply, -500, 100, rgb, expr(m, unreal.MaterialExpressionScalarParameter, -700, 50, parameter_name="GlutStaerke", default_value=1.6))
    emissive = op(m, unreal.MaterialExpressionMultiply, -300, 200, op(m, unreal.MaterialExpressionMultiply, -400, 250, glow_color, mask), pulse)
    library.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(expr(m, unreal.MaterialExpressionConstant, -300, 450, r=0.72), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(expr(m, unreal.MaterialExpressionConstant, -300, 550, r=0.15), "", unreal.MaterialProperty.MP_METALLIC)
    expr(m, unreal.MaterialExpressionScalarParameter, -300, 700, parameter_name="KitVersion", default_value=KIT_VERSION)
    library.recompile_material(m)
    assets.save_loaded_asset(m, False)
    return m


def import_textures():
    textures = {}
    tasks = []
    for png in sorted(glob.glob(os.path.join(SOURCE, "Texturen", "GP_Teil_*.png"))):
        name = "T_" + os.path.splitext(os.path.basename(png))[0]
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", png)
        task.set_editor_property("destination_path", FOLDER + "/Texturen")
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    for task in tasks:
        name = task.get_editor_property("destination_name")
        tex = unreal.load_asset("{}/Texturen/{}".format(FOLDER, name))
        if tex is not None:
            # the armour is made at runtime as skeletal meshes, whose textures stayed on a blurred low mip: always fully loaded
            tex.set_editor_property("never_stream", True)
            assets.save_loaded_asset(tex, False)
            textures[name[2:]] = tex
    return textures


def instances(parent, textures):
    out = {}
    for part, tex in textures.items():
        path = "{}/Materialien/MI_{}".format(FOLDER, part)
        mi = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
            "MI_" + part, FOLDER + "/Materialien", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        library.set_material_instance_parent(mi, parent)
        library.set_material_instance_texture_parameter_value(mi, "Farbe", tex)
        assets.save_loaded_asset(mi, False)
        out[part] = mi
    return out


def import_pieces(mis):
    for fbx in sorted(glob.glob(os.path.join(SOURCE, "SM_Ing_GP_*.fbx"))):
        name = os.path.splitext(os.path.basename(fbx))[0]
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        data = ui.static_mesh_import_data
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("auto_generate_collision", False)
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
        hits = 0
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
            for part, mi in mis.items():
                if slot_name == part or slot_name.startswith(part + "_"):
                    mesh.set_material(index, mi)
                    hits += 1
                    break
        assets.save_loaded_asset(mesh, False)
        box = mesh.get_bounding_box()
        log("{}: {} slots, {} matched, box {:.0f}..{:.0f} / {:.0f}..{:.0f} / {:.0f}..{:.0f}".format(
            name, len(mesh.get_editor_property("static_materials")), hits, box.min.x, box.max.x, box.min.y, box.max.y, box.min.z, box.max.z))


parent = master()
mis = instances(parent, import_textures())
log("{} part materials".format(len(mis)))
import_pieces(mis)
log("fertig")
