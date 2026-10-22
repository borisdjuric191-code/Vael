# Feuer-Baukasten, 3D flame tongues: imports the tongue meshes (Tools/Feuer/feuer_zunge.py) and builds the material that
# makes them burn (Boris 2026-10-22: clean drawn flames, calm, out of the invisible roots; volumes looked like clouds).
#
# - SM_Feuer_Zunge_0..2: a tongue 100 cm high, foot at the origin; UV0 u around, v from foot to tip.
# - M_Feuer_Zunge: unlit, translucent. Two noises drift up the tongue; towards the tip they eat it away, so it licks and
#   tears off in pieces; the foot stays whole. Yellow-white in the middle of the body, orange, deep red at its outline and
#   tip. The tongue sways calmly (World Position Offset, more towards the tip), each instance in its own rhythm
#   (PerInstanceRandom). Parameters: Staerke (fade in/out), Leuchtkraft, Tempo, Reissen (how much of the tip tears off),
#   Schwanken (cm of sway at the tip), Rand/Mitte/Kern.
# Creates what is missing; the material is rebuilt when it is older than KIT_VERSION.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateFireTongues.py [source folder]"

import os
import sys
import unreal

FOLDER = "/Game/Vael/Effects/Feuer/Zunge"
NOISE = "/Game/Vael/Effects/Feuer/T_Feuer_Rauschen"
TONGUES = ("SM_Feuer_Zunge_0", "SM_Feuer_Zunge_1", "SM_Feuer_Zunge_2")
KIT_VERSION = 1.2
SOURCE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "Vael_Quellen", "Feuer3D", "zunge")

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
made = []
notes = []


def log(message):
    unreal.log_warning("[CreateFireTongues] " + message)


def import_tongue(name):
    path = FOLDER + "/" + name
    if assets.does_asset_exist(path) and "-FeuerNeu" not in sys.argv:
        return unreal.load_asset(path)
    fbx = os.path.join(SOURCE, name + ".fbx")
    if not os.path.exists(fbx):
        notes.append("missing " + fbx + " (run Tools/Feuer/feuer_zunge.py first)")
        return None
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
    task.set_editor_property("filename", fbx)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    if mesh is None:
        notes.append("import failed: " + name)
        return None
    nanite = mesh.get_editor_property("nanite_settings")
    if nanite.get_editor_property("enabled"):
        nanite.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", nanite)
    unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
    assets.save_loaded_asset(mesh, False)
    made.append("{} ({:.0f} cm high)".format(path, mesh.get_bounding_box().max.z))
    return mesh


