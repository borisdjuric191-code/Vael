# Die echten Knochen aus dem Fab-Skelett ins Spiel (Tools/Welt/vael_knochen_fab.py; "Human Skeleton" von Salt Lake Manor, CC-BY,
# Namensnennung in den Credits). Boris 16.11.2026: Platzhalter ersetzen, beim Magier Knochensturm und andere Knochenteile.
#
# - /Game/Vael/Requisiten/Knochen: die drei Fab-Texturen, M_Vael_Gebein (Farbe x Tönung, ORM, Normal, Leuchten für Zauber),
#   MI_Gebein_Hoehle (gealtert, dunkler: das helle Weiß stach in der Gebeinkammer heraus) und MI_Gebein_Zauber
# - SM_Knochen_<Teil>: einzelne Knochen für Knochensturm, Knochendornen und Markkäfig (keine Kollision)
# - SM_Narbenwall neu: Fleisch mit echten Rippen, Langknochen und Schädel
# Die Höhlenteile (Schädelwand, Säule, Bogen, Haufen) holt Scripts/BuildHoehle.py selbst.
#
# Mit geschlossenem Editor:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportKnochenFab.py" -unattended -nosplash
import os
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
ZIEL = "/Game/Vael/Requisiten/Knochen"
QUELLE = "C:/Projekte/Vael_Quellen/Skelett_Fab/textures"
KNOCHEN = "C:/Projekte/Vael_Quellen/Knochen/export"
FORMEN = "C:/Projekte/Vael/Scripts/Data/Zauber"


def log(text):
    unreal.log_warning("[KnochenFab] " + text)


def textur(datei, name, art):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(QUELLE, datei))
    task.set_editor_property("destination_path", ZIEL)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    tex = eal.load_asset(ZIEL + "/" + name)
    if art == "normal":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    elif art == "maske":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    eal.save_asset(ZIEL + "/" + name, only_if_is_dirty=False)
    return tex


def material(farbe, normal, orm):
    path = ZIEL + "/M_Vael_Gebein"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mat = tools.create_asset("M_Vael_Gebein", ZIEL, unreal.Material, unreal.MaterialFactoryNew())

    def n(cls, x, y, **props):
        e = mel.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def sample(tex, x, y, typ):
        e = n(unreal.MaterialExpressionTextureSample, x, y, texture=tex, sampler_type=typ)
        return e

    t_farbe = sample(farbe, -800, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    t_normal = sample(normal, -800, 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    t_orm = sample(orm, -800, 600, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    tonung = n(unreal.MaterialExpressionVectorParameter, -800, -250, parameter_name="Tonung", default_value=unreal.LinearColor(1, 1, 1, 1))
    mul = n(unreal.MaterialExpressionMultiply, -400, 0)
    mel.connect_material_expressions(t_farbe, "RGB", mul, "A")
    mel.connect_material_expressions(tonung, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(t_normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(t_orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(t_orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(t_orm, "B", unreal.MaterialProperty.MP_METALLIC)
    # Leuchten für die Zauber (Knochendornen glimmen violett): Glow x Leuchtfarbe, an den Kanten stärker
    glow = n(unreal.MaterialExpressionScalarParameter, -800, 900, parameter_name="Glow", default_value=0.0)
    gfarbe = n(unreal.MaterialExpressionVectorParameter, -800, 1000, parameter_name="Leuchtfarbe", default_value=unreal.LinearColor(0.6, 0.2, 1.0, 1))
    fres = n(unreal.MaterialExpressionFresnel, -800, 1150, exponent=2.5)
    a = n(unreal.MaterialExpressionMultiply, -500, 950)
    mel.connect_material_expressions(glow, "", a, "A")
    mel.connect_material_expressions(gfarbe, "", a, "B")
    b = n(unreal.MaterialExpressionAdd, -500, 1150)
    mel.connect_material_expressions(fres, "", b, "A")
    mel.connect_material_expressions(n(unreal.MaterialExpressionConstant, -650, 1250, r=0.25), "", b, "B")
    c = n(unreal.MaterialExpressionMultiply, -300, 1000)
    mel.connect_material_expressions(a, "", c, "A")
    mel.connect_material_expressions(b, "", c, "B")
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property("used_with_nanite", True)
    mel.recompile_material(mat)
    eal.save_asset(path, only_if_is_dirty=False)
    return mat


def instanz(name, parent, tonung):
    path = ZIEL + "/" + name
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, ZIEL, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_vector_parameter_value(mi, "Tonung", unreal.LinearColor(tonung[0], tonung[1], tonung[2], 1))
    eal.save_asset(path, only_if_is_dirty=False)
    return mi


def mesh(datei, ordner, name, materialien, nanite=False):
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
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", datei)
    task.set_editor_property("destination_path", ordner)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    sm = eal.load_asset(ordner + "/" + name)
    if sm is None:
        log("fehlt: " + datei)
        return None
    settings = sm.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", nanite)
    sm.set_editor_property("nanite_settings", settings)
    unreal.EditorStaticMeshLibrary.remove_collisions(sm)
    for i, slot in enumerate(sm.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        for key, value in materialien.items():
            if key in slot_name:
                sm.set_material(i, value)
    eal.save_asset(ordner + "/" + name, only_if_is_dirty=False)
    return sm


eal.make_directory(ZIEL)
farbe = textur("Skeleton_LP_MI_Skeleton01_LP_BaseColor.png", "T_Gebein_Farbe", "farbe")
normal = textur("Skeleton_LP_MI_Skeleton01_LP_Normal.png", "T_Gebein_Normal", "normal")
orm = textur("Skeleton_LP_MI_Skeleton01_LP_OcclusionRoughnessMetallic.png", "T_Gebein_ORM", "maske")
mat = material(farbe, normal, orm)
hoehle = instanz("MI_Gebein_Hoehle", mat, (0.6, 0.54, 0.45))
zauber = instanz("MI_Gebein_Zauber", mat, (1.0, 0.96, 0.88))

for datei in sorted(os.listdir(KNOCHEN)):
    if datei.endswith(".fbx"):
        mesh(os.path.join(KNOCHEN, datei), ZIEL, datei[:-4], {"Gebein": zauber})
        log("Knochen " + datei[:-4])

wall = mesh(os.path.join(FORMEN, "SM_Narbenwall.fbx"), "/Game/Vael/Effects/Flaechen", "SM_Narbenwall",
            {"Fleisch": eal.load_asset("/Game/Vael/Effects/Flaechen/M_Vael_Fleisch"), "Knochen": zauber})
log("Narbenwall slots {}".format([str(s.get_editor_property("material_slot_name")) for s in wall.get_editor_property("static_materials")]))
log("fertig")
