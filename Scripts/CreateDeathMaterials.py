# Creates the overlay looks of the deaths of creatures (Boris 2026-11-27: "durch Feuer werden sie verkohlt"), used by
# UVaelDeathMotion (Source/Vael/Creatures/VaelDeathMotion.cpp) as overlay material over every mesh of a corpse.
#
# - M_Tod_Verkohlt: charcoal black with glowing cracks of embers. "Deckung" 0..1 how much it covers, "Glut" 0..1 the embers.
# - M_Tod_Eis: pale blue frost with a bright rim. "Deckung" 0..1.
# - M_Tod_Nid: violet glowing mist with a dark core. "Deckung", "Glut".
# - M_Treffer_Blitz: the short flash over a hit body (UVaelHitFeedbackSubsystem). "Farbe", "Staerke".
# All three are translucent overlays: the body stays under them, the camera sees it covered.
# Never overwrites: a material that exists is left alone. VAEL_TOD_NEU=1 makes them again.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateDeathMaterials.py" -unattended -nosplash

import os

import unreal

FOLDER = "/Game/Vael/VFX/Tod"

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log_warning("[Tod] " + str(message))


def new_material(name):
    path = "{}/{}".format(FOLDER, name)
    if assets.does_asset_exist(path):
        if not os.environ.get("VAEL_TOD_NEU"):
            log("{} gibt es schon".format(path))
            return None
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
        return material
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def expr(material, cls, x, y, **props):
    node = library.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def scalar(material, name, value, x, y):
    return expr(material, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def const3(material, rgb, x, y):
    return expr(material, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def mul(material, a, b, x, y, a_out="", b_out=""):
    node = expr(material, unreal.MaterialExpressionMultiply, x, y)
    library.connect_material_expressions(a, a_out, node, "A")
    library.connect_material_expressions(b, b_out, node, "B")
    return node


def lerp(material, a, b, alpha, x, y):
    node = expr(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    library.connect_material_expressions(a, "", node, "A")
    library.connect_material_expressions(b, "", node, "B")
    library.connect_material_expressions(alpha, "", node, "Alpha")
    return node


def noise(material, scale, x, y, levels=3):
    """World space noise 0..1 (object moves with the corpse well enough for a few seconds)"""
    position = expr(material, unreal.MaterialExpressionWorldPosition, x - 250, y)
    node = expr(material, unreal.MaterialExpressionNoise, x, y, scale=scale, levels=levels, output_min=0.0, output_max=1.0,
                noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU, quality=1)
    library.connect_material_expressions(position, "", node, "Position")
    return node


def power(material, base, exponent, x, y):
    node = expr(material, unreal.MaterialExpressionPower, x, y, const_exponent=exponent)
    library.connect_material_expressions(base, "", node, "Base")
    return node


def setup_translucent(material):
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)


def finish(material):
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    log("fertig: " + material.get_path_name())


def verkohlt():
    m = new_material("M_Tod_Verkohlt")
    if m is None:
        return
    setup_translucent(m)
    cover = scalar(m, "Deckung", 1.0, -900, 500)
    glut = scalar(m, "Glut", 1.0, -900, 300)
    n = noise(m, 0.045, -900, -100)
    fine = noise(m, 0.2, -900, 100, 2)
    # charcoal: nearly black, a little grey where the noise is high
    coal = lerp(m, const3(m, (0.012, 0.011, 0.01), -600, -300), const3(m, (0.06, 0.055, 0.05), -600, -200), fine, -380, -250)
    library.connect_material_property(coal, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(m, unreal.MaterialExpressionConstant, -380, -120, r=0.95)
    library.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # embers: thin bright cracks where the coarse noise is near its peak, fading with Glut
    cracks = power(m, n, 7.0, -600, 0)
    ember = mul(m, mul(m, cracks, const3(m, (40.0, 9.0, 1.2), -600, 120), -380, 40), glut, -200, 100)
    library.connect_material_property(ember, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = mul(m, cover, expr(m, unreal.MaterialExpressionConstant, -600, 600, r=0.94), -380, 520)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)


def eis():
    m = new_material("M_Tod_Eis")
    if m is None:
        return
    setup_translucent(m)
    cover = scalar(m, "Deckung", 1.0, -900, 500)
    fresnel = expr(m, unreal.MaterialExpressionFresnel, -900, 200, exponent=2.5)
    n = noise(m, 0.08, -900, -100, 2)
    frost = lerp(m, const3(m, (0.35, 0.55, 0.75), -600, -300), const3(m, (0.85, 0.94, 1.0), -600, -200), n, -380, -250)
    library.connect_material_property(frost, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(expr(m, unreal.MaterialExpressionConstant, -380, -120, r=0.15), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(expr(m, unreal.MaterialExpressionConstant, -380, -60, r=0.8), "", unreal.MaterialProperty.MP_SPECULAR)
    rim = mul(m, fresnel, const3(m, (0.6, 0.85, 1.2), -600, 260), -380, 220)
    library.connect_material_property(rim, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    base = expr(m, unreal.MaterialExpressionConstant, -600, 600, r=0.72)
    opacity = mul(m, cover, base, -380, 520)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)


def nid():
    m = new_material("M_Tod_Nid")
    if m is None:
        return
    setup_translucent(m)
    cover = scalar(m, "Deckung", 1.0, -900, 500)
    glut = scalar(m, "Glut", 1.0, -900, 300)
    fresnel = expr(m, unreal.MaterialExpressionFresnel, -900, 120, exponent=1.6)
    n = noise(m, 0.06, -900, -100, 3)
    library.connect_material_property(const3(m, (0.05, 0.01, 0.08), -380, -250), "", unreal.MaterialProperty.MP_BASE_COLOR)
    swirl = lerp(m, fresnel, n, expr(m, unreal.MaterialExpressionConstant, -600, 40, r=0.5), -600, 120)
    light = mul(m, mul(m, swirl, const3(m, (6.0, 1.4, 12.0), -600, 220), -380, 160), glut, -200, 160)
    library.connect_material_property(light, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = mul(m, cover, expr(m, unreal.MaterialExpressionConstant, -600, 600, r=0.85), -380, 520)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)


def treffer():
    """The flash over a hit body (UVaelHitFeedbackSubsystem): bright at the rim, "Farbe", "Staerke" 0..1"""
    m = new_material("M_Treffer_Blitz")
    if m is None:
        return
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = expr(m, unreal.MaterialExpressionVectorParameter, -900, 0, parameter_name="Farbe", default_value=unreal.LinearColor(1.0, 0.9, 0.7, 1.0))
    strength = scalar(m, "Staerke", 1.0, -900, 300)
    fresnel = expr(m, unreal.MaterialExpressionFresnel, -900, 150, exponent=1.8)
    add = expr(m, unreal.MaterialExpressionAdd, -650, 150)
    library.connect_material_expressions(fresnel, "", add, "A")
    library.connect_material_expressions(expr(m, unreal.MaterialExpressionConstant, -800, 250, r=0.45), "", add, "B")
    light = mul(m, mul(m, color, add, -450, 60), mul(m, strength, expr(m, unreal.MaterialExpressionConstant, -650, 380, r=4.0), -450, 300), -250, 150)
    library.connect_material_property(light, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = mul(m, strength, expr(m, unreal.MaterialExpressionConstant, -650, 480, r=0.7), -250, 420)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)


verkohlt()
eis()
nid()
treffer()
