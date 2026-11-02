# Materials of the spells that were still flat placeholders: warning circles, living pools on the ground, the black sun,
# the flesh of the Narbenwall, the clay of the Lehmgolem, the fever mist. Run headless with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=C:/Projekte/Vael/Scripts/CreateSpellSurfaceKit.py
# Rerun-safe: a material graph is only rebuilt when KIT_VERSION grew, instances are written every run.

import unreal

FOLDER = "/Game/Vael/Effects/Flaechen"
NEBEL = "/Game/Vael/Effects/Nebel"
KIT_VERSION = 1.33

assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
notes = []

FLOAT1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
FLOAT3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

NOISE = """struct VaelNoise {
float h(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float n(float2 p) { float2 i = floor(p); float2 f = frac(p); f = f * f * (3.0 - 2.0 * f);
return lerp(lerp(h(i), h(i + float2(1, 0)), f.x), lerp(h(i + float2(0, 1)), h(i + float2(1, 1)), f.x), f.y); }
float fbm(float2 p) { return n(p) * 0.5 + n(p * 2.1 + 3.1) * 0.3 + n(p * 4.3 + 7.7) * 0.2; }
};
VaelNoise N;
"""

# ---------------------------------------------------------------- Warning circle
# P is the place on the disc in units of its radius; Fuellung says how far the strike has come
WARN_SHAPE = """float R = length(P);
float Angle = atan2(P.y, P.x);
float Rim = saturate(1.0 - abs(R - 0.97) / 0.014);
float Teeth = step(0.5, frac(Angle * 3.8197 + Zeit * 0.35)) * saturate(1.0 - abs(R - 0.86) / 0.016);
float Front = saturate(1.0 - abs(R - Fuellung) / 0.05) * step(R, 0.97);
float Inside = step(R, Fuellung) * 0.16;
"""
WARN_EMISSIVE = WARN_SHAPE + "return Farbe * (Rim * 1.1 + Teeth * 0.7 + Front * 0.9 + Inside * 0.6) * step(R, 1.0);"
WARN_OPACITY = WARN_SHAPE + "return saturate(Rim * 0.9 + Teeth * 0.6 + Front * 0.7 + Inside) * step(R, 1.0);"

# ---------------------------------------------------------------- Pool on the ground
# UV runs from -1 to 1 over the pool. Bubbles rise, swell and burst, thick liquids barely move, a Strudel winds everything into the middle.
POOL_HEIGHT = NOISE + """float2 Spin(float2 p, float a) { return float2(p.x * cos(a) - p.y * sin(a), p.x * sin(a) + p.y * cos(a)); }
"""
POOL_SHAPE = NOISE + """float Edge = length(UV) + (N.fbm(UV * 2.6 + 11.0) - 0.5) * 0.42;
float Clip = Wachstum - Edge;
float Turn = Strudel * (Zeit * 1.2 + 2.6 / (length(UV) + 0.25));
float2 Q = float2(UV.x * cos(Turn) - UV.y * sin(Turn), UV.x * sin(Turn) + UV.y * cos(Turn)) * Kachel;
float2 Drift = float2(Zeit * Fluss, -Zeit * Fluss * 0.7);
float Field = N.fbm(Q + Drift + N.fbm(Q * 0.7 - Drift) * 1.4);
float2 Cell = floor(Q * 1.7);
float2 Part = frac(Q * 1.7) - 0.5;
float Born = N.h(Cell);
float Age = frac(Zeit * (0.25 + Born * 0.35) + Born * 7.0);
float Size = step(1.0 - Blasen, N.h(Cell + 3.0)) * (0.1 + Age * 0.3);
float Dist = length(Part + (float2(N.h(Cell + 5.0), N.h(Cell + 9.0)) - 0.5) * 0.4);
float Bubble = saturate(1.0 - Dist / max(Size, 0.001)) * step(0.001, Size);
float Height = Field + Bubble * 0.45;
float Vein = saturate(1.0 - abs(N.fbm(Q * 0.8 + Drift * 0.3) - 0.5) / 0.011);
float Beat = 0.6 + 0.4 * sin(Zeit * 2.3 + Field * 6.0);
float RimShare = saturate(1.0 - Clip / 0.11);
"""
POOL_COLOR = POOL_SHAPE + "return lerp(lerp(FarbeTief, FarbeHell, saturate(Height * 1.5 - 0.3)), Randfarbe, RimShare);"
POOL_EMISSIVE = POOL_SHAPE + "return Aderfarbe * Aderleuchten * (Vein * Beat * (1.0 - RimShare) + Bubble * Bubble * 0.8) + FarbeHell * Eigenlicht * Height;"
POOL_NORMAL = POOL_SHAPE + """float D = 0.05;
float Hx = N.fbm(Q + float2(D, 0) + Drift + N.fbm(Q * 0.7 - Drift) * 1.4);
float Hy = N.fbm(Q + float2(0, D) + Drift + N.fbm(Q * 0.7 - Drift) * 1.4);
float3 Out = float3((Field - Hx) * Relief, (Field - Hy) * Relief, 1.0);
return normalize(Out);"""
POOL_MASK = POOL_SHAPE + "return Clip;"
POOL_ROUGH = POOL_SHAPE + "return lerp(Rauheit, 0.75, RimShare);"

