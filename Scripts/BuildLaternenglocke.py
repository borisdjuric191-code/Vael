# Laternenglocke (Boris 2026-11-02): the placeholder shapes give way to a Lady Fern from the Megascans pack with two or
# three Lichterblueten (the Tripo blossom of the Urbaum) hanging from the tips of its longest fronds.
#
# - Finds the hang points of each of the nine fern variants from their vertices: the outermost points of the fronds,
#   high enough above the ground for a blossom, spread around the plant.
# - M_Laternenbluete: the Tripo blossom glowing only in the color the game gives it ("Glut", "Leuchten"); the Urbaum's
#   material tints the orange petals, so violet would come out red.
# - Writes the hang points as Variants into DA_Harvest_Laternenglocke, with SM_Lichterbluete as the hanging part. AVaelHarvestable hangs, swings and colors the blossoms: warm yellow-white in a pure region, fading to
#   Nid violet between GlowTurnStart and GlowTurnEnd corruption; gathering takes the blossoms, the fern stays.
# Every placed Laternenglocke in every level follows the data, nothing is placed or moved. Safe to run again.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildLaternenglocke.py"

import math

import unreal

DATA = "/Game/Vael/Nature/DA_Harvest_Laternenglocke"
FERNS = "/Game/Fab/Megascans/Plants/Lady_Fern_wdvlditia/Raw/wdvlditia_tier_0/StaticMeshes/SM_wdvlditia_Var{}"
VARIANTS = "ABCDEFGHI"
BLOSSOM = "/Game/Vael/Requisiten/Wurzelforst/Lichterbluete/SM_Lichterbluete"
BLOSSOM_TEXTURE = "/Game/Vael/Requisiten/Wurzelforst/Lichterbluete/T_Lichterbluete_basecolor"
FOLDER = "/Game/Vael/Nature/Laternenglocke"

HANGING_SIZE = 30.0         # cm, length of one blossom; smaller ones vanish in the iso view
MAX_BLOSSOMS = 3
MIN_BLOSSOMS = 2
MIN_SPREAD = math.radians(75.0)
CLEARANCE = 10.0            # cm between the bottom of a hanging blossom and the ground
INWARD = 0.12               # share of the reach a hang point moves in from the very tip, where the frond still carries it
SAMPLES = 20000             # vertices looked at per fern
MIN_FERN_HEIGHT = 50.0      # cm


def log(message):
    unreal.log_warning("[BuildLaternenglocke] " + message)


