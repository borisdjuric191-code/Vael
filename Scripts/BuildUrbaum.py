# Dresses the Urbaum, the great tree of the Ewiger Hain, in L_Wurzelforst_Blockout.
#
# The tree itself is the Tripo model SM_Urbaum, which wf_blockout.py sets in place of the Urstamm placeholders
# (MODELLE "TRIPO_1"). This script builds everything around it, from the measured branch ends in Scripts/Data/Urbaum_baum.json
# (Tools/Kreaturenfabrik/blender/vael_baum_vermessen.py):
# - Material M_WF_Urbaum: the Tripo bark with fine real bark laid over the world (the model's own texture is only 2K
#   for 48 m), moss on everything that faces up, and the foot fading into the moss of the forest floor.
# - Crown: Old Tree Branch arms from every branch end, crowned with large clusters of elderberry leaves, a few palm
#   fronds bursting out; ivy climbing the trunk, moss pads on roots and branches.
# - Lichterblueten (lantern blossoms) hanging irregularly under the crown, warm glow from the material, a few soft lights.
# - Foot: ferns, sorrel and violet wood sorrel between the roots, the light blue Taublatt (own wet material M_WF_Taublatt)
#   alone or in small groups, moss over the seams where root meets ground.
# - The grey box crowns of the Urstamm and its green placeholder light are switched off (not deleted).
# Everything it adds is tagged WF_Urbaum (Outliner folder Wurzelforst/3_Ewiger_Hain/Urbaum); rerunning replaces only that.
# Rerun it after wf_blockout.py (which puts the tree back to its default spot and material).
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildUrbaum.py" -EnablePlugins=PythonScriptPlugin

import json
import math
import os
import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
ASSET_FOLDER = "/Game/Vael/Wurzelforst/Urbaum"
TREE_DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "Urbaum_baum.json")
TAG = "WF_Urbaum"
HIDE_TAG = "WF_UrbaumVerdeckt"
FOLDER = "Wurzelforst/3_Ewiger_Hain/Urbaum"
SINK = 160.0  # cm the model sits below the ground at the trunk: hides its base plate, and the hill falls away under the outer roots
SEED = 48

TRIPO = "/Game/Vael/Requisiten/Wurzelforst"
FAB = "/Game/Fab/Megascans"
PACKS = {
    "branch": FAB + "/3D/Old_Tree_Branch_whnnajh",
    "leaves": FAB + "/Plants/Elderberry_wfzobb2ia",
    "palm": FAB + "/Plants/Alexandra_Palm_ukgkcilia",
    "ivy": FAB + "/Plants/English_Ivy_xfcnebhqx",
    "violet": FAB + "/Plants/Violet_Wood_Sorrel_uchkajuia",
    "fern": FAB + "/Plants/Lady_Fern_wdvlditia",
    "sorrel": FAB + "/Plants/Wood_Sorrel_wcwmchzja",
    "balsam": FAB + "/Plants/Small_Balsam_xefmcgrqx",
    "moss": "/Game/Megascans/3D_Assets/IcelandicMossClusters",
}
DEW = TRIPO.replace("/Wurzelforst", "/Aschenmark") + "/Taublatt"  # the light blue dew plant with water in its cups
BARK_DETAIL = FAB + "/3D/Old_Tree_Branch_whnnajh/Raw/whnnajh_tier_0/Textures/T_whnnajh_8K_B"
MOSS_TEXTURE = "/Game/Megascans/3D_Assets/IcelandicMossClusters/T_IcelandicMossClusters_A"

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
notes = []
WORLD = [None]


def log(message):
    unreal.log_warning("[BuildUrbaum] " + message)


# ---------------------------------------------------------------- Materials

