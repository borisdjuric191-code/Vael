"""
Vael – Kreaturenfabrik: legt das gemeinsame Kreatur-Master-Material an (nur wenn es noch fehlt).

  /Game/Vael/Materialien/M_Kreatur_Master
    Texturparameter  BaseColor, Normal, Roughness, Metallic   (Tripo-PBR-Export)
    Skalar           MarkStaerke (0 = rein, 1 = voll vom Mark gezeichnet), GlutStaerke (0 = kalt)
    Farbe            MarkFarbe (violett), GlutFarbe (orange)

Das Mark leuchtet über einen Kanten-Fresnel; GlutStaerke/GlutFarbe lassen orange Stellen der Grundfarbe glühen (Glutkriecher). Violette Adern folgen
(Plan im Tab „Kreaturenfabrik“); die Parameter bleiben dabei gleich, Instanzen müssen nicht neu angelegt werden.

Ausführen ohne offenen Editor:
  UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael_Tools/unreal/vael_kreatur_master.py"
"""
import unreal

PFAD = "/Game/Vael/Materialien"
NAME = "M_Kreatur_Master"


def anlegen():
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    if eal.does_asset_exist(f"{PFAD}/{NAME}"):
        # Neu aufbauen: die Parameter-Namen bleiben, Instanzen behalten ihre Werte
        mat = eal.load_asset(f"{PFAD}/{NAME}")
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PFAD, unreal.Material, unreal.MaterialFactoryNew())

    def textur(param, x, y, sampler):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("sampler_type", sampler)
        return node

    farbe = textur("BaseColor", -700, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = textur("Normal", -700, 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rauh = textur("Roughness", -700, 0, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    metall = textur("Metallic", -700, 150, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    # Platzhalter-Texturen, damit das Material auch ohne Instanz gültig ist; Rauheit und Metall brauchen lineare (nicht sRGB)
    for node, standard in ((farbe, "/Engine/EngineResources/DefaultTexture"), (normal, "/Engine/EngineMaterials/DefaultNormal"),
                           (rauh, "/Engine/ArtTools/RenderToTexture/Textures/127grey"), (metall, "/Engine/ArtTools/RenderToTexture/Textures/127grey")):
        tex = unreal.load_asset(standard)
        if tex is not None:
            node.set_editor_property("texture", tex)

    mel.connect_material_property(farbe, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rauh, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metall, "R", unreal.MaterialProperty.MP_METALLIC)

    # Mark: violettes Leuchten an den Kanten, stärker je mehr die Kreatur gezeichnet ist
    staerke = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 500)
    staerke.set_editor_property("parameter_name", "MarkStaerke")
    staerke.set_editor_property("default_value", 0.0)
    markfarbe = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 600)
    markfarbe.set_editor_property("parameter_name", "MarkFarbe")
    markfarbe.set_editor_property("default_value", unreal.LinearColor(0.55, 0.12, 1.0, 1.0))
    fresnel = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -450, 700)
    fresnel.set_editor_property("exponent", 3.0)
    mal1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 550)
    mel.connect_material_expressions(staerke, "", mal1, "A")
    mel.connect_material_expressions(markfarbe, "RGB", mal1, "B")
    mal2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 600)
    mel.connect_material_expressions(mal1, "", mal2, "A")
    mel.connect_material_expressions(fresnel, "", mal2, "B")
    # Glut: was in der Grundfarbe orange glüht (Risse, Drüsen, Augen), leuchtet je nach GlutStaerke
    # Maske = sättige((Rot - Blau) * 3 - 0,6): graue Kruste bleibt dunkel, beige Panzer fast auch
    glut = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 850)
    glut.set_editor_property("parameter_name", "GlutStaerke")
    glut.set_editor_property("default_value", 0.0)
    glutfarbe = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 950)
    glutfarbe.set_editor_property("parameter_name", "GlutFarbe")
    glutfarbe.set_editor_property("default_value", unreal.LinearColor(1.0, 0.45, 0.12, 1.0))
    rot = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -450, 820)
    rot.set_editor_property("r", True); rot.set_editor_property("g", False); rot.set_editor_property("b", False); rot.set_editor_property("a", False)
    mel.connect_material_expressions(farbe, "RGB", rot, "")
    blau = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -450, 880)
    blau.set_editor_property("r", False); blau.set_editor_property("g", False); blau.set_editor_property("b", True); blau.set_editor_property("a", False)
    mel.connect_material_expressions(farbe, "RGB", blau, "")
    diff = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -330, 850)
    mel.connect_material_expressions(rot, "", diff, "A")
    mel.connect_material_expressions(blau, "", diff, "B")
    verst = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -230, 850)
    verst.set_editor_property("const_b", 3.0)
    mel.connect_material_expressions(diff, "", verst, "A")
    versatz = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -150, 850)
    versatz.set_editor_property("const_b", 0.6)
    mel.connect_material_expressions(verst, "", versatz, "A")
    maske = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, -60, 850)
    mel.connect_material_expressions(versatz, "", maske, "")
    glut1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 30, 880)
    mel.connect_material_expressions(maske, "", glut1, "A")
    mel.connect_material_expressions(glut, "", glut1, "B")
    glut2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 120, 900)
    mel.connect_material_expressions(glut1, "", glut2, "A")
    mel.connect_material_expressions(glutfarbe, "RGB", glut2, "B")
    glut3 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 210, 900)
    mel.connect_material_expressions(glut2, "", glut3, "A")
    mel.connect_material_expressions(farbe, "RGB", glut3, "B")

    summe = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, 300, 700)
    mel.connect_material_expressions(mal2, "", summe, "A")
    mel.connect_material_expressions(glut3, "", summe, "B")
    mel.connect_material_property(summe, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mat.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(mat)
    eal.save_asset(f"{PFAD}/{NAME}")
    unreal.log(f"[Vael] {NAME} angelegt")


if __name__ == "__main__":
    anlegen()
