# Look test of the Glutkammer (refugium of the Aschenmark): imports the procedural pieces from Tools/Refugium/glutkammer_probe.py,
# gives them their materials and builds a small test level L_GlutkammerProbe (about 24 m across) with cave light.
#
# - Pieces: SM_GK_Rippenwand (ribs + membranes), SM_GK_Eisack (sac full of eggs), SM_GK_Gelegebeet (torn clutch sac as bed),
#   SM_GK_Wurzelvorhang
#   -> /Game/Vael/Refugium/Glutkammer/Probe
# - Materials: M_GK_Gewebe (petrified ribs), M_GK_Glut with MI_GK_Membran / MI_GK_Eisack (glowing tissue, slow pulse),
#   M_GK_Ader, M_GK_Schale, M_GK_Erde, M_GK_Wurzel, M_GK_Becken (glowing pool), M_GK_Boden
# - Level: floor, a ring of rocks (high on the far side, low towards the camera), five rib walls on the far half,
#   egg sacs in the membranes, four shell beds with plants around the middle, the root curtain over the pool,
#   light shafts from above in volumetric fog, PlayerStart.
# Only creates its own assets; the probe level is rebuilt from scratch on every run.
#
# Run with the editor closed (after the Blender script):
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildGlutkammerProbe.py" -unattended -nosplash -AllowCommandletRendering

import math
import os
import random

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Glutkammer/04_export"
FOLDER = "/Game/Vael/Refugium/Glutkammer/Probe"
LEVEL_PATH = "/Game/Vael/Maps/Test/L_GlutkammerProbe"
PIECES = ["SM_GK_Rippenwand", "SM_GK_Eisack", "SM_GK_Gelegebeet", "SM_GK_Wurzelvorhang", "SM_GK_Glutbecken", "SM_GK_Gelege_Schlafend",
          "SM_GK_Lichtstrahl", "SM_GK_Felswand_Hoch", "SM_GK_Felswand_Mittel", "SM_GK_Felswand_Niedrig"]
# pieces that stay plain meshes (translucent); the rest is built as Nanite
NO_NANITE = {"SM_GK_Lichtstrahl"}
ROCK_TEX = "/Game/Megascans/3D_Assets/IcelandicVolcanoTerrain/T_IcelandicVolcanoTerrain_A"
ROCK_NORMAL = "/Game/Megascans/3D_Assets/IcelandicVolcanoTerrain/T_IcelandicVolcanoTerrain_N"
MUD_TEX = "/Game/Megascans/Surfaces/HeavyMud/T_HeavyMud_A"
MUD_NORMAL = "/Game/Megascans/Surfaces/HeavyMud/T_HeavyMud_N"
BARK_TEX = "/Game/Fab/Megascans/3D/Old_Tree_Branch_whnnajh/Raw/whnnajh_tier_0/Textures/T_whnnajh_8K_B"
ROCKS = ["/Game/Rock_Collection_04/Meshes/Rock_0{0}/StaticMeshes/SM_Rock_0{0}".format(i) for i in range(1, 8)]
PLANTS = ["/Game/Vael/Requisiten/Aschenmark/{0}/SM_{0}".format(n) for n in ("Glutdistel", "Glimmklee", "Russrose", "Funkenmohn")]
SEED = 23

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
rng = random.Random(SEED)
notes = []


def log(message):
    unreal.log_warning("[GlutkammerProbe] " + message)


# ---------------------------------------------------------------- Import

def import_pieces():
    for name in PIECES:
        fbx = os.path.join(SOURCE, name + ".fbx")
        if not os.path.exists(fbx):
            raise FileNotFoundError(fbx + " fehlt - zuerst Tools/Refugium/glutkammer_probe.py in Blender laufen lassen")
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
        try:
            data.set_editor_property("build_nanite", name not in NO_NANITE)
        except Exception as error:
            notes.append("Nanite: {}".format(error))
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", fbx)
        task.set_editor_property("destination_path", FOLDER)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
        if unreal.load_asset("{}/{}".format(FOLDER, name)) is None:
            raise RuntimeError(name + " wurde nicht importiert")
    log("Teile importiert")


# ---------------------------------------------------------------- Materials

