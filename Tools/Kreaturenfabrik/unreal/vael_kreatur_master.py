"""
Vael – Kreaturenfabrik: legt das gemeinsame Kreatur-Master-Material an (nur wenn es noch fehlt).

  /Game/Vael/Materialien/M_Kreatur_Master
    Texturparameter  BaseColor, Normal, Roughness, Metallic   (Tripo-PBR-Export)
    Skalar           MarkStaerke (0 = rein, 1 = voll vom Mark gezeichnet)
    Farbe            MarkFarbe   (violett)

Erste Fassung: das Mark leuchtet über einen Kanten-Fresnel. Violette Adern und Glutkanten folgen
(Plan im Tab „Kreaturenfabrik“); die Parameter bleiben dabei gleich, Instanzen müssen nicht neu angelegt werden.

Ausführen ohne offenen Editor:
  UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael_Tools/unreal/vael_kreatur_master.py"
"""
import unreal

PFAD = "/Game/Vael/Materialien"
NAME = "M_Kreatur_Master"


def anlegen():
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(f"{PFAD}/{NAME}"):
        unreal.log(f"[Vael] {NAME} existiert schon, nichts geändert")
        return

    mel = unreal.MaterialEditingLibrary
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
    # Platzhalter-Texturen, damit das Material auch ohne Instanz gültig ist
    for node, standard in ((farbe, "/Engine/EngineResources/DefaultTexture"), (normal, "/Engine/EngineMaterials/DefaultNormal"),
                           (rauh, "/Engine/EngineResources/WhiteSquareTexture"), (metall, "/Engine/EngineResources/Black")):
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
    mel.connect_material_expressions(markfarbe, "", mal1, "B")
    mal2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 600)
    mel.connect_material_expressions(mal1, "", mal2, "A")
    mel.connect_material_expressions(fresnel, "", mal2, "B")
    mel.connect_material_property(mal2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mat.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(mat)
    eal.save_asset(f"{PFAD}/{NAME}")
    unreal.log(f"[Vael] {NAME} angelegt")


if __name__ == "__main__":
    anlegen()