class Graph:
    def __init__(self, material):
        self.material = material
        self.rows = {}

    def node(self, expression_class, column, **properties):
        row = self.rows.get(column, 0)
        self.rows[column] = row + 1
        expression = library.create_material_expression(self.material, expression_class, -300 * column, 140 * row)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def link(self, source, target, target_input="", source_output=""):
        if not library.connect_material_expressions(source, source_output, target, target_input):
            notes.append("link failed: {}.{} -> {}.{}".format(source.get_name(), source_output, target.get_name(), target_input))

    def scalar(self, name, value, column):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def const(self, value, column):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def vector(self, name, rgb, column):
        parameter = self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(parameter, column - 1, "RGB")

    def mask(self, source, column, channels, source_output=""):
        expression = self.node(unreal.MaterialExpressionComponentMask, column, r="R" in channels, g="G" in channels, b="B" in channels, a="A" in channels)
        self.link(source, expression, "", source_output)
        return expression

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def mul(self, c, a, b): return self.op(unreal.MaterialExpressionMultiply, c, a, b)
    def add(self, c, a, b): return self.op(unreal.MaterialExpressionAdd, c, a, b)
    def sub(self, c, a, b): return self.op(unreal.MaterialExpressionSubtract, c, a, b)
    def append(self, c, a, b): return self.op(unreal.MaterialExpressionAppendVector, c, a, b)

    def one(self, expression_class, c, a, **properties):
        expression = self.node(expression_class, c, **properties)
        self.link(a, expression)
        return expression

    def sat(self, c, a): return self.one(unreal.MaterialExpressionSaturate, c, a)
    def sin(self, c, a): return self.one(unreal.MaterialExpressionSine, c, a, period=6.283185)
    def cos(self, c, a): return self.one(unreal.MaterialExpressionCosine, c, a, period=6.283185)

    def ramp(self, c, x, low, high):
        return self.sat(c, self.mul(c + 1, self.sub(c + 2, x, self.const(low, c + 3)), self.const(1.0 / (high - low), c + 2)))

    def lerp(self, c, a, b, alpha):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, c)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression

    def sample(self, texture, uv, c, channel):
        expression = self.node(unreal.MaterialExpressionTextureSample, c, texture=texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        self.link(uv, expression, "UVs")
        return self.mask(expression, c - 1, channel)


def build_material(noise):
    path = FOLDER + "/M_Feuer_Zunge"
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(p) for p in library.get_scalar_parameter_names(material)] and \
                library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
            return material
        library.delete_all_material_expressions(material)
        made.append(path + " (graph rebuilt)")
    else:
        material = tools.create_asset("M_Feuer_Zunge", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        made.append(path)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("used_with_instanced_static_meshes", True)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 24)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 22)
    u = g.mask(uv, 21, "R")
    v = g.mask(uv, 21, "G")
    phase = g.node(unreal.MaterialExpressionPerInstanceRandom, 22)
    time = g.mul(20, g.node(unreal.MaterialExpressionTime, 21), g.scalar("Tempo", 0.35, 21))

    # Two noises drift up the tongue, each instance from its own place
    coarse = g.sample(noise, g.append(18, g.add(19, u, g.mul(20, phase, g.const(7.0, 21))), g.sub(19, g.mul(20, v, g.const(0.7, 21)), time)), 17, "G")
    fine = g.sample(noise, g.append(18, g.sub(19, g.mul(20, u, g.const(2.0, 21)), phase), g.sub(19, g.mul(20, v, g.const(1.4, 21)), g.mul(20, time, g.const(1.6, 21)))), 17, "B")
    n = g.add(15, g.mul(16, coarse, g.const(0.7, 16)), g.mul(16, fine, g.const(0.3, 16)))

    # Towards the tip the noise eats the tongue away: it licks, and pieces tear off; the foot stays whole
    eaten = g.mul(13, g.sub(14, v, g.const(0.05, 15)), g.scalar("Reissen", 1.35, 15))
    whole = g.sat(11, g.mul(12, g.sub(13, n, eaten), g.const(7.0, 13)))
    opacity = g.mul(9, whole, g.scalar("Staerke", 1.0, 10))

    # Hot in the middle of the body, cooler at its outline and towards its tip
    fresnel = g.node(unreal.MaterialExpressionFresnel, 14, exponent=1.6)
    # The hottest part is the belly, not the very foot, which sits in the ground
    body = g.mul(12, g.mul(13, g.sub(14, g.const(1.0, 15), fresnel), g.sub(14, g.const(1.0, 15), v)), g.sat(13, g.mul(14, v, g.const(5.0, 15))))
    heat = g.sat(10, g.add(11, g.mul(12, body, body), g.mul(12, g.sub(13, n, g.const(0.5, 14)), g.const(0.3, 13))))
    edge = g.vector("Rand", (0.55, 0.03, 0.0), 10)
    middle = g.vector("Mitte", (1.0, 0.22, 0.0), 10)
    core = g.vector("Kern", (1.0, 0.6, 0.12), 10)
    colour = g.lerp(6, g.lerp(7, edge, middle, g.ramp(8, heat, 0.12, 0.45)), core, g.ramp(7, heat, 0.55, 0.9))
    glow = g.mul(4, g.mul(5, colour, g.scalar("Leuchtkraft", 1.5, 6)), g.scalar("Staerke", 1.0, 6))

    # A calm sway, stronger towards the tip, each tongue in its own rhythm
    swing = g.mul(16, g.node(unreal.MaterialExpressionTime, 17), g.scalar("Schwung", 1.1, 17))
    beat = g.add(15, swing, g.add(16, g.mul(17, phase, g.const(6.283, 18)), g.mul(17, v, g.const(2.2, 18))))
    reach = g.mul(14, g.mul(15, v, v), g.scalar("Schwanken", 6.0, 15))
    sway_x = g.mul(12, g.sin(13, beat), reach)
    sway_y = g.mul(12, g.cos(13, g.mul(14, beat, g.const(0.73, 15))), g.mul(13, reach, g.const(0.6, 14)))
    offset = g.append(10, g.append(11, sway_x, sway_y), g.const(0.0, 11))

    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    library.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def main():
    for name in TONGUES:
        import_tongue(name)
    noise = unreal.load_asset(NOISE)
    if noise is None:
        notes.append("missing " + NOISE + " (run Scripts/CreateFireKit.py first)")
    else:
        build_material(noise)
    for line in made:
        log("made " + line)
    for line in notes:
        log("NOTE " + line)
    log("done")


main()
