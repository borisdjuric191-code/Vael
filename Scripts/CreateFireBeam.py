"""Material of the core of the fire beam (Feuerstrahl): a cylinder of concentrated fire, white in the middle,
yellow, orange and black towards the edge, churning. Used by AVaelSpellBeam through UVaelFireSettings.BeamCoreMaterial.
Run headless with the editor closed:
UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=Scripts/CreateFireBeam.py
Bump KIT_VERSION to rebuild it.
"""
import unreal

KIT_VERSION = "1.1"
PATH = "/Game/Vael/Effects/Feuer/M_Vael_Feuerstrahl"

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def node(material, kind, x, y, **props):
    expression = mel.create_material_expression(material, kind, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(source, target, target_input, source_output=""):
    mel.connect_material_expressions(source, source_output, target, target_input)


def constant(material, value, x, y):
    return node(material, unreal.MaterialExpressionConstant, x, y, r=value)


def color(material, rgb, x, y):
    return node(material, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def build():
    existing = unreal.load_asset(PATH)
    if existing is not None:
        if unreal.EditorAssetLibrary.get_metadata_tag(existing, "VaelKit") == KIT_VERSION:
            unreal.log("Vael: {} is up to date".format(PATH))
            return
    if existing is not None:
        material = existing
        mel.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_Vael_Feuerstrahl", "/Game/Vael/Effects/Feuer", unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", False)

    # How much the surface faces the eye: 1 on the middle line of the cylinder, 0 at its silhouette
    fresnel = node(material, unreal.MaterialExpressionFresnel, -1400, 0, exponent=2.6)
    middle = node(material, unreal.MaterialExpressionOneMinus, -1200, 0)
    link(fresnel, middle, "")

    # Churning fire: noise drifting through world space
    world = node(material, unreal.MaterialExpressionWorldPosition, -1800, 300)
    shrink = node(material, unreal.MaterialExpressionDivide, -1600, 300)
    link(world, shrink, "A")
    link(constant(material, 45.0, -1800, 420), shrink, "B")
    time = node(material, unreal.MaterialExpressionTime, -1800, 520)
    drift = node(material, unreal.MaterialExpressionMultiply, -1600, 520)
    link(time, drift, "A")
    link(constant(material, 3.0, -1800, 620), drift, "B")
    moved = node(material, unreal.MaterialExpressionAdd, -1400, 380)
    link(shrink, moved, "A")
    link(drift, moved, "B")
    noise = node(material, unreal.MaterialExpressionNoise, -1200, 380, scale=1.0, levels=4, turbulence=True, output_min=-0.5, output_max=0.45)
    link(moved, noise, "Position")

    # Heat: high in the middle, torn by the noise
    heat_raw = node(material, unreal.MaterialExpressionAdd, -1000, 100)
    boost = node(material, unreal.MaterialExpressionMultiply, -1100, 0)
    link(middle, boost, "A")
    link(constant(material, 0.95, -1300, 120), boost, "B")
    link(boost, heat_raw, "A")
    link(noise, heat_raw, "B")
    heat = node(material, unreal.MaterialExpressionSaturate, -850, 100)
    link(heat_raw, heat, "")

    # Thirds of the heat: black to orange, orange to yellow, yellow to white
    thirds = node(material, unreal.MaterialExpressionMultiply, -700, 100)
    link(heat, thirds, "A")
    link(constant(material, 3.0, -850, 220), thirds, "B")

    def band(offset, y):
        shifted = node(material, unreal.MaterialExpressionSubtract, -550, y)
        link(thirds, shifted, "A")
        link(constant(material, offset, -700, y + 80), shifted, "B")
        clamped = node(material, unreal.MaterialExpressionSaturate, -420, y)
        link(shifted, clamped, "")
        return clamped

    black = color(material, (0.0, 0.0, 0.0), -420, -300)
    orange = color(material, (3.2, 0.55, 0.03), -420, -200)
    yellow = color(material, (6.0, 3.2, 0.35), -420, -100)
    white = color(material, (10.0, 9.0, 7.0), -420, 400)

    first = node(material, unreal.MaterialExpressionLinearInterpolate, -200, -250)
    link(black, first, "A")
    link(orange, first, "B")
    link(band(0.0, 0), first, "Alpha")
    second = node(material, unreal.MaterialExpressionLinearInterpolate, -50, -150)
    link(first, second, "A")
    link(yellow, second, "B")
    link(band(1.0, 160), second, "Alpha")
    third = node(material, unreal.MaterialExpressionLinearInterpolate, 100, 0)
    link(second, third, "A")
    link(white, third, "B")
    link(band(2.0, 320), third, "Alpha")
    mel.connect_material_property(third, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Opaque where it burns, the black edge thinning into the air
    opacity_raw = node(material, unreal.MaterialExpressionMultiply, -200, 500)
    link(heat, opacity_raw, "A")
    link(constant(material, 2.2, -350, 600), opacity_raw, "B")
    opacity = node(material, unreal.MaterialExpressionSaturate, 0, 500)
    link(opacity_raw, opacity, "")
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(material)
    unreal.EditorAssetLibrary.set_metadata_tag(material, "VaelKit", KIT_VERSION)
    if unreal.EditorAssetLibrary.save_loaded_asset(material, False):
        unreal.log("Vael: built {} ({})".format(PATH, KIT_VERSION))
    else:
        unreal.log_error("Vael: {} could not be saved".format(PATH))


build()
