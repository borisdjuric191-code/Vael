"""Material of the core of the fire beam (Feuerstrahl): a cylinder of concentrated fire, white in the middle,
yellow, orange and black towards the edge, churning. Used by AVaelSpellBeam through UVaelFireSettings.BeamCoreMaterial.
Run headless with the editor closed:
UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=Scripts/CreateFireBeam.py
Bump KIT_VERSION to rebuild it.
"""
import unreal

KIT_VERSION = "1.7"
PATH = "/Game/Vael/Effects/Feuer/M_Vael_Feuerstrahl"

# How fast the licks race along the beam, and how much they are stretched along it (lower is longer)
FLOW_SPEED = 7.5
STRETCH = 0.35

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def node(material, kind, x, y, **props):
    expression = mel.create_material_expression(material, kind, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(source, target, target_input, source_output=""):
    if not mel.connect_material_expressions(source, source_output, target, target_input):
        unreal.log_error("Vael: could not link {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))


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

    # Streaks of fire racing outwards: noise in the frame of the cylinder (U around it, V along it), stretched along
    # the beam so it tears into long licks, its repeats along the beam set by the beam from its length ("Kacheln")
    tiles = node(material, unreal.MaterialExpressionScalarParameter, -2000, 800, parameter_name="Kacheln", default_value=6.0)
    uv = node(material, unreal.MaterialExpressionTextureCoordinate, -2000, 600)
    around = node(material, unreal.MaterialExpressionComponentMask, -1800, 560, r=True, g=False, b=False, a=False)
    link(uv, around, "")
    along = node(material, unreal.MaterialExpressionComponentMask, -1800, 680, r=False, g=True, b=False, a=False)
    link(uv, along, "")
    around_scaled = node(material, unreal.MaterialExpressionMultiply, -1600, 560)
    link(around, around_scaled, "A")
    link(constant(material, 7.0, -1800, 760), around_scaled, "B")
    along_scaled = node(material, unreal.MaterialExpressionMultiply, -1600, 700)
    link(along, along_scaled, "A")
    link(tiles, along_scaled, "B")
    stretched = node(material, unreal.MaterialExpressionMultiply, -1450, 700)
    link(along_scaled, stretched, "A")
    link(constant(material, STRETCH, -1600, 820), stretched, "B")
    time = node(material, unreal.MaterialExpressionTime, -2000, 950)
    rush = node(material, unreal.MaterialExpressionMultiply, -1800, 950)
    link(time, rush, "A")
    link(node(material, unreal.MaterialExpressionScalarParameter, -2000, 1050, parameter_name="Tempo", default_value=FLOW_SPEED), rush, "B")
    racing = node(material, unreal.MaterialExpressionSubtract, -1300, 700)
    link(stretched, racing, "A")
    link(rush, racing, "B")
    churn = node(material, unreal.MaterialExpressionMultiply, -1800, 1100)
    link(time, churn, "A")
    link(constant(material, 0.6, -2000, 1150), churn, "B")
    flat = node(material, unreal.MaterialExpressionAppendVector, -1150, 620)
    link(around_scaled, flat, "A")
    link(racing, flat, "B")
    position = node(material, unreal.MaterialExpressionAppendVector, -1000, 700)
    link(flat, position, "A")
    link(churn, position, "B")
    noise = node(material, unreal.MaterialExpressionNoise, -850, 700, scale=1.0, levels=5, turbulence=True, output_min=-0.6, output_max=0.55)
    link(position, noise, "")

    # Heat: high in the middle line, torn into licks by the streaks
    heat_raw = node(material, unreal.MaterialExpressionAdd, -700, 100)
    boost = node(material, unreal.MaterialExpressionMultiply, -1100, 0)
    link(middle, boost, "A")
    link(constant(material, 0.75, -1300, 120), boost, "B")
    link(boost, heat_raw, "A")
    link(noise, heat_raw, "B")
    # "Hitze": each layer of the beam its own (the outer glow cool, the core white-hot)
    heated = node(material, unreal.MaterialExpressionMultiply, -650, 200)
    link(heat_raw, heated, "A")
    link(node(material, unreal.MaterialExpressionScalarParameter, -800, 300, parameter_name="Hitze", default_value=1.0), heated, "B")
    heat = node(material, unreal.MaterialExpressionSaturate, -600, 100)
    link(heated, heat, "")

    # Thirds of the heat: black to orange, orange to yellow, yellow to white
    thirds = node(material, unreal.MaterialExpressionMultiply, -700, 100)
    link(heat, thirds, "A")
    # (white only where the heat is nearly full: 2.4 instead of 3 thirds)
    link(constant(material, 2.4, -850, 220), thirds, "B")

    def band(offset, y):
        shifted = node(material, unreal.MaterialExpressionSubtract, -550, y)
        link(thirds, shifted, "A")
        link(constant(material, offset, -700, y + 80), shifted, "B")
        clamped = node(material, unreal.MaterialExpressionSaturate, -420, y)
        link(shifted, clamped, "")
        return clamped

    black = color(material, (0.0, 0.0, 0.0), -420, -300)
    orange = color(material, (2.0, 0.32, 0.02), -420, -200)
    yellow = color(material, (3.6, 1.6, 0.15), -420, -100)
    white = color(material, (6.0, 5.0, 3.8), -420, 400)

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
    thinned = node(material, unreal.MaterialExpressionMultiply, -100, 600)
    link(opacity_raw, thinned, "A")
    link(node(material, unreal.MaterialExpressionScalarParameter, -250, 700, parameter_name="Deckkraft", default_value=1.0), thinned, "B")
    opacity = node(material, unreal.MaterialExpressionSaturate, 0, 500)
    link(thinned, opacity, "")
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(material)
    unreal.EditorAssetLibrary.set_metadata_tag(material, "VaelKit", KIT_VERSION)
    if unreal.EditorAssetLibrary.save_loaded_asset(material, False):
        unreal.log("Vael: built {} ({})".format(PATH, KIT_VERSION))
    else:
        unreal.log_error("Vael: {} could not be saved".format(PATH))


build()
