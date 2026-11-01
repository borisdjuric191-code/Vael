# Prepares the Harzkolben (Herbarium, Tripo model SM_Harzkolben) for the game. Nothing is placed yet.
#
# - Material M_WF_Harzkolben: the Tripo picture. The orange resin is found by its colour and glows like embers, slowly
#   pulsing, every plant in its own rhythm; the wine red hood gets a faint glow as if the embers shone through it.
#   The plant sways a little.
# - LODs on the mesh, textures at 2K except the normal map.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildHarzkolben.py" -EnablePlugins=PythonScriptPlugin

import unreal

FOLDER = "/Game/Vael/Requisiten/Wurzelforst/Harzkolben"
NAME = "Harzkolben"
MATERIAL_PATH = FOLDER + "/M_WF_Harzkolben"
MESH_HEIGHT = 200.0           # cm, as exported by the 'requisite' step

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
notes = []


def log(message):
    unreal.log_warning("[BuildHarzkolben] " + message)


class Graph:
    """Places material nodes in columns and checks every link (as in BuildRiesenpilze.py)"""

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
            notes.append("link failed: {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))

    def scalar(self, name, value, column):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def const(self, value, column):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def one(self, expression_class, column, a):
        expression = self.node(expression_class, column)
        self.link(a, expression)
        return expression

    def mul(self, column, a, b):
        return self.op(unreal.MaterialExpressionMultiply, column, a, b)

    def add(self, column, a, b):
        return self.op(unreal.MaterialExpressionAdd, column, a, b)

    def sub(self, column, a, b):
        return self.op(unreal.MaterialExpressionSubtract, column, a, b)

    def div(self, column, a, b):
        return self.op(unreal.MaterialExpressionDivide, column, a, b)

    def append(self, column, a, b):
        return self.op(unreal.MaterialExpressionAppendVector, column, a, b)

    def mask(self, source, column, r=True, g=True, b=True):
        expression = self.node(unreal.MaterialExpressionComponentMask, column, r=r, g=g, b=b, a=False)
        self.link(source, expression)
        return expression

    def saturate(self, source, column):
        return self.one(unreal.MaterialExpressionSaturate, column, source)

    def sine(self, source, column):
        return self.one(unreal.MaterialExpressionSine, column, source)

    def sample(self, picture, column, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        return self.node(unreal.MaterialExpressionTextureSample, column, texture=picture, sampler_type=sampler)

    def to_world(self, local, column):
        expression = self.node(unreal.MaterialExpressionTransform, column,
                               transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                               transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        self.link(local, expression)
        return expression


def texture(kind):
    path = "{}/T_{}_{}".format(FOLDER, NAME, kind)
    loaded = unreal.load_asset(path)
    if loaded is None:
        notes.append("missing texture " + path)
    return loaded


def fresh_material(path):
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
        return material
    folder, name = path.rsplit("/", 1)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def build_material():
    material = fresh_material(MATERIAL_PATH)
    material.set_editor_property("two_sided", True)
    g = Graph(material)
    linear = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR

    picture = g.sample(texture("basecolor"), 8)
    colour = g.mask(picture, 7)
    library.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    normal_map = g.sample(texture("normal"), 3, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    library.connect_material_property(normal_map, "RGB", unreal.MaterialProperty.MP_NORMAL)
    roughness = g.sample(texture("roughness"), 4, sampler=linear)
    library.connect_material_property(g.mask(roughness, 3, g=False, b=False), "", unreal.MaterialProperty.MP_ROUGHNESS)
    metallic = g.sample(texture("metallic"), 4, sampler=linear)
    library.connect_material_property(g.mask(metallic, 3, g=False, b=False), "", unreal.MaterialProperty.MP_METALLIC)

    red = g.mask(picture, 7, g=False, b=False)
    green = g.mask(picture, 7, r=False, b=False)
    blue = g.mask(picture, 7, r=False, g=False)

    # Resin: bright red with almost no blue. Bark and dry leaf edges stay below the threshold
    resin_raw = g.div(5, g.sub(6, g.sub(7, red, blue), g.scalar("HarzSchwelle", 0.46, 7)), g.scalar("HarzWeich", 0.2, 6))
    resin = g.saturate(resin_raw, 4)
    # Hood: red far above green, without the resin
    hood_raw = g.div(5, g.sub(6, g.sub(7, red, green), g.const(0.08, 7)), g.const(0.14, 6))
    hood = g.mul(3, g.saturate(hood_raw, 4), g.sub(4, g.const(1.0, 5), resin))

    # Slow pulse, every plant in its own rhythm
    place = g.node(unreal.MaterialExpressionObjectPositionWS, 8)
    seed = g.mul(6, g.mask(place, 7, b=False, g=False), g.const(0.013, 7))
    time = g.node(unreal.MaterialExpressionTime, 8)
    pulse_phase = g.add(5, g.mul(6, time, g.scalar("PulsTempo", 0.16, 7)), seed)  # Sine counts whole turns
    pulse = g.add(3, g.const(0.72, 4), g.mul(4, g.sine(pulse_phase, 5), g.const(0.28, 5)))

    ember = g.node(unreal.MaterialExpressionVectorParameter, 4, parameter_name="Glutfarbe",
                   default_value=unreal.LinearColor(1.0, 0.33, 0.06, 1.0))
    resin_glow = g.mul(2, g.mul(3, ember, resin), g.mul(3, pulse, g.scalar("Glut", 5.5, 4)))
    hood_glow = g.mul(2, g.mul(3, colour, hood), g.mul(3, pulse, g.scalar("Kapuzenschein", 1.2, 4)))
    library.connect_material_property(g.add(1, resin_glow, hood_glow), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # The plant sways a little, more the higher up
    local_z = g.mask(g.node(unreal.MaterialExpressionLocalPosition, 8), 7, r=False, g=False)
    height_share = g.div(6, local_z, g.const(MESH_HEIGHT, 7))
    sway_amount = g.mul(3, g.mul(4, height_share, height_share), g.scalar("Schwanken", 2.5, 4))
    sway_phase = g.add(4, g.mul(5, time, g.const(0.35, 6)), seed)
    sway = g.append(2, g.append(3, g.sine(sway_phase, 4), g.sine(g.mul(5, sway_phase, g.const(0.7, 6)), 4)), g.const(0.0, 3))
    library.connect_material_property(g.to_world(g.mul(1, sway, sway_amount), 1), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def lighten(mesh):
    """LODs for the far ones, textures at 2K except the normal map"""
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    if subsystem.get_lod_count(mesh) < 3:
        options = unreal.StaticMeshReductionOptions()
        options.set_editor_property("auto_compute_lod_screen_size", False)
        settings = []
        for percent, screen in ((1.0, 1.0), (0.4, 0.3), (0.12, 0.1)):
            setting = unreal.StaticMeshReductionSettings()
            setting.set_editor_property("percent_triangles", percent)
            setting.set_editor_property("screen_size", screen)
            settings.append(setting)
        options.set_editor_property("reduction_settings", settings)
        subsystem.set_lods(mesh, options)
    for kind in ("basecolor", "roughness", "metallic"):
        picture = texture(kind)
        if picture is not None and picture.get_editor_property("max_texture_size") != 2048:
            picture.set_editor_property("max_texture_size", 2048)
            assets.save_loaded_asset(picture, False)


def main():
    mesh = unreal.load_asset("{}/SM_{}".format(FOLDER, NAME))
    if mesh is None:
        log("SM_Harzkolben missing, run the import first")
        return
    material = build_material()
    mesh.set_material(0, material)
    lighten(mesh)
    assets.save_loaded_asset(mesh, False)
    for note in notes:
        log(note)
    log("done, {} notes".format(len(notes)))


main()
