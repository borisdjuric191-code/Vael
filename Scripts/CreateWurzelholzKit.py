# The root wood (Wurzelholz) of the plant dragons (Boris 2026-11-19): imports the meshes of Tools/Drache/wurzelholz.py and builds
# M_Wurzelholz, the one woody base every plant dragon shares; the plants only colour it (veins, moss) and adorn it. The material
# speaks the language of M_Halm (Wuchs, Wind, Spin, Schatten, Leuchten, Leuchtfarbe, Aufleuchten) so it can dress the plant arms and
# the rooting of her feet later.
#
# - Bark: the tiling dead pine wood (fibres along V), tinted by Rindenton; moss in patches of world noise, on the up side, in the
#   cracks and where vertex colour G asks for it
# - Veins: vertex colour B glows in Leuchtfarbe, partly in Akzentfarbe (the partner plant), pulsing slowly
# - Wuchs: grows from the base (UV 1 "Verlauf" X) like the stalks
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateWurzelholzKit.py"

import os
import unreal

FOLDER = "/Game/Vael/Characters/Floromant/Wurzelholz"
SOURCES = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "Vael_Quellen", "Wurzelholz")
BARK = "/Game/Megaplant_Library/Tree_Dead_Pine/Textures/T_Tree_Dead_Pine_tiling_C"
BARK_NAH = "/Game/Megaplant_Library/Tree_Dead_Pine/Textures/T_Tree_Dead_Pine_tiling_NAH"
MESHES = ["SM_Wurzelholz_Schaedel", "SM_Wurzelholz_Kiefer", "SM_Wurzelholz_Strang_1", "SM_Wurzelholz_Strang_2",
          "SM_Wurzelholz_Strang_3", "SM_Wurzelholz_Faser"]
VERSION = 2.6
# Size of the moss patches in the world (scale of the noise per cm)
g_noise_scale = 0.03

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
notes = []