class Graph:
    """Places material nodes in columns and checks every link (as in BuildMangroven.py)"""

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

    def color(self, name, rgb, column):
        parameter = self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name,
                              default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(parameter, column - 1)

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def mul(self, column, a, b):
        return self.op(unreal.MaterialExpressionMultiply, column, a, b)

    def add(self, column, a, b):
        return self.op(unreal.MaterialExpressionAdd, column, a, b)

    def sub(self, column, a, b):
        return self.op(unreal.MaterialExpressionSubtract, column, a, b)

    def power(self, column, a, exponent):
        expression = self.node(unreal.MaterialExpressionPower, column)
        self.link(a, expression, "Base")
        self.link(exponent, expression, "Exp")
        return expression

    def mask(self, source, column, r=True, g=True, b=True):
        expression = self.node(unreal.MaterialExpressionComponentMask, column, r=r, g=g, b=b, a=False)
        self.link(source, expression)
        return expression

    def lerp(self, a, b, alpha, column):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression

    def saturate(self, source, column):
        expression = self.node(unreal.MaterialExpressionSaturate, column)
        self.link(source, expression)
        return expression

    def grey(self, source, column):
        expression = self.node(unreal.MaterialExpressionDesaturation, column)
        self.link(source, expression)
        self.link(self.const(1.0, column + 1), expression, "Fraction")
        return expression

    def vertex_r(self, column):
        """The mask the Blender script writes (vertex colour R, also in UV channel 1; the UV copy survives every import)"""
        return self.mask(self.node(unreal.MaterialExpressionTextureCoordinate, column + 1, coordinate_index=1), column, g=False, b=False)

    def uv(self, tiling_u, tiling_v, column):
        return self.node(unreal.MaterialExpressionTextureCoordinate, column, u_tiling=tiling_u, v_tiling=tiling_v)

    def sample(self, texture, uv, column, normal=False):
        """Texture sample on the mesh UVs; virtual textures get the matching sampler (the Megascans ones are virtual)"""
        virtual = texture is not None and texture.get_editor_property("virtual_texture_streaming")
        if normal:
            sampler = unreal.MaterialSamplerType.SAMPLERTYPE_VIRTUAL_NORMAL if virtual else unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        else:
            sampler = unreal.MaterialSamplerType.SAMPLERTYPE_VIRTUAL_COLOR if virtual else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
        expression = self.node(unreal.MaterialExpressionTextureSample, column, texture=texture, sampler_type=sampler)
        self.link(uv, expression, "UVs")
        return expression

    def noise(self, scale, column, function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU, levels=3, lo=0.0, hi=1.0, position=None):
        expression = self.node(unreal.MaterialExpressionNoise, column, scale=scale, levels=levels, output_min=lo, output_max=hi,
                               noise_function=function)
        self.link(position or self.node(unreal.MaterialExpressionWorldPosition, column + 1), expression)
        return expression

    def pulse(self, speed, column):
        """0.85..1.15, slow breathing that shifts with the height, so the tissue does not blink in step"""
        time = self.node(unreal.MaterialExpressionTime, column + 3)
        z = self.mask(self.node(unreal.MaterialExpressionWorldPosition, column + 4), column + 3, r=False, g=False, b=True)
        phase = self.add(column + 2, self.mul(column + 3, time, self.const(speed, column + 4)), self.mul(column + 3, z, self.const(0.004, column + 4)))
        sine = self.node(unreal.MaterialExpressionSine, column + 1, period=6.2832)
        self.link(phase, sine)
        return self.add(column, self.mul(column + 1, sine, self.const(0.15, column + 2)), self.const(1.0, column + 1))


def texture(path):
    loaded = unreal.load_asset(path)
    if loaded is None:
        notes.append("missing texture " + path)
    return loaded


def new_material(name, two_sided=False):
    path = "{}/{}".format(FOLDER, name)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("two_sided", two_sided)
    return material


def finish(material):
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def textured(g, tex_path, normal_path, tint_name, tint, tiling, column):
    """Greyed texture tinted by a colour parameter, plus its normal map; returns (colour, normal)"""
    uv = g.uv(tiling[0], tiling[1], column + 3)
    albedo = g.mul(column, g.grey(g.mask(g.sample(texture(tex_path), uv, column + 2), column + 1), column + 1),
                   g.color(tint_name, tint, column + 1))
    normal = g.sample(texture(normal_path), uv, column + 2, normal=True) if normal_path else None
    return albedo, normal


