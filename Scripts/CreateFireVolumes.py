# Feuer-Baukasten in 3D: imports the simulated fire volumes (Tools/Feuer/feuer_volumen.py) as animated Sparse Volume Textures
# and builds the volume material UVaelFireComponent renders them with (UHeterogeneousVolumeComponent): a real body of flame
# and smoke instead of pictures on cards.
#
# - SVT_Feuer_<Kind>: the OpenVDB sequence <source>/<kind>/vdb/feuer_<kind>_0001.vdb ... Attributes A holds the density
#   (smoke), Attributes B the flame (the default assignment of the importer, in the order the grids are written).
#   Kinds: Lager (a campfire), Glut, Brand, Gross (the stages a Brandherd grows through) and Kugel.
# - M_Feuer_Volumen: volume material. The flame glows deep red at its edges, orange in its body and yellow-white in its core;
#   the smoke darkens and absorbs. Two drifting noises blow the volume sideways, more towards the tips (Wehen, Tempo), so the
#   tongues wind and whip. Parameters: Leuchtkraft, Rauch, Staerke (fade in/out), Dichte (set by the game for the size),
#   Wehen, Tempo, Volumen (the texture).
# - MI_Feuer_Volumen_<Kind>: one instance per kind.
# Creates what is missing; the material is rebuilt when it is older than KIT_VERSION, a volume when -FeuerNeu is given.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateFireVolumes.py <source folder>"

import os
import sys
import unreal

FOLDER = "/Game/Vael/Effects/Feuer/Volumen"
KINDS = ("Lager", "Glut", "Brand", "Gross", "Kugel")
KIT_VERSION = 3.1
SOURCE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "Vael_Quellen", "Feuer3D")

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
made = []
notes = []


def log(message):
    unreal.log_warning("[CreateFireVolumes] " + message)


def import_volume(kind):
    name = "SVT_Feuer_" + kind
    path = FOLDER + "/" + name
    first = os.path.join(SOURCE, kind.lower(), "vdb", "feuer_{}_0001.vdb".format(kind.lower()))
    if not os.path.exists(first):
        notes.append("missing " + first)
        return unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if assets.does_asset_exist(path) and "-FeuerNeu" not in sys.argv:
        return unreal.load_asset(path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", first)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    # A sequence is named after its files, whatever the task says
    sequence_path = FOLDER + "/feuer_" + kind.lower()
    if assets.does_asset_exist(sequence_path):
        if assets.does_asset_exist(path):
            assets.delete_asset(path)
        assets.rename_asset(sequence_path, path)
    volume = unreal.load_asset(path)
    if volume is None:
        notes.append("import failed: " + first)
        return None
    frames = volume.get_num_frames() if hasattr(volume, "get_num_frames") else "?"
    made.append("{} ({} frames, {})".format(path, frames, type(volume).__name__))
    return volume


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

    def mask_out(self, source, column, output):
        """A named output of an expression with several, as a plain value"""
        expression = self.node(unreal.MaterialExpressionMultiply, column, const_b=1.0)
        self.link(source, expression, "A", output)
        return expression

    def append(self, c, a, b):
        return self.op(unreal.MaterialExpressionAppendVector, c, a, b)

    def noise_at(self, texture, uv, c):
        """The middle channel of the fire noise; a volume has no screen derivatives, so it reads the full-size picture"""
        expression = self.node(unreal.MaterialExpressionTextureSample, c, texture=texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
                               mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL, const_mip_value=0)
        self.link(uv, expression, "UVs")
        return self.mask(expression, c - 1, "G")

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def mul(self, c, a, b): return self.op(unreal.MaterialExpressionMultiply, c, a, b)
    def add(self, c, a, b): return self.op(unreal.MaterialExpressionAdd, c, a, b)
    def sub(self, c, a, b): return self.op(unreal.MaterialExpressionSubtract, c, a, b)

    def sat(self, c, a):
        expression = self.node(unreal.MaterialExpressionSaturate, c)
        self.link(a, expression)
        return expression

    def ramp(self, c, x, low, high):
        return self.sat(c, self.mul(c + 1, self.sub(c + 2, x, self.const(low, c + 3)), self.const(1.0 / (high - low), c + 2)))

    def lerp(self, c, a, b, alpha):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, c)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression


def build_material(default_volume, noise):
    path = FOLDER + "/M_Feuer_Volumen"
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(p) for p in library.get_scalar_parameter_names(material)] and \
                library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
            return material
        library.delete_all_material_expressions(material)
        made.append(path + " (graph rebuilt)")
    else:
        material = tools.create_asset("M_Feuer_Volumen", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        made.append(path)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_VOLUME)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    # Without this usage the volume component falls back to the default material
    material.set_editor_property("used_with_heterogeneous_volumes", True)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 20)
    sample = g.node(unreal.MaterialExpressionSparseVolumeTextureSampleParameter, 14, parameter_name="Volumen")
    if default_volume is not None:
        sample.set_editor_property("sparse_volume_texture", default_volume)

    # Where in the volume this step of the ray is (as the engine works it out when nothing is given), then blown sideways by
    # two drifting noises: barely at the foot, more and more towards the tips, so the tongues wind and whip and never repeat
    world = g.node(unreal.MaterialExpressionWorldPosition, 24)
    local = g.node(unreal.MaterialExpressionTransformPosition, 23, transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                   transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    g.link(world, local)
    bounds = g.node(unreal.MaterialExpressionObjectLocalBounds, 23)
    uvw = g.node(unreal.MaterialExpressionDivide, 21)
    g.link(g.sub(22, local, g.mask_out(bounds, 23, "Min")), uvw, "A")
    g.link(g.mask_out(bounds, 22, "Extents"), uvw, "B")
    if noise is not None:
        time = g.mul(21, g.node(unreal.MaterialExpressionTime, 22), g.scalar("Tempo", 0.06, 22))
        u = g.mask(uvw, 20, "R")
        v = g.mask(uvw, 20, "G")
        w = g.mask(uvw, 20, "B")
        rising = g.sub(19, g.mul(19, w, g.const(0.9, 20)), time)
        push_x = g.noise_at(noise, g.append(18, g.mul(19, v, g.const(1.3, 20)), rising), 17)
        push_y = g.noise_at(noise, g.append(18, g.add(19, g.mul(19, u, g.const(1.3, 20)), g.const(0.37, 20)), g.mul(19, rising, g.const(1.13, 20))), 17)
        reach = g.mul(16, g.mul(17, w, w), g.scalar("Wehen", 0.03, 17))
        offset = g.append(15, g.append(16, g.mul(16, g.sub(17, push_x, g.const(0.5, 18)), reach), g.mul(16, g.sub(17, push_y, g.const(0.5, 18)), reach)), g.const(0.0, 16))
        # The first input of the sample is its coordinates (by name the editing library does not find it)
        g.link(g.add(15, uvw, offset), sample, "")
    else:
        g.link(uvw, sample, "")
    density = g.mask(sample, 12, "R", "Attributes A")
    flame = g.mask(sample, 12, "R", "Attributes B")

    # The flame is a body with an outline (Boris 2026-10-22: clean shapes like drawn fire, no glowing haze): beyond the
    # outline it is dense, so the eye sees its surface and not the light of the whole depth added up. It glows in the colours
    # of fire, deep red at its edge, orange in its body, yellow-white in its hottest core; the glow is as strong as the body is
    # dense, so a body of any size shines alike
    edge = g.vector("Rand", (0.45, 0.02, 0.0), 10)
    middle = g.vector("Mitte", (1.0, 0.2, 0.0), 10)
    core = g.vector("Kern", (1.0, 0.55, 0.1), 10)
    colour = g.lerp(6, g.lerp(7, edge, middle, g.ramp(8, flame, 0.25, 0.5)), core, g.ramp(7, flame, 0.6, 0.95))
    # Staerke fades the fire; Dichte is set by the game for the size of the fire
    strength = g.mul(5, g.scalar("Staerke", 1.0, 6), g.scalar("Dichte", 1.0, 6))
    outline = g.scalar("Kontur", 0.2, 9)
    body = g.sat(7, g.mul(8, g.sub(9, flame, outline), g.const(8.0, 9)))
    solid = g.mul(5, body, g.scalar("Koerper", 6.0, 6))
    glow = g.mul(4, g.mul(5, colour, g.mul(6, solid, g.scalar("Leuchtkraft", 1.3, 7))), strength)

    # A thin veil of smoke darkens above it
    extinction = g.mul(4, g.add(5, g.mul(6, density, g.scalar("Rauch", 0.8, 7)), solid), strength)
    albedo = g.node(unreal.MaterialExpressionConstant3Vector, 4, constant=unreal.LinearColor(0.004, 0.0035, 0.003, 1.0))

    library.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(extinction, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def make_instance(parent, kind, volume):
    name = "MI_Feuer_Volumen_" + kind
    path = FOLDER + "/" + name
    instance = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if instance is None:
        instance = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        library.set_material_instance_parent(instance, parent)
        made.append(path)
    if volume is not None:
        library.set_material_instance_sparse_volume_texture_parameter_value(instance, "Volumen", volume) if hasattr(library, "set_material_instance_sparse_volume_texture_parameter_value") \
            else notes.append("cannot set the volume on " + name + " from Python")
    assets.save_loaded_asset(instance, False)


def main():
    volumes = {kind: import_volume(kind) for kind in KINDS}
    material = build_material(volumes.get("Brand"), unreal.load_asset("/Game/Vael/Effects/Feuer/T_Feuer_Rauschen"))
    for kind in KINDS:
        make_instance(material, kind, volumes[kind])
    for line in made:
        log("made " + line)
    for line in notes:
        log("NOTE " + line)
    log("done")


main()