# ---------------------------------------------------------------- The black sun
SUN_SHAPE = NOISE + """float2 C = UV * 2.0 - 1.0;
float R = length(C);
float Angle = atan2(C.y, C.x);
float2 Around = float2(cos(Angle), sin(Angle));
float Flame = N.fbm(Around * 2.2 + float2(Zeit * 0.5, R * 3.0 - Zeit * 1.1));
float Disc = 1.0 - smoothstep(Scheibe - 0.015, Scheibe + 0.01, R);
float Reach = Scheibe + 0.08 + Flame * 0.5;
float Corona = pow(saturate(1.0 - (R - Scheibe) / max(Reach - Scheibe, 0.01)), 2.2) * step(Scheibe - 0.02, R);
float Rim = saturate(1.0 - abs(R - Scheibe) / 0.012);
"""
SUN_EMISSIVE = SUN_SHAPE + "return (Farbe * Corona + float3(1.0, 0.7, 0.4) * Rim * 0.9) * Leuchten * (1.0 - Disc);"
SUN_OPACITY = SUN_SHAPE + "return saturate(Disc + Corona * 0.25);"

# ---------------------------------------------------------------- Flesh of the Narbenwall
# P is the place on the mesh in cm, in its own frame, so the pattern doesn't swim when the wall pulses
FLESH_SHAPE = NOISE + """float2 A = P.xz * 0.03 + P.yy * 0.018;
float Meat = N.fbm(A);
float Scar = saturate(1.0 - abs(N.fbm(A * 1.3 + 4.0) - 0.5) / 0.03);
float Vein = saturate(1.0 - abs(N.fbm(A * 0.8 + 9.0) - 0.48) / 0.016);
"""
FLESH_COLOR = FLESH_SHAPE + "return lerp(lerp(float3(0.1, 0.012, 0.02), float3(0.34, 0.05, 0.07), Meat), float3(0.62, 0.48, 0.42), Scar * 0.75);"
FLESH_EMISSIVE = FLESH_SHAPE + "return Aderfarbe * Vein * Puls;"
FLESH_ROUGH = FLESH_SHAPE + "return lerp(0.22, 0.6, Scar);"