def log(message):
    unreal.log_warning("[CreateWurzelholzKit] " + message)


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
            notes.append("link failed: {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))

    def scalar(self, name, value, column):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def const(self, value, column):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def vector(self, name, rgb, column):
        parameter = self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name,
                              default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(parameter, column - 1)

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def one(self, expression_class, column, a, output=""):
        expression = self.node(expression_class, column)
        self.link(a, expression, "", output)
        return expression

    def mul(self, column, a, b):
        return self.op(unreal.MaterialExpressionMultiply, column, a, b)

    def add(self, column, a, b):
        return self.op(unreal.MaterialExpressionAdd, column, a, b)

    def sub(self, column, a, b):
        return self.op(unreal.MaterialExpressionSubtract, column, a, b)

    def mask(self, source, column, r=True, g=True, b=True, a=False, output=""):
        expression = self.node(unreal.MaterialExpressionComponentMask, column, r=r, g=g, b=b, a=a)
        self.link(source, expression, "", output)
        return expression

    def lerp(self, a, b, alpha, column):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression

    def saturate(self, source, column):
        return self.one(unreal.MaterialExpressionSaturate, column, source)

    def grey(self, source, column):
        expression = self.node(unreal.MaterialExpressionDesaturation, column)
        self.link(source, expression)
        self.link(self.const(1.0, column + 1), expression, "Fraction")
        return expression

    def append(self, a, b, column):
        expression = self.node(unreal.MaterialExpressionAppendVector, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression


def import_mesh(name):
    path = FOLDER + "/" + name
    fbx = os.path.join(SOURCES, name + ".fbx")
    if not os.path.exists(fbx):
        notes.append("missing " + fbx + " (run Tools/Drache/wurzelholz.py first)")
        return None
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    data = ui.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
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
    unreal.EditorStaticMeshLibrary.set_generate_lightmap_uv(mesh, False)
    assets.save_loaded_asset(mesh, False)
    log("imported " + path)
    return mesh


def build_material():
    name = "M_Wurzelholz"
    path = FOLDER + "/" + name
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        names = [str(parameter) for parameter in library.get_scalar_parameter_names(material)]
        if "KitVersion" in names and library.get_material_default_scalar_parameter_value(material, "KitVersion") >= VERSION - 0.001:
            return material
        # Made anew rather than rebuilt: a graph rebuilt in the commandlet keeps the id of the material, and the game then loads
        # the old compiled shaders from the cache
        material = None
        assets.delete_asset(path)
    material = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.5)
    material.set_editor_property("used_with_spline_meshes", True)

    g = Graph(material)
    g.scalar("KitVersion", VERSION, 14)

    # Bark texture along the roots
    uv = g.mul(12, g.node(unreal.MaterialExpressionTextureCoordinate, 13, coordinate_index=0), g.scalar("Kachelung", 1.5, 13))
    bark = g.node(unreal.MaterialExpressionTextureSample, 11, texture=unreal.load_asset(BARK))
    g.link(uv, bark, "UVs")
    nah = g.node(unreal.MaterialExpressionTextureSample, 11, texture=unreal.load_asset(BARK_NAH),
                 sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_VIRTUAL_MASKS)
    g.link(uv, nah, "UVs")
    bark_rgb = g.mask(bark, 10)
    wood = g.mul(9, bark_rgb, g.vector("Rindenton", (1.05, 0.82, 0.58), 10))

    # Moss: more on the up side, in the colour of the plant's moss, grainy with the bark
    # Moss (G) and veins (B) come as vertex colour (UV channel 2 is taken over as the lightmap channel)
    vertex = g.node(unreal.MaterialExpressionVertexColor, 13)
    normal_ws = g.node(unreal.MaterialExpressionVertexNormalWS, 13)
    up = g.mask(normal_ws, 12, r=False, g=False, b=True)
    # Patches of moss over the wood, from noise in the world (the noise reads the world position itself; so arms and roots
    # get them too), plus what the mesh asks for
    patches = g.node(unreal.MaterialExpressionNoise, 12, scale=g_noise_scale, levels=3, output_min=0.0, output_max=1.0, quality=1)
    moss_raw = g.add(10, g.add(11, patches, g.mul(12, g.mask(vertex, 12, r=False, g=True, b=False), g.const(0.4, 13))),
                     g.mul(11, up, g.const(0.35, 12)))
    # ... and in the deep cracks of the bark, broken up by its grain
    crack = g.one(unreal.MaterialExpressionSaturate, 10, g.sub(11, g.const(0.55, 12), g.mask(nah, 12, r=True, g=False, b=False, output="A")))
    moss_raw = g.add(9, moss_raw, g.mul(10, crack, g.scalar("Rissmoos", 1.2, 11)))
    moss_edge = g.mul(8, g.sub(9, moss_raw, g.scalar("Moosgrenze", 0.8, 10)), g.const(4.0, 9))
    moss_mask = g.mul(6, g.saturate(moss_edge, 7), g.scalar("Moos", 1.0, 7))
    bark_grey = g.grey(bark_rgb, 9)
    moss_colour = g.mul(7, g.mul(8, bark_grey, g.const(2.2, 9)), g.vector("Moosfarbe", (0.2, 0.36, 0.05), 9))
    light = g.lerp(wood, moss_colour, moss_mask, 5)

    # The violet shadow form, as on the stalks
    shadow = g.scalar("Schatten", 0.0, 6)
    shadow_tone = g.mul(5, g.mul(6, g.grey(light, 7), g.vector("Schattenton", (0.48, 0.28, 0.70), 7)), g.const(1.6, 6))
    albedo = g.lerp(light, shadow_tone, shadow, 4)

    # Veins glow in the colour of the plant, where the bark is high in the colour of the partner
    vein = g.mask(vertex, 11, r=False, g=False, b=True)
    height = g.mask(nah, 10, r=True, g=False, b=False, output="A")
    accent_mask = g.mul(8, g.saturate(g.mul(9, g.sub(10, height, g.const(0.5, 11)), g.const(6.0, 10)), 9), g.scalar("Akzentanteil", 0.0, 9))
    vein_colour = g.lerp(g.vector("Leuchtfarbe", (1.0, 0.35, 0.06), 9), g.vector("Akzentfarbe", (0.3, 1.0, 0.85), 9), accent_mask, 7)
    vein_colour = g.lerp(vein_colour, g.vector("Schattenleuchten", (0.70, 0.35, 1.0), 8), shadow, 6)
    time = g.node(unreal.MaterialExpressionTime, 12)
    world = g.node(unreal.MaterialExpressionWorldPosition, 13)
    world_z = g.mask(world, 12, r=False, g=False, b=True)
    pulse_phase = g.add(10, g.mul(11, time, g.scalar("Pulstempo", 1.8, 12)), g.mul(11, world_z, g.const(0.03, 12)))
    pulse = g.add(8, g.const(0.65, 9), g.mul(9, g.one(unreal.MaterialExpressionSine, 10, pulse_phase), g.const(0.35, 10)))
    glow = g.mul(4, g.mul(5, g.mul(6, vein_colour, vein), pulse), g.mul(5, g.scalar("Leuchten", 0.8, 6), g.const(1.0, 6)))

    # The firing flash runs to the tip, as on the stalks
    ramp = g.mask(g.node(unreal.MaterialExpressionTextureCoordinate, 12, coordinate_index=1), 11, r=True, g=False, b=False)
    ramp2 = g.mul(10, ramp, ramp)
    flash = g.mul(4, g.mul(5, vein_colour, g.mul(6, ramp2, ramp2)), g.mul(5, g.scalar("Aufleuchten", 0.0, 6), g.const(8.0, 6)))
    emissive = g.add(3, glow, flash)

    # Growing from the base to the tip
    grow = g.mul(5, g.scalar("Wuchs", 1.0, 6), g.const(1.06, 6))
    opacity = g.saturate(g.mul(3, g.sub(4, grow, ramp), g.const(40.0, 4)), 2)

    # Sway and spin like M_Halm (still by default)
    world_xy = g.mask(world, 10, r=True, g=True, b=False)
    dot = g.node(unreal.MaterialExpressionDotProduct, 9)
    g.link(world_xy, dot, "A")
    g.link(g.node(unreal.MaterialExpressionConstant2Vector, 10, r=1.0, g=1.0), dot, "B")
    place = g.mul(8, dot, g.const(0.004, 9))
    phase = g.add(7, g.add(8, g.mul(9, time, g.scalar("WindTempo", 1.6, 10)), g.scalar("Zeitversatz", 0.0, 9)), place)
    sway_x = g.one(unreal.MaterialExpressionSine, 6, phase)
    sway_y = g.mul(5, g.one(unreal.MaterialExpressionCosine, 6, g.mul(7, phase, g.const(0.8, 8))), g.const(0.6, 6))
    sway = g.append(g.append(sway_x, sway_y, 5), g.const(0.0, 5), 4)
    sway_offset = g.mul(3, sway, g.mul(4, g.mul(5, g.scalar("Wind", 0.0, 6), g.scalar("Schwingweite", 7.0, 6)), ramp2))
    rotate = g.node(unreal.MaterialExpressionRotateAboutAxis, 4)
    g.link(g.vector("ArmAchse", (0.0, 0.0, 1.0), 6), rotate, "NormalizedRotationAxis")
    g.link(g.mul(5, g.scalar("Spin", 0.0, 6), ramp), rotate, "RotationAngle")
    g.link(g.vector("ArmPunkt", (0.0, 0.0, 0.0), 6), rotate, "PivotPoint")
    g.link(world, rotate, "Position")
    offset = g.add(2, sway_offset, rotate)

    # Normal from the packed map (RG), occlusion from B
    # Deeper bark than the texture brings (Rindenrelief)
    normal_xy = g.mul(8, g.sub(9, g.mask(nah, 10, r=True, g=True, b=False), g.const(0.5, 10)), g.mul(9, g.const(2.0, 10), g.scalar("Rindenrelief", 2.2, 10)))
    derive = g.node(unreal.MaterialExpressionDeriveNormalZ, 7)
    g.link(normal_xy, derive, "InXY")
    occlusion = g.mask(nah, 6, r=False, g=False, b=True)

    library.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(derive, "", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(occlusion, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    library.connect_material_property(g.const(0.0, 2), "", unreal.MaterialProperty.MP_METALLIC)
    library.connect_material_property(g.scalar("Rauheit", 0.85, 2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    library.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    log("built " + path)
    return material


if not os.environ.get("VAEL_NUR_MATERIAL"):
    for mesh_name in MESHES:
        import_mesh(mesh_name)
build_material()
for note in notes:
    log("NOTE " + note)
log("done")
