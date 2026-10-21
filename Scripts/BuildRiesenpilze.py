# Puts the Tripo giant mushroom (SM_Riesenpilz, "Schleierschirm") at the grey box mushrooms of the Kammweg in
# L_Wurzelforst_Blockout: the 28 Riesenpilze and the 4 Pilzinseln, each with a few small ones of its kind around it.
#
# - Material M_WF_Riesenpilz: the Tripo picture split by colour into cap (coral), glow (turquoise blisters, gills, tendril
#   tips) and stem (ivory). Each part can be recoloured; the glow parts shine. The hanging tendrils sway slowly
#   (World Position Offset, only away from the stem and more towards their tips).
# - Colour variants as instances MI_WF_Riesenpilz_<Name>; violet stays free for Nid.
# - Every mushroom different: size, wider or narrower cap, slight lean, its turn, sunk a little into the ground.
# - Collision only on the stem: capsules along the stem profile measured by
#   Tools/Kreaturenfabrik/blender/vael_pilz_vermessen.py; players walk under the cap and through the tendrils.
# - LODs and 2K textures, because about a hundred of them stand along the Kammweg.
# - The grey box stems and caps are switched off (not deleted); their lights stay.
# Everything it adds is tagged WF_Riesenpilz (Outliner folder Wurzelforst/4_Kammweg/Riesenpilze_Modell); rerunning
# replaces only that. Needs Vael_Quellen/Riesenpilz/03_analyse/pilz.json and the imported SM_Riesenpilz.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildRiesenpilze.py" -EnablePlugins=PythonScriptPlugin

import json
import math
import os
import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KIT = os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout")
MUSHROOM_DATA = os.path.join(os.path.dirname(ROOT), "Vael_Quellen", "Riesenpilz", "03_analyse", "pilz.json")
MATERIAL_FOLDER = "/Game/Vael/Wurzelforst/Blockout/Materialien"
MUSHROOM = "/Game/Vael/Requisiten/Wurzelforst/Riesenpilz"
TAG = "WF_Riesenpilz"
HIDE_TAG = "WF_RiesenpilzVerdeckt"
FOLDER = "Wurzelforst/4_Kammweg/Riesenpilze_Modell"
SEED = 32
MAP_ORIGIN = -201600.0  # WF_Platzhalter.json holds map cm; the world is shifted by the landscape corner (as in wf_moor.py)

# Height of the model compared with the grey box stem, and the limits (Boris: giants a little under 15-20 m)
HEIGHT_SHARE = 0.8
HEIGHT_MIN_CM, HEIGHT_MAX_CM = 650.0, 1400.0
ISLAND_HEIGHT_CM = 1100.0
SMALL_PER_GIANT = (2, 4)
SMALL_HEIGHT_CM = (120.0, 380.0)

# name: (cap, glow, stem, recolour 0..1); "Koralle" is the model as drawn
VARIANTS = {
    "Koralle": ((0.95, 0.35, 0.28), (0.10, 0.85, 0.75), (0.90, 0.88, 0.78), 0.0),
    "Glut": ((1.00, 0.42, 0.06), (0.75, 1.00, 0.15), (0.92, 0.84, 0.66), 1.0),
    "Blutkappe": ((0.75, 0.05, 0.06), (0.30, 1.00, 0.70), (0.88, 0.85, 0.80), 1.0),
    "Giftgelb": ((0.95, 0.80, 0.08), (0.05, 0.75, 1.00), (0.85, 0.90, 0.75), 1.0),
    "Tiefsee": ((0.08, 0.30, 0.85), (1.00, 0.45, 0.65), (0.82, 0.86, 0.88), 1.0),
}
# The grey box colours as a hint, the rest by chance
BOX_HINT = {"Pilz_Tuerkis": "Koralle", "Pilz_Orange": "Glut", "Pilz_Rot": "Blutkappe"}

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
notes = []
WORLD = [None]


def log(message):
    unreal.log_warning("[BuildRiesenpilze] " + message)