# ---------------------------------------------------------------- Clay of the Lehmgolem
# P is the place in cm around the middle of each lump. The mud never stands still: wet streaks and bulges run down all the time,
# the surface churns slowly, wet dark mud over drier crusted clay
CLAY_SHAPE = NOISE + """float3 Q = P * 0.045;
float Flow = Zeit * Fluss;
float2 Down = float2(Q.x + Q.y * 0.7, Q.z + Flow);
float Streak = N.fbm(float2(Down.x * 2.2, Down.y * 0.55));
float Churn = N.fbm(Q.xy * 1.3 + Q.zz * 0.9 + Flow * 0.25);
float Wet = saturate((Streak - 0.36) * 3.0);
"""
CLAY_COLOR = CLAY_SHAPE + "return lerp(float3(0.15, 0.095, 0.05), float3(0.035, 0.025, 0.015), Wet) * (0.75 + 0.5 * Churn);"
CLAY_ROUGH = CLAY_SHAPE + "return lerp(0.8, 0.12, Wet);"
CLAY_OFFSET = CLAY_SHAPE + "return Normale * ((Streak - 0.5) * Beule + (Churn - 0.5) * Beule * 0.6);"
def log(message):
    unreal.log_warning("[CreateSpellSurfaceKit] " + message)


class Graph:
    def __init__(self, name):
        self.path = "{}/{}".format(FOLDER, name)
        self.name = name
        self.row = 0
        self.material = None

    def begin(self):
        """False when the graph is up to date"""
        if assets.does_asset_exist(self.path):
            material = unreal.load_asset(self.path)
            if "KitVersion" in [str(parameter) for parameter in library.get_scalar_parameter_names(material)]:
                if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
                    self.material = material
                    return False
            library.delete_all_material_expressions(material)
            for expression in list(unreal.ObjectIterator(unreal.MaterialExpression)):
                if expression.get_outer() == material:
                    library.delete_material_expression(material, expression)
            self.material = material
        else:
            self.material = tools.create_asset(self.name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        return True

    def node(self, expression_class, **properties):
        self.row += 1
        expression = library.create_material_expression(self.material, expression_class, -700, 110 * self.row)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def scalar(self, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)

    def vector(self, name, rgb):
        parameter = self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        mask = self.node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
        self.link(parameter, mask)
        return mask

    def time(self):
        return self.node(unreal.MaterialExpressionTime)

    def uv(self):
        return self.node(unreal.MaterialExpressionTextureCoordinate)

    def link(self, source, target, target_input=""):
        if not library.connect_material_expressions(source, "", target, target_input):
            notes.append("{}: link failed {} -> {}".format(self.name, source.get_name(), target_input))

    def custom(self, code, output_type, sources):
        expression = self.node(unreal.MaterialExpressionCustom, code=code, output_type=output_type)
        inputs = []
        for name in sources:
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", name)
            inputs.append(entry)
        expression.set_editor_property("inputs", inputs)
        for name, source in sources.items():
            self.link(source, expression, name)
        return expression

    def out(self, expression, prop):
        library.connect_material_property(expression, "", prop)

    def finish(self):
        self.scalar("KitVersion", KIT_VERSION)
        library.recompile_material(self.material)
        assets.save_loaded_asset(self.material, False)
        log("built " + self.name)


def build_warning():
    g = Graph("M_Vael_Warnkreis")
    if not g.begin():
        return
    m = g.material
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    # The place on the disc from the world, because the engine cylinder has no useful UVs on its lid
    world = g.node(unreal.MaterialExpressionWorldPosition)
    middle = g.node(unreal.MaterialExpressionObjectPositionWS)
    offset = g.node(unreal.MaterialExpressionSubtract)
    g.link(world, offset, "A")
    g.link(middle, offset, "B")
    flat = g.node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
    g.link(offset, flat)
    place = g.node(unreal.MaterialExpressionDivide)
    g.link(flat, place, "A")
    g.link(g.scalar("Radius", 100.0), place, "B")

    time = g.time()
    fill = g.scalar("Fuellung", 0.0)
    colour = g.vector("Farbe", (1.0, 0.4, 0.15))
    g.out(g.custom(WARN_EMISSIVE, FLOAT3, {"P": place, "Zeit": time, "Fuellung": fill, "Farbe": colour}), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.custom(WARN_OPACITY, FLOAT1, {"P": place, "Zeit": time, "Fuellung": fill}), unreal.MaterialProperty.MP_OPACITY)
    g.finish()


def build_pool():
    g = Graph("M_Vael_Pfuetze")
    if not g.begin():
        return
    m = g.material
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("opacity_mask_clip_value", 0.0)
    m.set_editor_property("tangent_space_normal", False)

    uv = g.uv()
    time = g.time()
    shared = {
        "UV": uv, "Zeit": time, "Wachstum": g.scalar("Wachstum", 1.0), "Kachel": g.scalar("Kachel", 3.0), "Fluss": g.scalar("Fluss", 0.05),
        "Blasen": g.scalar("Blasen", 0.3), "Strudel": g.scalar("Strudel", 0.0),
    }
    colours = dict(shared)
    colours.update({"FarbeTief": g.vector("FarbeTief", (0.03, 0.02, 0.01)), "FarbeHell": g.vector("FarbeHell", (0.1, 0.07, 0.04)), "Randfarbe": g.vector("Randfarbe", (0.08, 0.06, 0.04))})
    g.out(g.custom(POOL_COLOR, FLOAT3, colours), unreal.MaterialProperty.MP_BASE_COLOR)

    glow = dict(shared)
    glow.update({"Aderfarbe": g.vector("Aderfarbe", (0.5, 0.1, 1.0)), "Aderleuchten": g.scalar("Aderleuchten", 0.0), "FarbeHell": colours["FarbeHell"], "Eigenlicht": g.scalar("Eigenlicht", 0.0)})
    g.out(g.custom(POOL_EMISSIVE, FLOAT3, glow), unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    normal = dict(shared)
    normal["Relief"] = g.scalar("Relief", 1.6)
    g.out(g.custom(POOL_NORMAL, FLOAT3, normal), unreal.MaterialProperty.MP_NORMAL)

    rough = dict(shared)
    rough["Rauheit"] = g.scalar("Rauheit", 0.3)
    g.out(g.custom(POOL_ROUGH, FLOAT1, rough), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.custom(POOL_MASK, FLOAT1, shared), unreal.MaterialProperty.MP_OPACITY_MASK)
    g.finish()


def build_sun():
    g = Graph("M_Vael_Schwarzsonne")
    if not g.begin():
        return
    m = g.material
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)

    uv = g.uv()
    time = g.time()
    disc = g.scalar("Scheibe", 0.38)
    g.out(g.custom(SUN_EMISSIVE, FLOAT3, {"UV": uv, "Zeit": time, "Scheibe": disc, "Farbe": g.vector("Farbe", (1.0, 0.3, 0.05)), "Leuchten": g.scalar("Leuchten", 5.0)}), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.custom(SUN_OPACITY, FLOAT1, {"UV": uv, "Zeit": time, "Scheibe": disc}), unreal.MaterialProperty.MP_OPACITY)
    g.finish()


def build_flesh():
    g = Graph("M_Vael_Fleisch")
    if not g.begin():
        return
    world = g.node(unreal.MaterialExpressionWorldPosition)
    middle = g.node(unreal.MaterialExpressionObjectPositionWS)
    offset = g.node(unreal.MaterialExpressionSubtract)
    g.link(world, offset, "A")
    g.link(middle, offset, "B")
    g.out(g.custom(FLESH_COLOR, FLOAT3, {"P": offset}), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.custom(FLESH_EMISSIVE, FLOAT3, {"P": offset, "Aderfarbe": g.vector("Aderfarbe", (0.55, 0.1, 1.0)), "Puls": g.scalar("Puls", 1.0)}), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.custom(FLESH_ROUGH, FLOAT1, {"P": offset}), unreal.MaterialProperty.MP_ROUGHNESS)
    g.finish()


def build_clay():
    g = Graph("M_Vael_Lehm")
    if not g.begin():
        return
    world = g.node(unreal.MaterialExpressionWorldPosition)
    middle = g.node(unreal.MaterialExpressionObjectPositionWS)
    offset = g.node(unreal.MaterialExpressionSubtract)
    g.link(world, offset, "A")
    g.link(middle, offset, "B")
    zeit, fluss = g.time(), g.scalar("Fluss", 0.9)
    inputs = {"P": offset, "Zeit": zeit, "Fluss": fluss}
    g.out(g.custom(CLAY_COLOR, FLOAT3, inputs), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.custom(CLAY_ROUGH, FLOAT1, inputs), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.custom(CLAY_OFFSET, FLOAT3, dict(inputs, Normale=g.node(unreal.MaterialExpressionVertexNormalWS), Beule=g.scalar("Beule", 6.0))),
          unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    g.finish()


def import_glyphs():
    """Atlas of Tools/Zauber/vael_zeichen.py: 4 x 4 signs, R stroke, G glow"""
    import os
    path = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Zauber", "T_Vael_Zeichen.png")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", "T_Vael_Zeichen")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + "/T_Vael_Zeichen")
    if texture is None:
        notes.append("import failed: T_Vael_Zeichen")
        return None
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    assets.save_loaded_asset(texture, False)
    return texture


