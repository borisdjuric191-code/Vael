"""Step 9: M_Kelchreisser_Sabber, the slobber of the Kelchreisser (Boris 2026-10-29: must be easy to see, but not
look like a cartoon). The water kit's materials are almost clear and vanish on drops this small; this one is a thick
slime: mostly opaque (a little light shows through), dark yellow-green, wet and mirror-glossy, with a faint glow at
its rims so threads and drops catch the eye in the dark forest.

Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>"
"""
import unreal

ZIEL = "/Game/Vael/Kreaturen/Kelchreisser"
NAME = "M_Kelchreisser_Sabber"
FARBE = (0.26, 0.29, 0.1)
SCHIMMER = 0.7   # glow at the rims (fresnel), times the colour
DECKUNG = 0.82   # how much of the light behind it the slime hides
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def ausdruck(material, klasse, x, y):
    return mel.create_material_expression(material, klasse, x, y)


def main():
    pfad = "{}/{}".format(ZIEL, NAME)
    if eal.does_asset_exist(pfad):
        material = eal.load_asset(pfad)
        mel.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, ZIEL, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

    farbe = ausdruck(material, unreal.MaterialExpressionConstant3Vector, -600, -100)
    farbe.set_editor_property("constant", unreal.LinearColor(*FARBE, 1.0))
    mel.connect_material_property(farbe, "", unreal.MaterialProperty.MP_BASE_COLOR)

    glanz = ausdruck(material, unreal.MaterialExpressionConstant, -600, 100)
    glanz.set_editor_property("r", 0.06)
    mel.connect_material_property(glanz, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spiegel = ausdruck(material, unreal.MaterialExpressionConstant, -600, 200)
    spiegel.set_editor_property("r", 1.0)
    mel.connect_material_property(spiegel, "", unreal.MaterialProperty.MP_SPECULAR)

    rand = ausdruck(material, unreal.MaterialExpressionFresnel, -600, 300)
    rand.set_editor_property("exponent", 2.5)
    staerke = ausdruck(material, unreal.MaterialExpressionConstant3Vector, -600, 420)
    staerke.set_editor_property("constant", unreal.LinearColor(FARBE[0] * SCHIMMER, FARBE[1] * SCHIMMER, FARBE[2] * SCHIMMER, 1.0))
    mal = ausdruck(material, unreal.MaterialExpressionMultiply, -300, 360)
    mel.connect_material_expressions(rand, "", mal, "A")
    mel.connect_material_expressions(staerke, "", mal, "B")
    mel.connect_material_property(mal, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    deckung = ausdruck(material, unreal.MaterialExpressionConstant, -600, 560)
    deckung.set_editor_property("r", DECKUNG)
    mel.connect_material_property(deckung, "", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(material)
    eal.save_asset(pfad, only_if_is_dirty=False)
    unreal.log_warning("[Kelchreisser] {} angelegt".format(pfad))


main()