# ---------------------------------------------------------------- Material

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
        return self.one(unreal.MaterialExpressionSaturate, column, source)

    def grey(self, source, column):
        expression = self.node(unreal.MaterialExpressionDesaturation, column)
        self.link(source, expression)
        self.link(self.const(1.0, column + 1), expression, "Fraction")
        return expression

    def sample(self, texture, column, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        return self.node(unreal.MaterialExpressionTextureSample, column, texture=texture, sampler_type=sampler)


def texture(path):
    loaded = unreal.load_asset(path)
    if loaded is None:
        notes.append("missing texture " + path)
    return loaded


def build_material(shape):
    path = "{}/M_WF_Riesenpilz".format(MATERIAL_FOLDER)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_WF_Riesenpilz", MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)

    # Split the picture by colour: turquoise = glow, red over green = cap, the rest = stem
    picture = g.mask(g.sample(texture(MUSHROOM + "/T_Riesenpilz_basecolor"), 13), 12)
    red = g.mask(picture, 11, g=False, b=False)
    green = g.mask(picture, 11, r=False, b=False)
    blue = g.mask(picture, 11, r=False, g=False)
    cyan = g.sub(10, g.mul(10, g.add(11, green, blue), g.const(0.5, 11)), red)
    glow_share = g.saturate(g.mul(8, g.sub(9, cyan, g.scalar("LeuchtSchwelle", 0.12, 10)), g.const(5.0, 9)), 7)
    cap_share = g.saturate(g.mul(8, g.sub(9, g.sub(10, red, green), g.scalar("HutSchwelle", 0.12, 10)), g.const(4.0, 9)), 7)

    # Recolour: brightness of the picture times the new colour, so shading and detail stay
    light = g.mul(7, g.grey(picture, 8), g.const(1.8, 8))
    recolour = g.scalar("Umfaerben", 0.0, 7)
    stem = g.lerp(picture, g.mul(6, light, g.color("StielFarbe", VARIANTS["Koralle"][2], 7)), recolour, 5)
    cap_colour = g.color("HutFarbe", VARIANTS["Koralle"][0], 7)
    glow_colour = g.color("LeuchtFarbe", VARIANTS["Koralle"][1], 7)
    with_cap = g.lerp(stem, g.lerp(picture, g.mul(6, light, cap_colour), recolour, 5), cap_share, 4)
    albedo = g.lerp(with_cap, g.lerp(picture, g.mul(6, light, glow_colour), recolour, 5), glow_share, 3)

    # Glow on the turquoise parts; a soft pulse so the forest breathes
    time = g.node(unreal.MaterialExpressionTime, 8)
    pulse = g.add(5, g.mul(6, g.one(unreal.MaterialExpressionSine, 6, g.mul(7, time, g.const(0.7, 8))), g.const(0.2, 7)), g.const(1.0, 6))
    emissive = g.mul(2, g.mul(3, g.mul(4, glow_share, glow_colour), g.scalar("LeuchtStaerke", 4.0, 4)), pulse)

    # Tendril sway: local position (works for instances), distance from the stem axis, how far below the cap
    local = g.node(unreal.MaterialExpressionLocalPosition, 14)
    local_z = g.mask(local, 13, r=False, g=False)
    under_cap = g.scalar("HutUnterseite", shape["hut_unterseite_cm"], 14)
    height_share = g.saturate(g.div(12, local_z, under_cap), 11)
    bottom = g.node(unreal.MaterialExpressionConstant2Vector, 13, r=shape["stiel_unten_cm"][0] / 1.0, g=shape["stiel_unten_cm"][1] / 1.0)
    top = g.node(unreal.MaterialExpressionConstant2Vector, 13, r=shape["stiel_oben_cm"][0] / 1.0, g=shape["stiel_oben_cm"][1] / 1.0)
    axis = g.lerp(bottom, top, height_share, 10)
    flat = g.mask(local, 12, b=False)
    away = g.one(unreal.MaterialExpressionLength, 9, g.sub(10, flat, axis))
    stem_radius = g.scalar("StielRadius", shape["stiel_radius_cm"], 10)
    outside = g.saturate(g.div(7, g.sub(8, away, g.mul(9, stem_radius, g.const(1.8, 10))), stem_radius), 6)
    hanging = g.saturate(g.div(7, g.sub(8, under_cap, local_z), g.mul(8, under_cap, g.const(0.45, 9))), 6)
    bend = g.mul(4, g.mul(5, outside, g.mul(5, hanging, hanging)), g.scalar("FadenSchwingen", 35.0, 5))
    world = g.node(unreal.MaterialExpressionWorldPosition, 9)
    place = g.mul(7, g.add(8, g.mask(world, 9, g=False, b=False), g.mask(world, 9, r=False, b=False)), g.const(0.003, 8))
    phase = g.add(6, g.mul(7, time, g.scalar("FadenTempo", 0.8, 8)), place)
    wave_x = g.one(unreal.MaterialExpressionSine, 5, phase)
    wave_y = g.one(unreal.MaterialExpressionCosine, 5, g.mul(6, phase, g.const(0.77, 7)))
    return material, (albedo, emissive, wave_x, wave_y, bend, g)


def finish_material(material, parts):
    albedo, emissive, wave_x, wave_y, bend, g = parts
    xy = g.node(unreal.MaterialExpressionAppendVector, 3)
    g.link(wave_x, xy, "A")
    g.link(wave_y, xy, "B")
    xyz = g.node(unreal.MaterialExpressionAppendVector, 2)
    g.link(xy, xyz, "A")
    g.link(g.const(0.0, 3), xyz, "B")
    offset = g.mul(1, xyz, bend)

    normal_map = g.sample(texture(MUSHROOM + "/T_Riesenpilz_normal"), 3, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    library.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal_map, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(g.scalar("Rauheit", 0.55, 2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)


def build_variants(material):
    instances = {}
    for name, (cap, glow, stem, recolour) in VARIANTS.items():
        path = "{}/MI_WF_Riesenpilz_{}".format(MATERIAL_FOLDER, name)
        if assets.does_asset_exist(path):
            instance = unreal.load_asset(path)
        else:
            instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_WF_Riesenpilz_" + name, MATERIAL_FOLDER, unreal.MaterialInstanceConstant,
                                                                               unreal.MaterialInstanceConstantFactoryNew())
        library.set_material_instance_parent(instance, material)
        library.set_material_instance_scalar_parameter_value(instance, "Umfaerben", recolour)
        for parameter, rgb in (("HutFarbe", cap), ("LeuchtFarbe", glow), ("StielFarbe", stem)):
            library.set_material_instance_vector_parameter_value(instance, parameter, unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        library.update_material_instance(instance)
        assets.save_loaded_asset(instance, False)
        instances[name] = instance
    return instances


# ---------------------------------------------------------------- Placement

def trace(start, end):
    hit = unreal.SystemLibrary.line_trace_single(WORLD[0], start, end,
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    values = hit.to_tuple()
    if not values[0]:
        return None
    return values[4], values[7]


def stem_only_collision(mesh, shape, segments=5):
    """Capsules along the measured stem replace the import's single hull around everything: players walk under the
    cap and through the tendrils; only the stem (and its foot sheath) blocks"""
    body = mesh.get_editor_property("body_setup")
    if body is None:
        notes.append("SM_Riesenpilz has no body setup")
        return
    profile = shape["stiel_profil_cm"]
    size = max(1, int(math.ceil(len(profile) / float(segments))))
    capsules = []
    for start in range(0, len(profile), size):
        part = profile[start:start + size]
        z0, z1 = part[0][0], part[-1][1]
        a = unreal.Vector(part[0][2], part[0][3], z0)
        b = unreal.Vector(part[-1][2], part[-1][3], z1)
        radius = max(p[4] for p in part) * 0.85
        direction = (b - a).normal()
        capsule = unreal.KSphylElem()
        capsule.set_editor_property("center", (a + b) * 0.5)
        capsule.set_editor_property("rotation", unreal.MathLibrary.make_rot_from_z(direction))
        capsule.set_editor_property("radius", radius)
        capsule.set_editor_property("length", max((b - a).length() - radius, 1.0))
        capsules.append(capsule)
    geometry = body.get_editor_property("agg_geom")
    geometry.set_editor_property("convex_elems", [])
    geometry.set_editor_property("box_elems", [])
    geometry.set_editor_property("sphere_elems", [])
    geometry.set_editor_property("sphyl_elems", capsules)
    body.set_editor_property("agg_geom", geometry)
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    mesh.modify()
    assets.save_loaded_asset(mesh, False)
    check = len(body.get_editor_property("agg_geom").get_editor_property("sphyl_elems"))
    if check != len(capsules):
        notes.append("collision capsules not kept ({} of {})".format(check, len(capsules)))


def lighten(mesh):
    """About a hundred of them stand at the Kammweg: LODs for the far and small ones, textures at 2K"""
    # The editor subsystem is missing in the commandlet; the older library does the same there
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    if subsystem.get_lod_count(mesh) < 3:
        options = unreal.StaticMeshReductionOptions()
        options.set_editor_property("auto_compute_lod_screen_size", False)
        settings = []
        for percent, screen in ((1.0, 1.0), (0.35, 0.35), (0.12, 0.12)):
            setting = unreal.StaticMeshReductionSettings()
            setting.set_editor_property("percent_triangles", percent)
            setting.set_editor_property("screen_size", screen)
            settings.append(setting)
        options.set_editor_property("reduction_settings", settings)
        subsystem.set_lods(mesh, options)
        assets.save_loaded_asset(mesh, False)
    for kind in ("basecolor", "normal", "roughness", "metallic"):
        picture = unreal.load_asset("{}/T_Riesenpilz_{}".format(MUSHROOM, kind))
        if picture is not None and picture.get_editor_property("max_texture_size") != 2048:
            picture.set_editor_property("max_texture_size", 2048)
            assets.save_loaded_asset(picture, False)


class Placer:
    """Collects instances per material and spawns one AVaelScatterField each"""

    def __init__(self, mesh):
        self.mesh = mesh
        self.groups = {}

    def add(self, variant, material, location, rotation, scale):
        group = self.groups.setdefault(variant, {"material": material, "t": []})
        group["t"].append(unreal.Transform(location, rotation, scale))

    def spawn(self):
        total = 0
        for variant, group in sorted(self.groups.items()):
            field = actors.spawn_actor_from_class(unreal.VaelScatterField, group["t"][0].translation, unreal.Rotator(0.0, 0.0, 0.0))
            field.setup_field(self.mesh, 0.0, True, True)
            field.instances.set_material(0, group["material"])
            field.add_scatter_instances(group["t"])
            field.set_actor_label("Riesenpilze_" + variant)
            field.set_folder_path(FOLDER)
            field.set_editor_property("tags", [unreal.Name(TAG)])
            total += len(group["t"])
        return total


def place_one(place, instances, variant, x, y, ground_z, height_cm, model, lean_max):
    lo, hi = model
    scale = height_cm / (hi.z - lo.z)
    wide = scale * rng.uniform(0.85, 1.25)
    yaw = rng.uniform(0, 360)
    lean = rng.uniform(0, lean_max)
    lean_dir = rng.uniform(0, 360)
    rotation = unreal.MathLibrary.compose_rotators(unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
                                                   unreal.MathLibrary.compose_rotators(unreal.Rotator(roll=0.0, pitch=-lean, yaw=0.0),
                                                                                       unreal.Rotator(roll=0.0, pitch=0.0, yaw=lean_dir)))
    sink = rng.uniform(0.02, 0.05) * height_cm
    foot = unreal.Vector(x, y, ground_z - sink - lo.z * scale)
    place.add(variant, instances[variant], foot, rotation, unreal.Vector(wide, wide, scale))


def main():
    shape = json.load(open(MUSHROOM_DATA, encoding="utf-8"))
    boxes = json.load(open(os.path.join(KIT, "WF_Platzhalter.json"), encoding="utf-8"))["prims"]

    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    WORLD[0] = world
    level_actors = actors.get_all_level_actors()
    removed = 0
    for actor in level_actors:
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1
    hidden = 0
    for actor in level_actors:
        label = actor.get_actor_label()
        grey_box = (label.startswith("Riesenpilz_") and label.endswith(("_Stiel", "_Hut"))) or \
                   (label.startswith("Pilzinsel_") and label.endswith(("_Pilzstiel", "_Pilzhut")))
        if grey_box and unreal.Name(HIDE_TAG) not in actor.tags and unreal.Name(TAG) not in actor.tags:
            actor.root_component.set_visibility(False, True)
            actor.set_actor_hidden_in_game(True)
            actor.set_actor_enable_collision(False)
            actor.set_editor_property("tags", list(actor.tags) + [unreal.Name(HIDE_TAG)])
            hidden += 1

    mesh = unreal.load_asset(MUSHROOM + "/SM_Riesenpilz")
    if mesh is None:
        log("SM_Riesenpilz missing: import Vael_Quellen/Riesenpilz with Tools/Kreaturenfabrik/unreal/vael_kreatur_import.py first")
        return
    stem_only_collision(mesh, shape)
    lighten(mesh)
    material, parts = build_material(shape)
    finish_material(material, parts)
    instances = build_variants(material)
    box = mesh.get_bounding_box()
    model = (box.min, box.max)
    place = Placer(mesh)
    names = list(VARIANTS)
    counts = {"giant": 0, "island": 0, "small": 0, "no_ground": 0, "giant_ground": 0, "giant_no_ground": 0, "miss": 0, "steep": 0}

    caps = {p["n"][:-len("_Hut")]: p["mat"] for p in boxes if p["n"].startswith("Riesenpilz_") and p["n"].endswith("_Hut")}
    for prim in boxes:
        name = prim["n"]
        island = name.startswith("Pilzinsel_") and name.endswith("_Pilzstiel")
        if not (island or (name.startswith("Riesenpilz_") and name.endswith("_Stiel"))):
            continue
        x, y, z = prim["l"]
        x, y = x + MAP_ORIGIN, y + MAP_ORIGIN
        stem_height = prim["s"][2] * 100.0
        bottom = z - stem_height / 2.0
        hit = trace(unreal.Vector(x, y, bottom + 3000), unreal.Vector(x, y, bottom - 3000))
        ground_z = hit[0].z if hit else bottom
        counts["giant_ground" if hit else "giant_no_ground"] += 1
        if island:
            height = ISLAND_HEIGHT_CM * rng.uniform(0.9, 1.1)
            variant = "Koralle"
            counts["island"] += 1
        else:
            height = min(max(stem_height * HEIGHT_SHARE, HEIGHT_MIN_CM), HEIGHT_MAX_CM)
            variant = BOX_HINT.get(caps.get(name[:-len("_Stiel")]), "") or rng.choice(names)
            if rng.random() < 0.35:
                variant = rng.choice(names)
            counts["giant"] += 1
        place_one(place, instances, variant, x, y, ground_z, height, model, 5.0)

        # A few small ones of the same kind around it, a little further out than the cap
        for _ in range(rng.randint(*SMALL_PER_GIANT)):
            angle = rng.uniform(0, math.tau)
            reach = height * rng.uniform(0.45, 0.9)
            sx, sy = x + math.cos(angle) * reach, y + math.sin(angle) * reach
            spot = trace(unreal.Vector(sx, sy, ground_z + 2500), unreal.Vector(sx, sy, ground_z - 2500))
            if spot is None or spot[1].z < 0.75:
                counts["no_ground"] += 1
                counts["miss" if spot is None else "steep"] += 1
                continue
            small_variant = variant if rng.random() < 0.7 else rng.choice(names)
            place_one(place, instances, small_variant, sx, sy, spot[0].z, rng.uniform(*SMALL_HEIGHT_CM), model, 12.0)
            counts["small"] += 1

    total = place.spawn()
    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("mushrooms placed: {} giants, {} on islands, {} small ({} spots without flat ground skipped), {} instances; {} grey box parts hidden, {} old pieces removed".format(
        counts["giant"], counts["island"], counts["small"], counts["no_ground"], total, hidden, removed))
    log("traces: {}".format(counts))
    for note in notes:
        log("NOTE " + note)


main()