def build_glyphs(atlas):
    """The signs that rise to the cloud of a weather call: one sign of the atlas per instance (custom data 0), glowing in Farbe"""
    g = Graph("M_Vael_Zeichen")
    if atlas is None or not g.begin():
        return
    g.material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    g.material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    g.material.set_editor_property("two_sided", True)
    g.material.set_editor_property("used_with_instanced_static_meshes", True)
    index = g.node(unreal.MaterialExpressionPerInstanceCustomData, data_index=0)
    cell_uv = g.custom("float C = floor(Index + 0.5); return (float2(fmod(C, 4.0), floor(C / 4.0)) + saturate(UV)) / 4.0;",
                       unreal.CustomMaterialOutputType.CMOT_FLOAT2, {"Index": index, "UV": g.uv()})
    sample = g.node(unreal.MaterialExpressionTextureSample, texture=atlas, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(cell_uv, sample, "UVs")
    glow = g.custom("return Farbe * Leuchten * (Atlas.r + Atlas.g * 0.45);", FLOAT3,
                    {"Atlas": sample, "Farbe": g.vector("Farbe", (0.4, 0.7, 1.0)), "Leuchten": g.scalar("Leuchten", 5.0)})
    g.out(glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()


def instance(name, parent, values):
    path = "{}/{}".format(FOLDER, name)
    material = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(material, parent)
    for key, value in values.items():
        if isinstance(value, tuple):
            library.set_material_instance_vector_parameter_value(material, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
        else:
            library.set_material_instance_scalar_parameter_value(material, key, value)
    library.update_material_instance(material)
    assets.save_loaded_asset(material, False)


def build_instances():
    pool = unreal.load_asset(FOLDER + "/M_Vael_Pfuetze")
    instance("MI_Pfuetze_Schlamm", pool, {
        "FarbeTief": (0.018, 0.011, 0.006), "FarbeHell": (0.06, 0.038, 0.02), "Randfarbe": (0.05, 0.035, 0.022),
        "Kachel": 2.6, "Fluss": 0.03, "Blasen": 0.22, "Rauheit": 0.28, "Relief": 2.6})
    instance("MI_Pfuetze_Quelle", pool, {
        "FarbeTief": (0.0, 0.07, 0.1), "FarbeHell": (0.02, 0.3, 0.32), "Randfarbe": (0.62, 0.5, 0.3),
        "Kachel": 3.4, "Fluss": 0.16, "Blasen": 0.5, "Rauheit": 0.08, "Relief": 1.0, "Eigenlicht": 0.25,
        "Aderfarbe": (1.0, 0.55, 0.2), "Aderleuchten": 0.2})
    instance("MI_Pfuetze_Schwarzwasser", pool, {
        "FarbeTief": (0.002, 0.0, 0.006), "FarbeHell": (0.025, 0.006, 0.04), "Randfarbe": (0.03, 0.005, 0.035),
        "Kachel": 3.0, "Fluss": 0.07, "Blasen": 0.3, "Rauheit": 0.06, "Relief": 1.2,
        "Aderfarbe": (0.5, 0.08, 1.0), "Aderleuchten": 3.0})
    instance("MI_Pfuetze_Nidglut", pool, {
        "FarbeTief": (0.004, 0.0, 0.008), "FarbeHell": (0.04, 0.005, 0.06), "Randfarbe": (0.01, 0.005, 0.012),
        "Kachel": 4.2, "Fluss": 0.12, "Blasen": 0.4, "Rauheit": 0.7, "Relief": 3.0,
        "Aderfarbe": (0.65, 0.12, 1.0), "Aderleuchten": 7.0})
    instance("MI_Pfuetze_Moor", pool, {
        "FarbeTief": (0.01, 0.018, 0.012), "FarbeHell": (0.06, 0.075, 0.04), "Randfarbe": (0.05, 0.05, 0.03),
        "Kachel": 2.2, "Fluss": 0.05, "Blasen": 0.35, "Rauheit": 0.16, "Relief": 3.0, "Strudel": 1.0})

    instance("MI_Narbenwall_Knochen", unreal.load_asset("/Game/Vael/Characters/Grimoire/M_Grimoire"), {"Tonung": (0.8, 0.72, 0.58), "Metall": 0.0, "Rauheit": 0.5})

    # Cards of the fever mist: a sick, warm violet (Boris 2026-10-18: violet belongs to the Nid)
    path = NEBEL + "/MI_Vael_Wolke_Fieber"
    card = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset("MI_Vael_Wolke_Fieber", NEBEL, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(card, unreal.load_asset(NEBEL + "/M_Vael_Wolkenkarte"))
    library.set_material_instance_vector_parameter_value(card, "Farbe", unreal.LinearColor(0.42, 0.14, 0.42, 1.0))
    library.set_material_instance_scalar_parameter_value(card, "Eigenlicht", 0.45)
    library.set_material_instance_scalar_parameter_value(card, "Schatten", 0.55)
    library.update_material_instance(card)
    assets.save_loaded_asset(card, False)


def import_meshes():
    """Meshes of Tools/Zauber/vael_zauber_formen.py; slots get their materials by name"""
    import os
    sources = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Zauber")
    materials = {"Fleisch": unreal.load_asset(FOLDER + "/M_Vael_Fleisch"), "Knochen": unreal.load_asset(FOLDER + "/MI_Narbenwall_Knochen")}
    for name in ("SM_Narbenwall", "SM_Lehmklumpen"):
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
        task.set_editor_property("filename", os.path.join(sources, name + ".fbx"))
        task.set_editor_property("destination_path", FOLDER)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
        mesh = unreal.load_asset("{}/{}".format(FOLDER, name))
        if mesh is None:
            notes.append("import failed: " + name)
            continue
        nanite = mesh.get_editor_property("nanite_settings")
        if nanite.get_editor_property("enabled"):
            nanite.set_editor_property("enabled", False)
            mesh.set_editor_property("nanite_settings", nanite)
        unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
        slots = mesh.get_editor_property("static_materials")
        for index, entry in enumerate(slots):
            slot_name = str(entry.get_editor_property("material_slot_name"))
            for key, value in materials.items():
                if key in slot_name:
                    mesh.set_material(index, value)
        log("{} slots {}".format(name, [str(entry.get_editor_property("material_slot_name")) for entry in slots]))
        assets.save_loaded_asset(mesh, False)


def main():
    assets.make_directory(FOLDER)
    build_warning()
    build_pool()
    build_sun()
    build_flesh()
    build_clay()
    build_glyphs(import_glyphs())
    build_instances()
    import_meshes()
    for note in notes:
        log("NOTE " + note)
    log("done, {} notes".format(len(notes)))


main()
