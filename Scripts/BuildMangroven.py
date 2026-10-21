# Puts the Tripo mangrove (SM_Mangrove) at the 56 grey box mangroves of the Pfuhlmoor in L_Wurzelforst_Blockout.
#
# - Material M_WF_Mangrove: the Tripo bark darkened, fine real bark over it, moss on what faces up; dark and wet below
#   a line just above the water, a band of green algae right at the waterline, darker still under the water.
#   MI_WF_Mangrove_Tot: the same grey and bare, for the dead ones.
# - Every tree different: size, how deep it is sunk (in the water more roots vanish under the surface), a slight lean,
#   its turn; about one in eight is dead, leaning further, sunk deeper and without leaves.
# - Crowns: elderberry leaf clusters at the branch ends measured on the model (Scripts/Data/Mangrove_baum.json, from
#   Tools/Kreaturenfabrik/blender/vael_baum_vermessen.py), kept thin so the tree stays eerie, ivy hanging from them.
# - Collision on the real shape (SM_Mangrove and SM_Wurzelbogen use their complex collision): players walk between and
#   under the roots and through the root arches.
# - The grey box trunks, stilts and crowns of the mangroves are switched off (not deleted).
# Everything it adds is tagged WF_Mangrove (Outliner folder Wurzelforst/2_Pfuhlmoor/Mangroven); rerunning replaces only that.
# Needs WF_Moor.json from the kit's wf_moor.py (the places with their ground height).
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildMangroven.py" -EnablePlugins=PythonScriptPlugin

import json
import math
import os
import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KIT = os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout")
TREE_DATA = os.path.join(ROOT, "Scripts", "Data", "Mangrove_baum.json")
MATERIAL_FOLDER = "/Game/Vael/Wurzelforst/Blockout/Materialien"
MANGROVE = "/Game/Vael/Requisiten/Wurzelforst/Mangrove"
ARCH = "/Game/Vael/Requisiten/Wurzelforst/Wurzelbogen/SM_Wurzelbogen"
FAB = "/Game/Fab/Megascans"
LEAVES = FAB + "/Plants/Elderberry_wfzobb2ia"
IVY = FAB + "/Plants/English_Ivy_xfcnebhqx"
BARK_DETAIL = FAB + "/3D/Old_Tree_Branch_whnnajh/Raw/whnnajh_tier_0/Textures/T_whnnajh_8K_B"
MOSS_TEXTURE = "/Game/Megascans/3D_Assets/IcelandicMossClusters/T_IcelandicMossClusters_A"
TAG = "WF_Mangrove"
HIDE_TAG = "WF_MangroveVerdeckt"
FOLDER = "Wurzelforst/2_Pfuhlmoor/Mangroven"
SEED = 56
DEAD_SHARE = 0.12

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
notes = []


def log(message):
    unreal.log_warning("[BuildMangroven] " + message)


# ---------------------------------------------------------------- Material