class Graph:
    """Places material nodes in columns and checks every link (as in DressWurzelforst.py)"""

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

    def sample(self, texture, column, uvs=None, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        expression = self.node(unreal.MaterialExpressionTextureSample, column, texture=texture, sampler_type=sampler)
        if uvs is not None:
            self.link(uvs, expression, "UVs")
        return expression

    def world_aligned(self, texture, size, column):
        """Texture projected from the three axes, so the trunk doesn't stretch it"""
        texture_object = self.node(unreal.MaterialExpressionTextureObject, column + 1, texture=texture)
        call = self.node(unreal.MaterialExpressionMaterialFunctionCall, column)
        call.set_material_function(unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"))
        self.link(texture_object, call, "TextureObject")
        size3 = self.op(unreal.MaterialExpressionMultiply, column + 1, size,
                        self.node(unreal.MaterialExpressionConstant3Vector, column + 2, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0)))
        self.link(size3, call, "TextureSize")
        return self.mask(call, column - 1)


def new_material(name):
    path = "{}/{}".format(ASSET_FOLDER, name)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
        return material
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ASSET_FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def texture(path):
    loaded = unreal.load_asset(path)
    if loaded is None:
        notes.append("missing texture " + path)
    return loaded


def build_bark_material():
    material = new_material("M_WF_Urbaum")
    g = Graph(material)
    base = g.op(unreal.MaterialExpressionMultiply, 8, g.mask(g.sample(texture(TRIPO + "/Urbaum/T_Urbaum_basecolor"), 10), 9),
                g.scalar("RindeHelligkeit", 1.8, 9))
    normal_map = g.sample(texture(TRIPO + "/Urbaum/T_Urbaum_normal"), 4, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)

    # Fine real bark over the soft Tripo bark, at two sizes
    detail = g.lerp(g.world_aligned(texture(BARK_DETAIL), g.scalar("RindeKachel", 180.0, 11), 8),
                    g.world_aligned(texture(BARK_DETAIL), g.const(530.0, 11), 8), g.const(0.4, 8), 7)
    detail_bright = g.op(unreal.MaterialExpressionMultiply, 6, g.grey(detail, 7), g.const(2.2, 7))
    bark = g.op(unreal.MaterialExpressionMultiply, 5, base,
                g.lerp(g.const(1.0, 6), detail_bright, g.scalar("RindeDetail", 0.85, 6), 5))

    # Moss on what faces up, broken up by the moss texture itself
    moss = g.op(unreal.MaterialExpressionMultiply, 6, g.grey(g.world_aligned(texture(MOSS_TEXTURE), g.const(260.0, 11), 8), 7),
                g.color("MoosFarbe", (0.42, 0.66, 0.16), 8))
    normal_z = g.mask(g.node(unreal.MaterialExpressionVertexNormalWS, 9), 8, r=False, g=False, b=True)
    facing_up = g.saturate(g.op(unreal.MaterialExpressionMultiply, 6,
                                g.op(unreal.MaterialExpressionSubtract, 7, normal_z, g.scalar("MoosAb", 0.45, 8)), g.const(3.0, 7)), 5)
    moss_share = g.op(unreal.MaterialExpressionMultiply, 4, facing_up, g.scalar("MoosAnteil", 0.7, 5))
    colored = g.lerp(bark, moss, moss_share, 4)

    # The foot fades into the moss of the forest floor, so roots and ground don't meet in a hard line
    height = g.op(unreal.MaterialExpressionSubtract, 7,
                  g.mask(g.node(unreal.MaterialExpressionWorldPosition, 9), 8, r=False, g=False, b=True),
                  g.mask(g.node(unreal.MaterialExpressionObjectPositionWS, 9), 8, r=False, g=False, b=True))
    foot = g.saturate(g.op(unreal.MaterialExpressionDivide, 5,
                           g.op(unreal.MaterialExpressionSubtract, 6, g.scalar("BodenBis", 260.0, 7), height), g.const(180.0, 6)), 4)
    ground_moss = g.op(unreal.MaterialExpressionMultiply, 5, moss, g.const(0.8, 6))
    albedo = g.lerp(colored, ground_moss, g.op(unreal.MaterialExpressionMultiply, 3, foot, g.const(0.85, 4)), 2)

    library.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal_map, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(g.lerp(g.const(0.82, 3), g.const(0.95, 3), moss_share, 2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_dew_material():
    """The Taublatt's own colours, wet and glowing a little blue (its Tripo master can't be drawn as instances)"""
    material = new_material("M_WF_Taublatt")
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)
    base = g.op(unreal.MaterialExpressionMultiply, 6, g.mask(g.sample(texture(DEW + "/T_Taublatt_basecolor"), 8), 7),
                g.color("TauFarbe", (1.0, 1.08, 1.2), 8))
    glow = g.op(unreal.MaterialExpressionMultiply, 4, g.op(unreal.MaterialExpressionMultiply, 5, base, g.scalar("TauLeuchten", 0.6, 6)),
                g.lerp(g.const(0.7, 6), g.const(1.3, 6), g.node(unreal.MaterialExpressionPerInstanceRandom, 6), 5))
    normal_map = g.sample(texture(DEW + "/T_Taublatt_normal"), 4, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough = g.op(unreal.MaterialExpressionMultiply, 3, g.mask(g.sample(texture(DEW + "/T_Taublatt_roughness"), 5,
                                                                      sampler=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR), 4, g=False, b=False),
                 g.scalar("Nass", 0.55, 4))
    library.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(normal_map, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_lantern_material():
    """The blossom's own petals, lit warm from inside; every blossom a little brighter or dimmer"""
    material = new_material("M_WF_Lichterbluete")
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)
    base = g.mask(g.sample(texture(TRIPO + "/Lichterbluete/T_Lichterbluete_basecolor"), 8), 7)
    variation = g.lerp(g.scalar("LeuchtenMin", 0.55, 5), g.const(1.25, 5), g.node(unreal.MaterialExpressionPerInstanceRandom, 5), 4)
    glow = g.op(unreal.MaterialExpressionMultiply, 3,
                g.op(unreal.MaterialExpressionMultiply, 4, g.op(unreal.MaterialExpressionMultiply, 5, base, g.color("Glut", (1.0, 0.55, 0.22), 7)),
                     g.scalar("Leuchten", 6.0, 5)), variation)
    library.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(g.const(0.6, 2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


# ---------------------------------------------------------------- Meshes and placement

def pack_meshes(folder):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = [d.get_asset() for d in registry.get_assets_by_path(folder, recursive=True)
             if str(d.asset_class_path.asset_name) == "StaticMesh"]
    if not found:
        notes.append("no meshes in " + folder)
    return sorted(found, key=lambda m: m.get_name())


def allow_instancing(meshes):
    for mesh in meshes:
        for slot in mesh.static_materials:
            if slot.material_interface is None:
                continue
            base = slot.material_interface.get_base_material()
            if not base.get_editor_property("used_with_instanced_static_meshes"):
                library.set_material_usage(base, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
                library.recompile_material(base)
                assets.save_loaded_asset(base, False)
                notes.append("instancing allowed on " + base.get_name())


def extent(mesh):
    box = mesh.get_bounding_box()
    return box.min, box.max


class Placer:
    """Collects instances per mesh and spawns one AVaelScatterField per mesh at the end"""

    def __init__(self):
        self.instances = {}
        self.settings = {}

    def add(self, mesh, location, rotation, scale, cull=0.0, shadow=True, material=None):
        key = mesh.get_path_name()
        self.instances.setdefault(key, (mesh, []))[1].append(
            unreal.Transform(location, rotation, unreal.Vector(scale, scale, scale)))
        self.settings[key] = (cull, shadow, material)

    def spawn(self, kind_of):
        total = 0
        for key, (mesh, transforms) in sorted(self.instances.items()):
            cull, shadow, material = self.settings[key]
            field = actors.spawn_actor_from_class(unreal.VaelScatterField, transforms[0].translation, unreal.Rotator(0.0, 0.0, 0.0))
            field.setup_field(mesh, float(cull), shadow, False)
            if material is not None:
                field.instances.set_material(0, material)
            field.add_scatter_instances(transforms)
            field.set_actor_label("Urbaum_{}_{}".format(kind_of.get(key, "Teil"), mesh.get_name()))
            field.set_folder_path(FOLDER)
            field.set_editor_property("tags", [unreal.Name(TAG)])
            total += len(transforms)
        return total


def trace(start, end):
    hit = unreal.SystemLibrary.line_trace_single(WORLD[0], start, end,
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    values = hit.to_tuple()
    if not values[0]:
        return None
    return values[4], values[7], values[9]


def rot_from_z(direction, spin):
    return unreal.MathLibrary.compose_rotators(unreal.Rotator(roll=0.0, pitch=0.0, yaw=spin), unreal.MathLibrary.make_rot_from_z(direction))


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    WORLD[0] = world
    level_actors = actors.get_all_level_actors()

    tree = next((a for a in level_actors if {"TRIPO_1", "MODELL"} <= {str(t) for t in a.tags}), None)
    if tree is None:
        log("no Urbaum model in the level: enter SM_Urbaum at MODELLE TRIPO_1 in wf_blockout.py and run it first")
        return
    landscape = next((a for a in level_actors if isinstance(a, unreal.LandscapeProxy)), None)

    removed = 0
    for actor in level_actors:
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1
    # The grey box crowns would cover the real one: switched off (not deleted)
    hidden = 0
    for actor in level_actors:
        label = actor.get_actor_label()
        # (also its green placeholder light: it lit the leaves cyan; the lantern blossoms bring the light now)
        if (label.startswith("Urstamm_Krone") or label == "Urstamm_Licht") and unreal.Name(HIDE_TAG) not in actor.tags:
            actor.root_component.set_visibility(False, True)
            actor.set_actor_hidden_in_game(True)
            actor.set_actor_enable_collision(False)
            actor.set_editor_property("tags", list(actor.tags) + [unreal.Name(HIDE_TAG)])
            hidden += 1

    data = json.load(open(TREE_DATA, encoding="utf-8"))
    bark = build_bark_material()
    lantern_material = build_lantern_material()
    dew_material = build_dew_material()
    tree.static_mesh_component.set_material(0, bark)

    # True size (48 m) and standing on the ground at the trunk: wf_blockout.py sizes it by the grey box, whose long roots
    # reach far down the hill, which left it too big and metres too deep. The trace passes through the tree (no complex collision).
    location = tree.get_actor_location()
    ground = trace(unreal.Vector(location.x, location.y, location.z + 20000), unreal.Vector(location.x, location.y, location.z - 20000))
    ground_z = ground[0].z if ground else location.z
    tree.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
    tree.set_actor_location(unreal.Vector(location.x, location.y, ground_z - SINK), False, False)

    origin = tree.get_actor_location()
    yaw = tree.get_actor_rotation().yaw
    scale = tree.get_actor_scale3d().x

    def to_world(local):
        x, y, z = local
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        return unreal.Vector(origin.x + (x * c - y * s) * scale, origin.y + (x * s + y * c) * scale, origin.z + z * scale)

    def world_dir(local):
        x, y, z = local
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        return unreal.Vector(x * c - y * s, x * s + y * c, z).normal()

    meshes = {key: pack_meshes(folder) for key, folder in PACKS.items()}
    lantern = unreal.load_asset(TRIPO + "/Lichterbluete/SM_Lichterbluete")
    allow_instancing([m for group in meshes.values() for m in group])
    place = Placer()
    kinds = {}

    def put(kind, mesh, location, rotation, size_cm, **settings):
        lo, hi = extent(mesh)
        biggest = max(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, 1.0)
        place.add(mesh, location, rotation, size_cm / biggest, **settings)
        kinds[mesh.get_path_name()] = kind

    height = data["hoehe_cm"] * scale
    trunk_radius = data["stamm_radius_cm"] * scale
    ends = [(to_world(e["ort_cm"]), world_dir(e["richtung"])) for e in data["astenden"]]
    crown_points = []

    # Crown: two arms of old branch from every branch end, leaf clusters around and above them
    branch = meshes["branch"][0] if meshes["branch"] else None
    for end, direction in ends:
        flat = unreal.Vector(direction.x, direction.y, 0.0).normal()
        for spread, lift in ((-28.0, 18.0), (24.0, 34.0)):
            arm = unreal.MathLibrary.rotate_angle_axis(flat, spread + rng.uniform(-8, 8), unreal.Vector(0, 0, 1))
            arm = unreal.Vector(arm.x, arm.y, math.tan(math.radians(lift + rng.uniform(-6, 6)))).normal()
            length = rng.uniform(1300.0, 1800.0) * scale
            if branch is not None:
                lo, hi = extent(branch)
                axis = max((hi.x - lo.x, "x"), (hi.y - lo.y, "y"), (hi.z - lo.z, "z"))[1]
                rotation = {"x": unreal.MathLibrary.make_rot_from_x(arm), "y": unreal.MathLibrary.make_rot_from_y(arm),
                            "z": unreal.MathLibrary.make_rot_from_z(arm)}[axis]
                middle = end + arm * (length * 0.45)
                put("Ast", branch, middle, rotation, length)
            tip = end + arm * length
            crown_points.append(tip)
            crown_points.append(end + arm * (length * 0.5) + unreal.Vector(0, 0, 300 * scale))
    # A dome over the trunk top as well
    top = to_world((0.0, 0.0, data["hoehe_cm"]))
    for i in range(10):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(0, 1200) * scale
        crown_points.append(top + unreal.Vector(math.cos(a) * r, math.sin(a) * r, rng.uniform(-200, 500) * scale))

    leaf_clusters = []
    for point in crown_points:
        for _ in range(3):
            offset = unreal.Vector(rng.uniform(-550, 550), rng.uniform(-550, 550), rng.uniform(-450, 350)) * scale
            center = point + offset
            size = rng.uniform(900.0, 1400.0) * scale
            mesh = rng.choice(meshes["leaves"]) if meshes["leaves"] else None
            if mesh is None:
                continue
            put("Laub", mesh, center - unreal.Vector(0, 0, size * 0.35), unreal.Rotator(roll=rng.uniform(-12, 12), pitch=rng.uniform(-12, 12),
                                                                                            yaw=rng.uniform(0, 360)), size, cull=0.0, shadow=True)
            leaf_clusters.append((center, size * 0.5))
    # Palm fronds bursting out of the leaves at the edge, tilted outward (dragon tree tufts were tried: a blue blob from the camera)
    for point in crown_points[::2]:
        outward = unreal.Vector(point.x - origin.x, point.y - origin.y, 0.0).normal()
        tilt = unreal.Vector(outward.x, outward.y, 1.3).normal()
        if meshes["palm"]:
            put("Palmwedel", rng.choice(meshes["palm"]), point + unreal.Vector(0, 0, rng.uniform(-200, 100) * scale),
                rot_from_z(tilt, rng.uniform(0, 360)), rng.uniform(700, 1000) * scale)

    # Ivy climbing the trunk and moss pads on roots and branches, on surface spots measured on the model
    for spot in data.get("efeu_punkte", []):
        if meshes["ivy"]:
            normal = world_dir(spot["normale"])
            put("Efeu", rng.choice(meshes["ivy"]), to_world(spot["ort_cm"]) - normal * 15, rot_from_z(normal, rng.uniform(0, 360)),
                rng.uniform(220, 380) * scale, cull=0.0, shadow=False)
    moss_on_tree = 0
    for spot in data.get("moos_punkte", []):
        if meshes["moss"]:
            normal = world_dir(spot["normale"])
            put("Moos", rng.choice(meshes["moss"]), to_world(spot["ort_cm"]) - normal * 10, rot_from_z(normal, rng.uniform(0, 360)),
                rng.uniform(160, 320) * scale, cull=0.0, shadow=False)
            moss_on_tree += 1

    # Ground spots around the foot (the traces pass through the tree and land on the landscape)
    reach = data["wurzel_reichweite_cm"] * scale
    foot_spots = []
    for i in range(1000):
        a = rng.uniform(0, 2 * math.pi)
        r = math.sqrt(rng.random()) * reach * 1.15
        x, y = origin.x + math.cos(a) * r, origin.y + math.sin(a) * r
        hit = trace(unreal.Vector(x, y, origin.z + height * 0.7), unreal.Vector(x, y, origin.z - 3000))
        if not hit:
            continue
        location, normal, actor = hit
        if landscape is not None and actor == landscape and r > trunk_radius * 1.1:
            foot_spots.append((location, normal, r))

    # The foot: ferns, sorrel and violet sorrel between the roots, thickest near the trunk; moss right at the seams
    for location, normal, r in foot_spots:
        near = 1.0 - min(1.0, r / (reach * 1.15))
        roll = rng.random()
        spin = rot_from_z(normal, rng.uniform(0, 360))
        if roll < 0.30 and meshes["fern"]:
            put("Farn", rng.choice(meshes["fern"]), location, spin, rng.uniform(180, 280) * (0.8 + near * 0.9), cull=0.0, shadow=True)
        elif roll < 0.38 and meshes["leaves"]:
            put("Holunder", rng.choice(meshes["leaves"]), location - unreal.Vector(0, 0, 30), spin, rng.uniform(250, 420), cull=0.0, shadow=True)
        elif roll < 0.58 and meshes["violet"]:
            put("Veilchenklee", rng.choice(meshes["violet"]), location, spin, rng.uniform(70, 130), cull=0.0, shadow=False)
        elif roll < 0.73 and meshes["balsam"]:
            put("Springkraut", rng.choice(meshes["balsam"]), location, spin, rng.uniform(60, 110), cull=0.0, shadow=False)
        elif roll < 0.85 and meshes["sorrel"]:
            put("Sauerklee", rng.choice(meshes["sorrel"]), location, spin, rng.uniform(50, 90), cull=0.0, shadow=False)
        elif meshes["moss"]:
            put("Moos", rng.choice(meshes["moss"]), location - unreal.Vector(0, 0, 8), rot_from_z(normal, rng.uniform(0, 360)),
                rng.uniform(200, 380), cull=0.0, shadow=False)

    # Taublatt between the roots, alone or in twos and threes, standing up out of the moss (its clod sunk into the ground)
    dew = unreal.load_asset(DEW + "/SM_Taublatt")
    dew_count = 0
    if dew is not None:
        spots = [s for s in foot_spots if trunk_radius * 1.25 < s[2] < reach * 0.95]
        rng.shuffle(spots)
        for location, normal, r in spots[:24]:
            for _ in range(rng.choice((1, 2, 2, 3))):
                a = rng.uniform(0, 2 * math.pi)
                d = rng.uniform(0, 220)
                hit = trace(unreal.Vector(location.x + math.cos(a) * d, location.y + math.sin(a) * d, location.z + 600),
                            unreal.Vector(location.x + math.cos(a) * d, location.y + math.sin(a) * d, location.z - 600))
                spot = hit[0] if hit and hit[2] == landscape else location
                size = rng.uniform(160, 300)
                upright = (normal * 0.35 + unreal.Vector(0, 0, 0.65)).normal()
                put("Taublatt", dew, spot - unreal.Vector(0, 0, size * 0.12), rot_from_z(upright, rng.uniform(0, 360)), size,
                    cull=0.0, shadow=True, material=dew_material)
                dew_count += 1

    # Lantern blossoms hanging under the leaf clusters at irregular depths; their stem tip is the origin's top
    hung = 0
    light_spots = []
    if lantern is not None:
        lo, hi = extent(lantern)
        stem = hi.z - lo.z
        for center, radius in leaf_clusters:
            for _ in range(rng.randint(2, 5)):
                a = rng.uniform(0, 2 * math.pi)
                r = math.sqrt(rng.random()) * radius * 0.85
                size = rng.uniform(1.6, 3.2)
                hang = unreal.Vector(center.x + math.cos(a) * r, center.y + math.sin(a) * r,
                                     center.z - radius * rng.uniform(0.35, 0.75) - rng.uniform(0, 500) * scale)
                place.add(lantern, hang - unreal.Vector(0, 0, stem * size),
                          unreal.Rotator(roll=rng.uniform(-6, 6), pitch=rng.uniform(-6, 6), yaw=rng.uniform(0, 360)), size,
                          cull=0.0, shadow=False, material=lantern_material)
                kinds[lantern.get_path_name()] = "Lichterbluete"
                hung += 1
                if rng.random() < 0.07:
                    light_spots.append(hang - unreal.Vector(0, 0, stem * size * 0.5))

    total = place.spawn(kinds)

    lights = 0
    for spot in light_spots[:16]:
        light = actors.spawn_actor_from_class(unreal.PointLight, spot, unreal.Rotator(0, 0, 0))
        component = light.get_component_by_class(unreal.PointLightComponent)
        component.set_editor_property("intensity", 700.0)  # soft: the crown must stay green, the blossoms glow by themselves
        component.set_editor_property("attenuation_radius", 1200.0)
        component.set_editor_property("light_color", unreal.Color(r=255, g=170, b=90, a=255))  # named: positional order is not r, g, b
        component.set_editor_property("cast_shadows", False)
        light.set_actor_label("Urbaum_Laternenlicht_{:02d}".format(lights + 1))
        light.set_folder_path(FOLDER)
        light.set_editor_property("tags", [unreal.Name(TAG)])
        lights += 1

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("Urbaum dressed: {} pieces ({} lantern blossoms, {} Taublatt, {} moss pads on the tree), {} lights, {} grey box crowns hidden, {} old pieces removed".format(
        total, hung, dew_count, moss_on_tree, lights, hidden, removed))
    for note in notes:
        log("NOTE " + note)


main()