def color(hex_code):
    def to_linear(channel):
        value = int(channel, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4

    return unreal.LinearColor(r=to_linear(hex_code[0:2]), g=to_linear(hex_code[2:4]), b=to_linear(hex_code[4:6]), a=1.0)


def blossom_material():
    """Petals as they are, lit from inside in the color of the light: the brightness of the picture times Glut"""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    library = unreal.MaterialEditingLibrary
    path = FOLDER + "/M_Laternenbluete"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    material = tools.create_asset("M_Laternenbluete", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("two_sided", True)

    def node(expression_class, x, y, **properties):
        expression = library.create_material_expression(material, expression_class, x, y)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    picture = node(unreal.MaterialExpressionTextureSample, -1200, 0, texture=unreal.load_asset(BLOSSOM_TEXTURE))
    grey = node(unreal.MaterialExpressionDesaturation, -900, 100)
    library.connect_material_expressions(picture, "RGB", grey, "")
    full = node(unreal.MaterialExpressionConstant, -1100, 250, r=1.0)
    library.connect_material_expressions(full, "", grey, "Fraction")
    glut = node(unreal.MaterialExpressionVectorParameter, -900, 300, parameter_name="Glut", default_value=unreal.LinearColor(1.0, 0.62, 0.25, 1.0))
    leuchten = node(unreal.MaterialExpressionScalarParameter, -900, 450, parameter_name="Leuchten", default_value=6.0)
    # Brighter petals glow more; lifted a little so the dark veins still glow faintly
    lifted = node(unreal.MaterialExpressionAdd, -650, 100)
    library.connect_material_expressions(grey, "", lifted, "A")
    library.connect_material_expressions(node(unreal.MaterialExpressionConstant, -800, 200, r=0.15), "", lifted, "B")
    tinted = node(unreal.MaterialExpressionMultiply, -450, 200)
    library.connect_material_expressions(lifted, "", tinted, "A")
    library.connect_material_expressions(glut, "", tinted, "B")
    glow = node(unreal.MaterialExpressionMultiply, -250, 250)
    library.connect_material_expressions(tinted, "", glow, "A")
    library.connect_material_expressions(leuchten, "", glow, "B")
    # By day the petals show the light's color too, so a violet lantern reads violet in the sun
    base = node(unreal.MaterialExpressionLinearInterpolate, -250, 0)
    library.connect_material_expressions(picture, "RGB", base, "A")
    library.connect_material_expressions(tinted, "", base, "B")
    library.connect_material_expressions(node(unreal.MaterialExpressionConstant, -450, 50, r=0.65), "", base, "Alpha")
    library.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(node(unreal.MaterialExpressionConstant, -250, 450, r=0.6), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    log("made " + path)
    return material


def vertices(mesh):
    description = mesh.get_static_mesh_description(0)
    count = description.get_vertex_count()
    step = max(1, count // SAMPLES)
    points = []
    for index in range(0, count, step):
        position = description.get_vertex_position(unreal.VertexID(index))
        points.append((position.x, position.y, position.z))
    return points


def hang_points(points):
    """Outermost points first, each far enough around the plant from the ones taken; low tips touching the ground are left out"""
    lowest = HANGING_SIZE + CLEARANCE
    candidates = sorted((p for p in points if p[2] >= lowest), key=lambda p: -math.hypot(p[0], p[1]))
    chosen = []
    for x, y, z in candidates:
        angle = math.atan2(y, x)
        if all(abs((angle - other + math.pi) % (2 * math.pi) - math.pi) >= MIN_SPREAD for other, _ in chosen):
            chosen.append((angle, unreal.Vector(x * (1.0 - INWARD), y * (1.0 - INWARD), z)))
        if len(chosen) == MAX_BLOSSOMS:
            break
    return [point for _, point in chosen]


def main():
    data = unreal.load_asset(DATA)
    blossom = unreal.load_asset(BLOSSOM)
    if data is None or blossom is None:
        log("missing {} or {}".format(DATA, BLOSSOM))
        return

    variants = []
    for letter in VARIANTS:
        mesh = unreal.load_asset(FERNS.format(letter))
        if mesh is None:
            log("no fern variant " + letter)
            continue
        box = mesh.get_bounding_box()
        points = hang_points(vertices(mesh))
        log("Var{}: {:.0f} x {:.0f} x {:.0f} cm, {} hang points: {}".format(
            letter, box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z, len(points),
            ", ".join("({:.0f} {:.0f} {:.0f})".format(p.x, p.y, p.z) for p in points)))
        # Low ferns would carry the blossoms like heads too big for them
        if len(points) < MIN_BLOSSOMS or box.max.z - box.min.z < MIN_FERN_HEIGHT:
            continue
        variant = unreal.VaelHarvestVariant()
        variant.set_editor_property("mesh", mesh)
        variant.set_editor_property("hang_points", points)
        variants.append(variant)

    data.set_editor_property("variants", variants)
    data.set_editor_property("hanging_mesh", blossom)
    data.set_editor_property("hanging_material", blossom_material())
    data.set_editor_property("hanging_size", HANGING_SIZE)
    # Brighter blossoms burn out to white and lose their color
    data.set_editor_property("hanging_glow", 2.2)
    # Warm yellow-white light in a pure land, Nid violet where the Nid rules
    data.set_editor_property("glow_color", color("ffcf73"))
    data.set_editor_property("trait_color", color("a24dff"))
    unreal.EditorAssetLibrary.save_loaded_asset(data, False)
    log("done: {} variants in {}".format(len(variants), DATA))


main()