class Graph:
    """Places material nodes in columns and checks every link (as in BuildUrbaum.py)"""

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

    def sub(self, column, a, b):
        return self.op(unreal.MaterialExpressionSubtract, column, a, b)

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

    def sample(self, texture, column, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        return self.node(unreal.MaterialExpressionTextureSample, column, texture=texture, sampler_type=sampler)

    def world_aligned(self, texture, size, column):
        texture_object = self.node(unreal.MaterialExpressionTextureObject, column + 1, texture=texture)
        call = self.node(unreal.MaterialExpressionMaterialFunctionCall, column)
        call.set_material_function(unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"))
        self.link(texture_object, call, "TextureObject")
        size3 = self.mul(column + 1, size, self.node(unreal.MaterialExpressionConstant3Vector, column + 2, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0)))
        self.link(size3, call, "TextureSize")
        return self.mask(call, column - 1)


def texture(path):
    loaded = unreal.load_asset(path)
    if loaded is None:
        notes.append("missing texture " + path)
    return loaded


def build_material():
    path = "{}/M_WF_Mangrove".format(MATERIAL_FOLDER)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_WF_Mangrove", MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)

    # The Tripo bark is pale: darkened and warmed, fine real bark laid over it
    base = g.mul(8, g.mul(9, g.mask(g.sample(texture(MANGROVE + "/T_Mangrove_basecolor"), 11), 10), g.scalar("RindeHelligkeit", 0.85, 10)),
                 g.color("RindeFarbe", (0.95, 0.88, 0.76), 10))
    detail = g.grey(g.world_aligned(texture(BARK_DETAIL), g.scalar("RindeKachel", 160.0, 12), 9), 8)
    bark = g.mul(6, base, g.lerp(g.const(1.0, 7), g.mul(7, detail, g.const(2.2, 8)), g.scalar("RindeDetail", 0.7, 7), 6))

    # Moss on what faces up
    moss = g.mul(6, g.grey(g.world_aligned(texture(MOSS_TEXTURE), g.const(240.0, 12), 9), 8), g.color("MoosFarbe", (0.36, 0.58, 0.14), 8))
    normal_z = g.mask(g.node(unreal.MaterialExpressionVertexNormalWS, 9), 8, r=False, g=False, b=True)
    facing_up = g.saturate(g.mul(6, g.sub(7, normal_z, g.const(0.35, 8)), g.const(2.5, 7)), 5)
    moss_share = g.mul(4, facing_up, g.scalar("MoosAnteil", 0.6, 5))
    colored = g.lerp(bark, moss, moss_share, 4)

    # Water: wet and dark below a line just above the surface, green algae on the waterline, murkier under it
    world_z = g.mask(g.node(unreal.MaterialExpressionWorldPosition, 9), 8, r=False, g=False, b=True)
    noise = g.node(unreal.MaterialExpressionNoise, 9, scale=0.01, levels=2, output_min=-1.0, output_max=1.0,
                   noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX)
    g.link(g.node(unreal.MaterialExpressionWorldPosition, 10), noise)
    height = g.sub(7, world_z, g.mul(8, noise, g.const(12.0, 9)))
    wet = g.saturate(g.op(unreal.MaterialExpressionDivide, 5, g.sub(6, g.scalar("NassBis", 70.0, 7), height), g.const(50.0, 6)), 4)
    # Algae band: 1 at the waterline, fading out above and below
    offset = g.op(unreal.MaterialExpressionDivide, 6, g.sub(7, height, g.scalar("AlgenHoehe", 8.0, 8)), g.scalar("AlgenBreite", 22.0, 7))
    absolute = g.node(unreal.MaterialExpressionAbs, 5)
    g.link(offset, absolute)
    algae = g.saturate(g.sub(4, g.const(1.0, 5), absolute), 3)
    under = g.saturate(g.op(unreal.MaterialExpressionDivide, 5, g.mul(6, height, g.const(-1.0, 7)), g.const(60.0, 6)), 4)
    wet_color = g.mul(3, colored, g.lerp(g.const(1.0, 4), g.scalar("NassDunkel", 0.55, 4), wet, 3))
    with_algae = g.lerp(wet_color, g.color("AlgenFarbe", (0.08, 0.14, 0.03), 4), g.mul(3, algae, g.scalar("Algen", 0.75, 4)), 2)
    albedo = g.lerp(with_algae, g.mul(3, with_algae, g.color("UnterWasser", (0.45, 0.6, 0.5), 4)), under, 1)

    normal_map = g.sample(texture(MANGROVE + "/T_Mangrove_normal"), 3, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    roughness = g.lerp(g.lerp(g.const(0.85, 3), g.const(0.95, 3), moss_share, 2), g.const(0.35, 2), wet, 1)
    library.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal_map, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)

    # The dead ones: grey, bare, no moss
    dead_path = "{}/MI_WF_Mangrove_Tot".format(MATERIAL_FOLDER)
    if assets.does_asset_exist(dead_path):
        dead = unreal.load_asset(dead_path)
    else:
        dead = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_WF_Mangrove_Tot", MATERIAL_FOLDER, unreal.MaterialInstanceConstant,
                                                                       unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(dead, material)
    library.set_material_instance_scalar_parameter_value(dead, "MoosAnteil", 0.05)
    library.set_material_instance_scalar_parameter_value(dead, "RindeHelligkeit", 0.7)
    library.set_material_instance_vector_parameter_value(dead, "RindeFarbe", unreal.LinearColor(0.8, 0.8, 0.78, 1.0))
    library.update_material_instance(dead)
    assets.save_loaded_asset(dead, False)
    return material, dead


# ---------------------------------------------------------------- Placement

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
            # Only project materials: engine and plugin content must never be changed (give such meshes an own material)
            if not base.get_path_name().startswith("/Game/"):
                notes.append("not instancing-enabled (outside /Game): " + base.get_path_name())
                continue
            if not base.get_editor_property("used_with_instanced_static_meshes"):
                library.set_material_usage(base, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
                library.recompile_material(base)
                assets.save_loaded_asset(base, False)


def walk_through(mesh):
    """Collision on the real shape, so players pass between the roots and under the arches"""
    body = mesh.get_editor_property("body_setup")
    if body is not None and body.get_editor_property("collision_trace_flag") != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        assets.save_loaded_asset(mesh, False)
        notes.append("complex collision on " + mesh.get_name())


def extent(mesh):
    box = mesh.get_bounding_box()
    return box.min, box.max


def rot_from_z(direction, spin):
    return unreal.MathLibrary.compose_rotators(unreal.Rotator(roll=0.0, pitch=0.0, yaw=spin), unreal.MathLibrary.make_rot_from_z(direction))


class Placer:
    """Collects instances per mesh and material and spawns one AVaelScatterField each"""

    def __init__(self):
        self.groups = {}

    def add(self, kind, mesh, material, location, rotation, scale, collide=False, shadow=True):
        key = (mesh.get_path_name(), material.get_path_name() if material else "")
        group = self.groups.setdefault(key, {"mesh": mesh, "material": material, "kind": kind, "collide": collide, "shadow": shadow, "t": []})
        scale3 = scale if isinstance(scale, unreal.Vector) else unreal.Vector(scale, scale, scale)
        group["t"].append(unreal.Transform(location, rotation, scale3))

    def spawn(self):
        total = 0
        for key, group in sorted(self.groups.items()):
            field = actors.spawn_actor_from_class(unreal.VaelScatterField, group["t"][0].translation, unreal.Rotator(0.0, 0.0, 0.0))
            field.setup_field(group["mesh"], 0.0, group["shadow"], group["collide"])
            if group["material"] is not None:
                field.instances.set_material(0, group["material"])
            field.add_scatter_instances(group["t"])
            field.set_actor_label("Mangrove_{}_{}".format(group["kind"], group["mesh"].get_name()))
            field.set_folder_path(FOLDER)
            field.set_editor_property("tags", [unreal.Name(TAG)])
            total += len(group["t"])
        return total


def main():
    data = json.load(open(os.path.join(KIT, "WF_Moor.json"), encoding="utf-8"))
    if "mangroven" not in data:
        log("WF_Moor.json has no mangroves: run the kit's wf_moor.py first")
        return
    tree = json.load(open(TREE_DATA, encoding="utf-8"))
    water_z = data["wasserspiegel_cm"]

    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    level_actors = actors.get_all_level_actors()
    removed = 0
    for actor in level_actors:
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1
    hidden = 0
    for actor in level_actors:
        if actor.get_actor_label().startswith("Mangrove_") and unreal.Name(HIDE_TAG) not in actor.tags and unreal.Name(TAG) not in actor.tags:
            actor.root_component.set_visibility(False, True)
            actor.set_actor_hidden_in_game(True)
            actor.set_actor_enable_collision(False)
            actor.set_editor_property("tags", list(actor.tags) + [unreal.Name(HIDE_TAG)])
            hidden += 1

    mangrove = unreal.load_asset(MANGROVE + "/SM_Mangrove")
    if mangrove is None:
        log("SM_Mangrove missing: import Vael_Quellen/Mangrove with Tools/Kreaturenfabrik/unreal/vael_kreatur_import.py first")
        return
    walk_through(mangrove)
    arch = unreal.load_asset(ARCH)
    if arch is not None:
        walk_through(arch)
    living, dead = build_material()
    leaves = pack_meshes(LEAVES)
    ivy = pack_meshes(IVY)
    allow_instancing(leaves + ivy)
    place = Placer()
    lo, hi = extent(mangrove)
    model_height = hi.z - lo.z
    counts = {"living": 0, "dead": 0, "leaves": 0, "ivy": 0}

    for tree_spot in data["mangroven"]:
        x, y, ground = tree_spot["ort"]
        chance = tree_spot["zufall"]
        is_dead = chance < DEAD_SHARE
        in_water = tree_spot["im_wasser"]
        scale = 0.72 + rng.random() * 0.4
        # In the water more of the root cage vanishes under the surface; dead ones have sunk furthest
        sink = rng.uniform(80, 240) if in_water else rng.uniform(30, 110)
        if is_dead:
            sink += 120
        yaw = rng.uniform(0, 360)
        lean = rng.uniform(8, 18) if is_dead else rng.uniform(0, 5)
        lean_dir = rng.uniform(0, 360)
        rotation = unreal.MathLibrary.compose_rotators(unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
                                                       unreal.MathLibrary.compose_rotators(unreal.Rotator(roll=0.0, pitch=-lean, yaw=0.0),
                                                                                           unreal.Rotator(roll=0.0, pitch=0.0, yaw=lean_dir)))
        foot = unreal.Vector(x, y, min(ground, water_z + 10.0) - sink * scale - lo.z * scale)
        place.add("Baum", mangrove, dead if is_dead else living, foot, rotation, scale, collide=True)
        counts["dead" if is_dead else "living"] += 1
        if is_dead:
            continue

        # Thin crown: one or two leaf clusters at most branch ends, ivy hanging from some
        transform = unreal.Transform(foot, rotation, unreal.Vector(scale, scale, scale))
        for end in tree["astenden"]:
            if rng.random() < 0.25:
                continue
            tip = transform.transform_location(unreal.Vector(*end["ort_cm"]))
            outward = transform.transform_direction(unreal.Vector(*end["richtung"])).normal()
            for _ in range(rng.choice((1, 1, 2))):
                if not leaves:
                    break
                size = rng.uniform(380, 620) * scale
                mesh = rng.choice(leaves)
                l_lo, l_hi = extent(mesh)
                center = tip - outward * rng.uniform(0, 180) * scale + unreal.Vector(rng.uniform(-120, 120), rng.uniform(-120, 120), rng.uniform(-150, 60))
                place.add("Laub", mesh, None, center - unreal.Vector(0, 0, size * 0.35),
                          unreal.Rotator(roll=rng.uniform(-15, 15), pitch=rng.uniform(-15, 15), yaw=rng.uniform(0, 360)),
                          size / max(l_hi.x - l_lo.x, l_hi.y - l_lo.y, l_hi.z - l_lo.z, 1.0))
                counts["leaves"] += 1
            if ivy and rng.random() < 0.6:
                mesh = rng.choice(ivy)
                l_lo, l_hi = extent(mesh)
                size = rng.uniform(300, 520) * scale
                # Hangs like a curtain under the branch end, facing outward
                flat_out = unreal.Vector(outward.x, outward.y, 0.0).normal()
                place.add("Ranken", mesh, None, tip - unreal.Vector(0, 0, size * 0.45), rot_from_z(flat_out, rng.uniform(0, 360)),
                          size / max(l_hi.x - l_lo.x, l_hi.y - l_lo.y, l_hi.z - l_lo.z, 1.0), shadow=False)
                counts["ivy"] += 1

    total = place.spawn()
    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("mangroves placed: {} living, {} dead, {} leaf clusters, {} ivy curtains ({} instances); {} grey box parts hidden, {} old pieces removed".format(
        counts["living"], counts["dead"], counts["leaves"], counts["ivy"], total, hidden, removed))
    for note in notes:
        log("NOTE " + note)


main()