def material_tissue():
    """Petrified ribs: volcanic rock, warmed towards bone and amber, faint embers deep in the cracks"""
    m = new_material("M_GK_Gewebe")
    g = Graph(m)
    albedo, normal = textured(g, ROCK_TEX, ROCK_NORMAL, "GewebeFarbe", (0.62, 0.42, 0.3), (1.0, 2.0), 3)
    brightness = g.mul(2, albedo, g.scalar("Helligkeit", 1.6, 3))
    cracks = g.saturate(g.mul(3, g.sub(4, g.noise(0.04, 5, function=unreal.NoiseFunction.NOISEFUNCTION_VORONOI_ALU, levels=1),
                                       g.const(0.82, 5)), g.const(6.0, 4)), 2)
    glow = g.mul(1, g.mul(2, cracks, g.color("GlutFarbe", (1.0, 0.35, 0.08), 3)), g.scalar("Glut", 1.2, 2))
    library.connect_material_property(brightness, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if normal:
        library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(g.const(0.75, 1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finish(m)


def material_glow():
    """Glowing tissue (membranes, egg sacs, eggs): dark red at the rims, amber in the middle, branching veins, slow breathing"""
    m = new_material("M_GK_Glut", two_sided=True)
    g = Graph(m)
    mask = g.vertex_r(4)
    # Ridged noise: thin lines where the noise crosses zero, a coarse and a fine net, like veins instead of cells
    def ridges(scale, sharpness, column):
        n = g.noise(scale, column + 2, levels=3, lo=-1.0, hi=1.0)
        absolute = g.node(unreal.MaterialExpressionAbs, column + 1)
        g.link(n, absolute)
        return g.saturate(g.sub(column, g.const(1.0, column + 1), g.mul(column + 1, absolute, g.const(sharpness, column + 2))), column)
    veins = g.saturate(g.add(3, ridges(0.012, 9.0, 5),
                             g.mul(4, ridges(0.035, 7.0, 5), g.const(0.55, 5))), 3)
    base = g.lerp(g.color("RandFarbe", (0.1, 0.02, 0.015), 4), g.color("KernFarbe", (0.55, 0.2, 0.06), 4), mask, 2)
    base = g.lerp(base, g.color("AderFarbe", (0.18, 0.03, 0.02), 3), g.mul(3, veins, g.const(0.7, 4)), 1)
    fresnel = g.node(unreal.MaterialExpressionFresnel, 4, exponent=2.5)
    thick = g.sub(3, g.const(1.0, 4), g.mul(4, fresnel, g.scalar("RandDunkel", 0.6, 5)))
    strength = g.mul(2, g.power(3, mask, g.scalar("KernSchaerfe", 2.0, 4)), g.scalar("Leuchten", 6.0, 3))
    light = g.mul(1, g.mul(2, strength, thick), g.lerp(g.const(1.0, 3), g.const(0.35, 3), veins, 2))
    emissive = g.mul(0, g.mul(1, g.color("LeuchtFarbe", (1.0, 0.42, 0.1), 2), light), g.pulse(0.9, 2))
    library.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.scalar("Rauheit", 0.32, 1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(g.scalar("Glanz", 0.5, 1), "", unreal.MaterialProperty.MP_SPECULAR)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # light passes through the thin skin: two-sided foliage shading, warm where it shines through
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    library.connect_material_property(g.mul(1, g.color("DurchFarbe", (0.9, 0.3, 0.08), 2), g.scalar("Durchschein", 0.8, 2)), "",
                                      unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    finish(m)
    instances = {}
    for name, values, colours in (
            ("MI_GK_Membran", {"Leuchten": 2.5, "KernSchaerfe": 2.2}, {}),
            ("MI_GK_Eisack", {"Leuchten": 3.5, "KernSchaerfe": 3.2, "RandDunkel": 0.75, "Rauheit": 0.2, "Glanz": 0.8}, {}),
            # closed clutch on the floor: leathery but wet, eggs glowing through, the seam brightest
            ("MI_GK_Gelege_Schlafend", {"Leuchten": 0.4, "KernSchaerfe": 3.0, "RandDunkel": 0.7, "Rauheit": 0.35, "Glanz": 0.6, "Durchschein": 0.3},
             {"RandFarbe": (0.09, 0.035, 0.02), "KernFarbe": (0.5, 0.2, 0.07), "AderFarbe": (0.07, 0.02, 0.015)}),
            # leathery skin of the clutch sac: dark, only the egg bulges glow a little
            ("MI_GK_Gelegehaut", {"Leuchten": 1.1, "KernSchaerfe": 2.6, "Rauheit": 0.38, "Glanz": 0.6, "Durchschein": 0.5},
             {"RandFarbe": (0.07, 0.035, 0.025), "KernFarbe": (0.3, 0.13, 0.06), "AderFarbe": (0.05, 0.02, 0.015)}),
            # the small eggs: pale amber, glowing from inside
            ("MI_GK_Eier", {"Leuchten": 1.0, "KernSchaerfe": 1.8, "RandDunkel": 0.8, "Rauheit": 0.12, "Glanz": 1.0},
             {"RandFarbe": (0.3, 0.15, 0.07), "KernFarbe": (0.75, 0.45, 0.18), "LeuchtFarbe": (1.0, 0.52, 0.16)}),
            # the eggs of a sleeping clutch: darker amber, a dim warm glow deep inside
            ("MI_GK_Eier_Schlafend", {"Leuchten": 0.55, "KernSchaerfe": 2.4, "RandDunkel": 0.85, "Rauheit": 0.15, "Glanz": 1.0, "Durchschein": 0.5},
             {"RandFarbe": (0.16, 0.07, 0.035), "KernFarbe": (0.55, 0.28, 0.1), "LeuchtFarbe": (1.0, 0.42, 0.1)})):
        path = "{}/{}".format(FOLDER, name)
        mi = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
            name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        library.set_material_instance_parent(mi, m)
        for key, value in values.items():
            library.set_material_instance_scalar_parameter_value(mi, key, value)
        for key, rgb in colours.items():
            library.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        assets.save_loaded_asset(mi, False)
        instances[name] = mi
    return instances


def material_plain(name, rgb, roughness, emissive=None, noise_amount=0.25, two_sided=False):
    m = new_material(name, two_sided)
    g = Graph(m)
    n = g.noise(0.02, 3, lo=1.0 - noise_amount, hi=1.0 + noise_amount * 0.4)
    library.connect_material_property(g.mul(1, g.color("Farbe", rgb, 2), n), "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.const(roughness, 1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    if emissive:
        library.connect_material_property(g.color("Leuchten", emissive, 2), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finish(m)


def material_shell():
    """Chalky shell outside, dark and moist inside (vertex colour R = inside)"""
    m = new_material("M_GK_Schale")
    g = Graph(m)
    inside = g.vertex_r(3)
    stains = g.noise(0.05, 4, lo=0.75, hi=1.05)
    colour = g.lerp(g.color("Aussen", (0.62, 0.55, 0.43), 3), g.color("Innen", (0.08, 0.05, 0.04), 3), inside, 2)
    library.connect_material_property(g.mul(1, colour, stains), "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.lerp(g.const(0.85, 2), g.const(0.45, 2), inside, 1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finish(m)


def material_roots():
    """Dark red-brown bark; the tips that reach the glowing pool warm up a little (vertex colour R = 1 at the tips)"""
    m = new_material("M_GK_Wurzel")
    g = Graph(m)
    albedo, _ = textured(g, BARK_TEX, None, "RindeFarbe", (0.34, 0.17, 0.11), (1.0, 3.0), 3)
    tips = g.saturate(g.mul(3, g.sub(4, g.vertex_r(5), g.const(0.8, 5)), g.const(5.0, 4)), 2)
    glow = g.mul(1, g.mul(2, tips, g.color("SpitzenGlut", (1.0, 0.3, 0.06), 3)), g.scalar("Glut", 0.6, 2))
    library.connect_material_property(g.mul(1, albedo, g.scalar("Helligkeit", 1.4, 2)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.const(0.8, 1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finish(m)


def material_pool():
    """Glowing pool of cooling ember under the roots: slowly drifting bright and dark patches"""
    m = new_material("M_GK_Becken")
    g = Graph(m)
    time = g.node(unreal.MaterialExpressionTime, 7)
    drift = g.mul(6, time, g.node(unreal.MaterialExpressionConstant3Vector, 7, constant=unreal.LinearColor(9.0, 5.0, 3.0, 0.0)))
    position = g.add(5, g.node(unreal.MaterialExpressionWorldPosition, 6), drift)
    patches = g.noise(0.012, 4, levels=4, position=position)
    hot = g.power(3, patches, g.const(2.2, 4))
    colour = g.lerp(g.color("Dunkel", (0.35, 0.05, 0.01), 3), g.color("Hell", (1.0, 0.45, 0.1), 3), hot, 2)
    emissive = g.mul(1, g.mul(2, colour, g.vertex_r(3)), g.scalar("Leuchten", 2.5, 2))
    library.connect_material_property(g.color("Grund", (0.05, 0.02, 0.01), 2), "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.const(0.2, 1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finish(m)


def material_floor():
    """Floor in mid browns and ochre so fire, creatures and effects stay readable (Designbibel, Glutkammer)"""
    m = new_material("M_GK_Boden")
    g = Graph(m)
    albedo, normal = textured(g, MUD_TEX, MUD_NORMAL, "BodenFarbe", (0.42, 0.3, 0.21), (14.0, 14.0), 3)
    large = g.noise(0.004, 4, lo=0.75, hi=1.1)
    ash = g.saturate(g.mul(4, g.sub(5, g.noise(0.0025, 6, levels=2), g.const(0.6, 6)), g.const(4.0, 5)), 3)
    ground = g.lerp(g.mul(2, albedo, large), g.color("Asche", (0.33, 0.3, 0.27), 3), g.mul(3, ash, g.const(0.3, 4)), 1)
    library.connect_material_property(g.mul(1, ground, g.scalar("Helligkeit", 1.5, 2)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    if normal:
        library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(g.const(0.9, 1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    return finish(m)


def material_beam():
    """Light shaft: additive, unlit; bright in the middle and at the floor, fading towards the top and at the edges,
    with dust drifting down through it; soft where it meets the floor"""
    m = new_material("M_GK_Lichtstrahl")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    g = Graph(m)
    height = g.vertex_r(6)
    top = g.power(4, g.sub(5, g.const(1.0, 6), height), g.scalar("ObenWeich", 2.6, 5))
    foot = g.saturate(g.mul(4, height, g.const(25.0, 5)), 3)
    fresnel = g.node(unreal.MaterialExpressionFresnel, 6, exponent=1.0, base_reflect_fraction=0.0)
    middle = g.power(4, g.sub(5, g.const(1.0, 6), fresnel), g.scalar("Kante", 3.5, 5))
    time = g.node(unreal.MaterialExpressionTime, 9)
    fall = g.mul(8, time, g.node(unreal.MaterialExpressionConstant3Vector, 9, constant=unreal.LinearColor(4.0, 2.0, -22.0, 0.0)))
    stretched = g.mul(7, g.add(8, g.node(unreal.MaterialExpressionWorldPosition, 9), fall),
                      g.node(unreal.MaterialExpressionConstant3Vector, 8, constant=unreal.LinearColor(1.0, 1.0, 0.25, 0.0)))
    dust = g.noise(0.09, 5, levels=2, lo=0.7, hi=1.15, position=stretched)
    fade = g.node(unreal.MaterialExpressionDepthFade, 3, fade_distance_default=140.0)
    g.link(g.const(1.0, 4), fade, "Opacity")
    shape = g.mul(2, g.mul(3, g.mul(4, top, foot), middle), dust)
    emissive = g.mul(0, g.mul(1, g.mul(2, shape, fade), g.color("Farbe", (1.0, 0.74, 0.44), 2)), g.scalar("Staerke", 0.14, 1))
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finish(m)


def material_rock():
    """Cave wall of cooled lava: dark basalt with lighter rims, ash dusted on everything that faces up, darker bands of old
    lava layers (UV1 V), glowing cracks near the floor (UV1 U) that breathe slowly"""
    m = new_material("M_GK_Fels")
    g = Graph(m)
    albedo, normal = textured(g, ROCK_TEX, ROCK_NORMAL, "FelsFarbe", (0.3, 0.22, 0.18), (1.0, 1.0), 4)
    coarse = g.noise(0.006, 5, lo=0.7, hi=1.15)
    rock = g.mul(3, albedo, coarse)
    # UV1 masks only on the own wall pieces; MI_GK_Fels_Brocken turns them off for meshes whose UV1 is a lightmap
    masks_on = g.scalar("Masken", 1.0, 5)
    band = g.mul(5, g.mask(g.node(unreal.MaterialExpressionTextureCoordinate, 7, coordinate_index=1), 6, r=False, g=True, b=False), masks_on)
    rock = g.mul(3, rock, g.lerp(g.const(1.0, 5), g.const(0.45, 5), band, 4))
    up = g.mask(g.node(unreal.MaterialExpressionVertexNormalWS, 6), 5, r=False, g=False, b=True)
    ash_amount = g.mul(3, g.saturate(g.mul(4, g.sub(5, up, g.const(0.5, 6)), g.const(3.5, 5)), 4), g.noise(0.03, 5, lo=0.5, hi=1.0))
    albedo = g.lerp(rock, g.color("Asche", (0.24, 0.22, 0.2), 3), g.mul(3, ash_amount, g.const(0.7, 4)), 2)
    crack = g.mul(4, g.vertex_r(6), masks_on)
    albedo = g.mul(1, g.mul(2, albedo, g.sub(3, g.const(1.0, 4), g.mul(4, crack, g.const(0.7, 5)))), g.scalar("Helligkeit", 2.3, 2))
    glow = g.mul(1, g.mul(2, g.mul(3, g.power(4, crack, g.const(2.6, 5)), g.color("GlutFarbe", (1.0, 0.32, 0.06), 4)),
                          g.scalar("Glut", 1.6, 3)), g.pulse(0.6, 2))
    library.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if normal:
        library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(g.lerp(g.const(0.82, 2), g.const(0.95, 2), ash_amount, 1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    path = "{}/MI_GK_Fels_Brocken".format(FOLDER)
    mi = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        "MI_GK_Fels_Brocken", FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(mi, m)
    library.set_material_instance_scalar_parameter_value(mi, "Masken", 0.0)
    assets.save_loaded_asset(mi, False)
    return m


def assign(mesh_name, materials):
    mesh = unreal.load_asset("{}/{}".format(FOLDER, mesh_name))
    for i, material in enumerate(materials):
        mesh.set_material(i, material)
    assets.save_loaded_asset(mesh, False)
    used = [slot.get_editor_property("material_interface").get_name() for slot in mesh.get_editor_property("static_materials")]
    log("{}: {}".format(mesh_name, used))
    return mesh


def build_materials():
    tissue = material_tissue()
    glow = material_glow()
    vein = material_plain("M_GK_Ader", (0.16, 0.03, 0.02), 0.4, emissive=(0.25, 0.04, 0.01))
    soil = material_plain("M_GK_Erde", (0.07, 0.045, 0.03), 0.95, noise_amount=0.35)
    roots = material_roots()
    pool = material_pool()
    floor = material_floor()
    beam = material_beam()
    rock = material_rock()
    silk = material_plain("M_GK_Faden", (0.5, 0.42, 0.32), 0.45, emissive=(0.03, 0.015, 0.005), noise_amount=0.1, two_sided=True)
    meshes = {
        "SM_GK_Rippenwand": assign("SM_GK_Rippenwand", [tissue, glow["MI_GK_Membran"]]),
        "SM_GK_Eisack": assign("SM_GK_Eisack", [glow["MI_GK_Eisack"], vein]),
        "SM_GK_Gelegebeet": assign("SM_GK_Gelegebeet", [glow["MI_GK_Gelegehaut"], soil, glow["MI_GK_Eier"], silk]),
        "SM_GK_Gelege_Schlafend": assign("SM_GK_Gelege_Schlafend", [glow["MI_GK_Gelege_Schlafend"], glow["MI_GK_Eier_Schlafend"], silk]),
        "SM_GK_Lichtstrahl": assign("SM_GK_Lichtstrahl", [beam]),
        "SM_GK_Felswand_Hoch": assign("SM_GK_Felswand_Hoch", [rock]),
        "SM_GK_Felswand_Mittel": assign("SM_GK_Felswand_Mittel", [rock]),
        "SM_GK_Felswand_Niedrig": assign("SM_GK_Felswand_Niedrig", [rock]),
        "SM_GK_Wurzelvorhang": assign("SM_GK_Wurzelvorhang", [roots]),
        "SM_GK_Glutbecken": assign("SM_GK_Glutbecken", [pool, tissue]),
    }
    log("Materialien angelegt")
    for note in notes:
        log("Hinweis: " + note)
    del notes[:]
    return meshes, {"pool": pool, "floor": floor, "tissue": tissue}


# ---------------------------------------------------------------- Level

def turn(v, yaw):
    """Vector turned by a yaw in degrees around Z"""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return unreal.Vector(v.x * c - v.y * s, v.x * s + v.y * c, v.z)


def spawn(actor_class, location, label, folder, rotation=None):
    actor = actors.spawn_actor_from_class(actor_class, location, rotation or unreal.Rotator(0.0, 0.0, 0.0))
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    return actor


def mesh_actor(mesh, location, label, folder, yaw=0.0, scale=1.0, pitch=0.0, roll=0.0, material=None, collision=False):
    actor = spawn(unreal.StaticMeshActor, location, label, folder, unreal.Rotator(roll, pitch, yaw))
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    if material is not None:
        component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale) if not isinstance(scale, tuple) else unreal.Vector(*scale))
    if not collision:
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def point_light(location, label, color, intensity, radius, shadows=False):
    light = spawn(unreal.PointLight, location, label, "Licht")
    c = light.light_component
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_editor_property("intensity", intensity)
    c.set_light_color(unreal.LinearColor(color[0], color[1], color[2], 1.0))
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("cast_shadows", shadows)
    light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    return light


def shaft(x, y, label, intensity=3500.0, cone=11.0):
    light = spawn(unreal.SpotLight, unreal.Vector(x, y, 1500.0), label, "Licht/Lichtschaechte", unreal.Rotator(0.0, -90.0, 0.0))
    c = light.light_component
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_editor_property("intensity", intensity)
    c.set_light_color(unreal.LinearColor(1.0, 0.86, 0.62, 1.0))
    c.set_editor_property("inner_cone_angle", cone * 0.55)
    c.set_editor_property("outer_cone_angle", cone)
    c.set_editor_property("attenuation_radius", 2600.0)
    c.set_editor_property("volumetric_scattering_intensity", 6.0)
    light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    return light


def build_level(meshes, mats):
    if assets.does_asset_exist(LEVEL_PATH):
        assets.delete_asset(LEVEL_PATH)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    cylinder = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")

    # Floor, 30 x 30 m, top at 0
    mesh_actor(cube, unreal.Vector(0, 0, -50), "Boden", "Raum", scale=(30.0, 30.0, 1.0), material=mats["floor"], collision=True)

    # Ring of rocks: high on the far side (+X/+Y, the camera looks that way), low towards the camera
    rocks = [unreal.load_asset(p) for p in ROCKS]
    rocks = [r for r in rocks if r is not None]
    for i in range(18):
        angle = 2 * math.pi * i / 18
        far = math.cos(angle - math.pi / 4)                   # 1 opposite the camera, -1 next to it
        height = 250.0 + 650.0 * max(0.0, far) + rng.uniform(-60, 60)
        rock = rocks[i % len(rocks)]
        box = rock.get_bounding_box()
        size = box.max - box.min
        width = max(size.x, size.y, 1.0)
        s = min(height / max(size.z, 1.0), 650.0 / width)        # never wider than 6.5 m, so it does not fill the room
        radius = 1180.0 + width * s * 0.35 + rng.uniform(-40, 60)
        mesh_actor(rock, unreal.Vector(math.cos(angle) * radius, math.sin(angle) * radius, -40.0), "Fels {}".format(i + 1), "Raum/Fels",
                   yaw=rng.uniform(0, 360), scale=s, collision=True)

    # Rib walls on an arc around the far half, each leaning in towards the middle
    rib = meshes["SM_GK_Rippenwand"]
    sac = meshes["SM_GK_Eisack"]
    gaps = (-180.0, 0.0, 180.0)
    sac_count = 0
    for i, deg in enumerate((-21.0, 12.0, 45.0, 78.0, 111.0)):
        a = math.radians(deg)
        r = 930.0
        position = unreal.Vector(math.cos(a) * r, math.sin(a) * r, 0.0)
        yaw = deg + 180.0
        mesh_actor(rib, position, "Rippenwand {}".format(i + 1), "Brutkammer/Rippen", yaw=yaw)
        # egg sacs in some of the membranes, half sunk into them
        for gy in gaps:
            if rng.random() < 0.45:
                continue
            z = rng.uniform(70.0, 300.0)
            local = unreal.Vector(-10.0 + z * 0.02, gy + rng.uniform(-25, 25), z)
            world_offset = turn(local, yaw)
            loc = unreal.Vector(position.x + world_offset.x, position.y + world_offset.y, local.z - 60.0)
            scale = rng.uniform(0.8, 1.15)
            mesh_actor(sac, loc, "Eisack {}".format(sac_count + 1), "Brutkammer/Eisaecke", yaw=yaw + rng.uniform(-20, 20),
                       pitch=rng.uniform(-12, 12), scale=scale)
            inward = turn(unreal.Vector(70.0, 0.0, 0.0), yaw)
            point_light(unreal.Vector(loc.x + inward.x, loc.y + inward.y, loc.z + 70.0 * scale), "Eisack-Licht {}".format(sac_count + 1),
                        (1.0, 0.45, 0.12), 35.0, 420.0)
            sac_count += 1

    # A few sacs lying loose on the floor near the walls
    for i, (x, y) in enumerate(((560, -420), (380, 640), (-120, 760))):
        mesh_actor(sac, unreal.Vector(x, y, -15.0), "Eisack lose {}".format(i + 1), "Brutkammer/Eisaecke", yaw=rng.uniform(0, 360),
                   pitch=rng.uniform(60, 85), scale=0.75)

    # Four clutch beds with plants around the free middle (away from the pool), two empty clutches tipped over further out
    bed = meshes["SM_GK_Gelegebeet"]
    plants = [unreal.load_asset(p) for p in PLANTS]
    centre = unreal.Vector(-200.0, -230.0, 0.0)
    for i in range(4):
        a = math.radians(20 + 90 * i)
        loc = unreal.Vector(centre.x + math.cos(a) * 300.0, centre.y + math.sin(a) * 300.0, -6.0)
        mesh_actor(bed, loc, "Gelegebeet {}".format(i + 1), "Brutkammer/Beete", yaw=rng.uniform(0, 360), collision=True)
        plant = plants[i]
        if plant is not None:
            mesh_actor(plant, unreal.Vector(loc.x, loc.y, loc.z + 30.0), "Pflanze {}".format(i + 1), "Brutkammer/Beete",
                       yaw=rng.uniform(0, 360), scale=0.9)
        else:
            notes.append("Pflanze fehlt: " + PLANTS[i])
    for i, (x, y, tilt) in enumerate(((-620, 240, 70), (240, -680, 60))):
        mesh_actor(bed, unreal.Vector(x, y, 10.0), "Gelege leer {}".format(i + 1), "Brutkammer/Beete",
                   yaw=rng.uniform(0, 360), pitch=tilt, scale=0.8)

    # Root curtain from the crack, over the glowing pool
    pool_at = unreal.Vector(260.0, 220.0, 0.0)
    mesh_actor(meshes["SM_GK_Wurzelvorhang"], unreal.Vector(pool_at.x, pool_at.y, -5.0), "Wurzelvorhang", "Brutkammer/Riss", yaw=40.0)
    mesh_actor(meshes["SM_GK_Glutbecken"], unreal.Vector(pool_at.x, pool_at.y, 0.0), "Glutbecken", "Brutkammer/Riss", yaw=15.0)
    point_light(unreal.Vector(pool_at.x, pool_at.y, 90.0), "Becken-Licht", (1.0, 0.4, 0.1), 110.0, 900.0, shadows=True)

    # Light: shafts from the cracks above, a dim warm fill, volumetric fog
    shaft(pool_at.x, pool_at.y, "Hauptschacht", intensity=2000.0, cone=9.0)
    shaft(-360.0, 300.0, "Schacht West", intensity=1100.0, cone=7.0)
    shaft(380.0, -380.0, "Schacht Sued", intensity=1100.0, cone=7.0)
    shaft(-80.0, -120.0, "Schacht Mitte", intensity=800.0, cone=6.0)
    point_light(unreal.Vector(0.0, 0.0, 900.0), "Fuelllicht", (1.0, 0.7, 0.5), 45.0, 3200.0)

    fog = spawn(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), "Hoehlennebel", "Licht")
    f = fog.component
    f.set_editor_property("fog_density", 0.035)
    f.set_editor_property("fog_height_falloff", 0.25)
    f.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.06, 0.035, 0.025, 1.0))
    f.set_volumetric_fog(True)
    for key, value in (("volumetric_fog_scattering_distribution", 0.65), ("volumetric_fog_albedo", unreal.Color(255, 220, 190, 255)),
                       ("volumetric_fog_extinction_scale", 1.4)):
        try:
            f.set_editor_property(key, value)
        except Exception as error:
            notes.append("Nebel {}: {}".format(key, error))

    post = spawn(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), "Nachbearbeitung", "Licht")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    for key, value in (("auto_exposure_min_brightness", 0.0), ("auto_exposure_max_brightness", 0.0), ("auto_exposure_bias", 0.3),
                       ("bloom_intensity", 0.9), ("vignette_intensity", 0.45)):
        settings.set_editor_property("override_" + key, True)
        settings.set_editor_property(key, value)
    post.set_editor_property("settings", settings)

    # Indoor marker: no sky, no sun or moon, no local weather, no training dummies
    spawn(unreal.VaelIndoorLevel, unreal.Vector(0, 0, 300), "Innenraum", "Spiel")

    spawn(unreal.PlayerStart, unreal.Vector(-170.0, -190.0, 100.0), "Spielerstart", "Spiel", unreal.Rotator(0.0, 0.0, 45.0))

    if unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH):
        log("Testkarte gespeichert: " + LEVEL_PATH)
    else:
        unreal.log_error("[GlutkammerProbe] Testkarte konnte nicht gespeichert werden")


def build():
    import_pieces()
    meshes, mats = build_materials()
    build_level(meshes, mats)
    for note in notes:
        log("Hinweis: " + note)
    log("fertig")


build()
