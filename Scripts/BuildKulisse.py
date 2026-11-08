# The scenery kit of the Aschenmark (goal step 9):
# 1. Imports the own pieces of Tools/Welt/aschenmark_kulisse.py (slag, glowing cracks, stakes, ribcages, cairns, graves,
#    a cart wreck) to /Game/Vael/Maps/Aschenmark/Kulisse with the Glutkammer kit materials.
# 2. M_AM_Kulisse: one master for the pack meshes, their own albedo and normal, plus what the Aschenmark does to them:
#    an ash coat on what faces up, soot, a tint, and embers glowing low down in thin cracks. Every pack mesh of the kit
#    gets its own instance (MI_AM_<mesh>) with its textures; the pack assets themselves stay untouched.
# 3. Writes Scripts/Data/Welt/AM_Kulisse.json: every piece with mesh, materials, kind, zones, scale range and how deep
#    it sinks, for the dressing script.
# 4. Builds /Game/Vael/Maps/Test/L_KulissenProbe: all pieces in rows on ash ground, for the preview sheet.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildKulisse.py" -EnablePlugins=PythonScriptPlugin -unattended -AllowCommandletRendering

import json
import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Aschenmark/export"
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
FOLDER = "/Game/Vael/Maps/Aschenmark/Kulisse"
KIT = "/Game/Vael/Refugium/Glutkammer/Probe"
PROBE = "/Game/Vael/Maps/Test/L_KulissenProbe"
M = 100.0

assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

SLOTS = {"Holz": "MI_GK_Holz", "Leder": "M_GK_Leder", "Metall": "M_GK_Metall", "Faden": "M_GK_Faden", "Glut": "M_GK_Glut",
         "Fels": "M_GK_Fels", "Erde": "M_GK_Erde", "Knochen": "M_GK_Faden", "Kohle": "M_GK_Fels", "Moos": "M_GK_Moos"}

# Own pieces: name -> (kind, zones, scale range, collision as it looks)
OWN = {
    "SM_AM_Schlacke_A": ("Schlacke", "SG", (0.8, 1.3), False),
    "SM_AM_Schlacke_B": ("Schlacke", "SG", (0.8, 1.2), False),
    "SM_AM_Schlacke_C": ("Schlacke", "SG", (0.8, 1.3), False),
    "SM_AM_Glutspalte_A": ("Glutspalte", "G", (0.8, 1.3), True),
    "SM_AM_Glutspalte_B": ("Glutspalte", "G", (0.8, 1.2), True),
    "SM_AM_Pfaehle_A": ("Pfaehle", "KSGP", (0.8, 1.2), True),
    "SM_AM_Pfaehle_B": ("Pfaehle", "KSGP", (0.8, 1.2), True),
    "SM_AM_Gerippe_A": ("Gerippe", "KSGP", (0.8, 1.4), True),
    "SM_AM_Gerippe_B": ("Gerippe", "KSGP", (0.8, 1.3), True),
    "SM_AM_Wegmarke_A": ("Wegmarke", "KSGP", (0.9, 1.1), False),
    "SM_AM_Wegmarke_B": ("Wegmarke", "KSGP", (0.9, 1.1), False),
    "SM_AM_Grabhuegel_A": ("Grab", "KP", (0.9, 1.1), True),
    "SM_AM_Grabhuegel_B": ("Grab", "KP", (0.9, 1.1), True),
    "SM_AM_Karrenwrack": ("Wrack", "KSP", (0.95, 1.05), True),
}

MS = "/Game/Megascans/3D_Assets/"
ROCK = "/Game/Rock_Collection_04/Meshes/"
# Pack pieces: mesh -> (kind, zones K Kesselgrund, S Schlackenhaenge, G Glutfelder, P Aschenpass, scale range, look (ash, soot, glow, tint))
WOOD = (0.55, 0.55, 0.0, (0.85, 0.8, 0.76))
BURNT = (0.45, 0.8, 0.25, (0.7, 0.66, 0.62))
STONE = (0.3, 0.3, 0.0, (0.75, 0.73, 0.71))
SLAG_STONE = (0.25, 0.65, 0.12, (0.6, 0.58, 0.57))
RUIN = (0.45, 0.55, 0.0, (0.75, 0.72, 0.7))
MOSS = (0.3, 0.15, 0.0, (0.8, 0.85, 0.78))
JUNK = (0.5, 0.45, 0.0, (0.82, 0.78, 0.74))
PACK = {
    MS + "FallenFirTree/SM_FallenFirTree": ("Stamm", "KGP", (0.8, 1.2), WOOD),
    MS + "FallenPineTree/SM_FallenPineTree": ("Stamm", "KGP", (0.9, 1.3), BURNT),
    MS + "FallenPineAssembly/SM_FallenPineAssembly": ("Stamm", "KG", (0.9, 1.3), WOOD),
    MS + "BrokenPineAssembly/SM_BrokenPineAssembly": ("Stamm", "KGP", (0.9, 1.2), BURNT),
    MS + "TreeLog/SM_TreeLog": ("Stamm", "KP", (0.7, 1.0), WOOD),
    MS + "MossyLog/SM_MossyLog": ("Stamm", "P", (0.8, 1.1), MOSS),
    MS + "BrokenPineStump/SM_BrokenPineStump": ("Stumpf", "KGP", (1.0, 1.6), BURNT),
    MS + "BrokenTreeStump/SM_BrokenTreeStump": ("Stumpf", "KG", (1.0, 1.6), BURNT),
    MS + "FirTreeStump/SM_FirTreeStump": ("Stumpf", "KP", (0.9, 1.4), WOOD),
    MS + "OldStump/SM_OldStump": ("Stumpf", "KP", (0.9, 1.3), WOOD),
    MS + "LichenStump/SM_LichenStump": ("Stumpf", "P", (1.0, 1.5), MOSS),
    MS + "TreeStumpB/SM_TreeStumpB": ("Stumpf", "KG", (1.0, 1.5), BURNT),
    MS + "WoodStump/SM_WoodStump": ("Stumpf", "KGP", (1.0, 1.5), BURNT),
    MS + "ForestRootsA/SM_ForestRootsA": ("Wurzel", "KGP", (1.0, 1.6), BURNT),
    MS + "ForestRootsB/SM_ForestRootsB": ("Wurzel", "KGP", (1.0, 1.6), WOOD),
    MS + "WoodBranchA/SM_WoodBranchA": ("Ast", "KSGP", (1.0, 1.6), BURNT),
    MS + "WoodBranchB/SM_WoodBranchB": ("Ast", "KSGP", (1.0, 1.4), WOOD),
    MS + "WoodBranchC/SM_WoodBranchC": ("Ast", "KSGP", (1.0, 1.5), BURNT),
    MS + "Branch/SM_Branch_00": ("Ast", "KGP", (0.9, 1.2), WOOD),
    ROCK + "Rock_01/StaticMeshes/SM_Rock_01": ("Fels", "KSGP", (0.5, 1.6), STONE),
    ROCK + "Rock_02/StaticMeshes/SM_Rock_02": ("Fels", "KSGP", (0.5, 1.6), STONE),
    ROCK + "Rock_03/StaticMeshes/SM_Rock_03": ("Fels", "SGP", (0.5, 1.8), SLAG_STONE),
    ROCK + "Rock_04/StaticMeshes/SM_Rock_04": ("Fels", "KSGP", (0.5, 1.6), STONE),
    ROCK + "Rock_05/StaticMeshes/SM_Rock_05": ("Fels", "SGP", (0.5, 1.8), SLAG_STONE),
    ROCK + "Rock_06/StaticMeshes/SM_Rock_06": ("Fels", "KSGP", (0.5, 1.6), STONE),
    ROCK + "Rock_07/StaticMeshes/SM_Rock_07": ("Fels", "SGP", (0.5, 1.8), SLAG_STONE),
    MS + "IcelandicBoulder/SM_IcelandicBoulder": ("Fels", "SGP", (0.8, 1.6), SLAG_STONE),
    MS + "GraniteRockAssembly/SM_GraniteRockAssembly": ("Fels", "KP", (0.8, 1.4), STONE),
    MS + "HovsBeachRock/SM_HovsBeachRock": ("Fels", "KSP", (0.8, 1.5), STONE),
    MS + "IcelandicMossyRock/SM_IcelandicMossyRock": ("Fels", "P", (0.9, 1.5), MOSS),
    MS + "IcelandicMossyRocks/SM_IcelandicMossyRocks": ("Fels", "P", (0.8, 1.2), MOSS),
    MS + "MossyRocksA/SM_MossyRocksA": ("Kiesel", "KP", (0.8, 1.3), STONE),
    MS + "MossyRocksB/SM_MossyRocksB": ("Kiesel", "KSP", (0.8, 1.3), STONE),
    MS + "MossyStonesPack/SM_MossyStonesPack_01": ("Kiesel", "KSGP", (1.0, 2.0), STONE),
    MS + "MossyEmbankmentB/SM_MossyEmbankmentB": ("Boeschung", "P", (0.9, 1.3), MOSS),
    MS + "MossyEmbankmentE/SM_MossyEmbankmentE": ("Boeschung", "KP", (0.9, 1.3), STONE),
    MS + "CastleWall/SM_CastleWall": ("Ruine", "KSP", (0.9, 1.2), RUIN),
    MS + "StoneWall/SM_StoneWall": ("Ruine", "KP", (0.9, 1.2), RUIN),
    MS + "MossyStoneWallA/SM_MossyStoneWallA": ("Ruine", "KP", (0.9, 1.2), RUIN),
    MS + "MossyStoneWallC/SM_MossyStoneWallC_00": ("Ruine", "KP", (0.9, 1.2), RUIN),
    MS + "MossyStoneWallC/SM_MossyStoneWallC_02": ("Ruine", "KSP", (0.9, 1.2), RUIN),
    MS + "Medieval_Modular_Wall/SM_MedievalModularWall": ("Ruine", "KP", (0.9, 1.1), RUIN),
    MS + "MedievalModularWall2x2MG/SM_MedievalModularWall2x2MG": ("Ruine", "KP", (0.9, 1.1), RUIN),
    MS + "MedievalModularCornerWall1x2M/SM_MedievalModularCornerWall1x2M": ("Ruine", "KP", (0.9, 1.1), RUIN),
    MS + "DamagedCastleStairs/SM_DamagedCastleStairs": ("Ruine", "KP", (1.5, 2.2), RUIN),
    MS + "JapaneseShrineStoneFloorB/SM_JapaneseShrineStoneFloorB": ("Platten", "KP", (0.9, 1.2), RUIN),
    MS + "GraveA/SM_GraveA_00": ("Grab", "KP", (0.9, 1.1), RUIN),
    MS + "GraveA/SM_GraveA_01": ("Grab", "KP", (0.9, 1.1), RUIN),
    MS + "GraveB/SM_GraveB": ("Grab", "KP", (0.9, 1.1), RUIN),
    MS + "WoodenBarrelB/SM_WoodenBarrelB": ("Kram", "KSGP", (0.9, 1.1), JUNK),
    MS + "WoodenBarrelC/SM_WoodenBarrelC": ("Kram", "KSGP", (0.9, 1.1), JUNK),
    MS + "WoodenBox/SM_WoodenBox": ("Kram", "KSGP", (0.9, 1.2), JUNK),
    MS + "WoodenAmmoCrate/SM_WoodenAmmoCrate": ("Kram", "KSP", (0.9, 1.1), JUNK),
    MS + "WoodenWheelA/SM_WoodenWheelA": ("Kram", "KSGP", (0.9, 1.1), JUNK),
    MS + "WoodenWheelB/SM_WoodenWheelB": ("Kram", "KSGP", (0.9, 1.1), JUNK),
    MS + "WoodenWheelbarrow/SM_WoodenWheelbarrow": ("Kram", "KS", (0.9, 1.1), JUNK),
    MS + "OldScythe/SM_OldScythe": ("Kram", "KP", (0.9, 1.1), JUNK),
    MS + "OldShovel/SM_OldShovel": ("Kram", "KSP", (0.9, 1.1), JUNK),
    MS + "PileOfChains/SM_PileOfChains": ("Kram", "SG", (0.9, 1.3), JUNK),
    MS + "OldMetalPotB/SM_OldMetalPotB": ("Kram", "KSGP", (0.9, 1.2), JUNK),
    MS + "BlacksmithAnvilA/SM_BlacksmithAnvilA": ("Kram", "S", (0.9, 1.1), JUNK),
    MS + "OldWoodenTrough/SM_OldWoodenTrough": ("Kram", "KP", (0.9, 1.1), JUNK),
    MS + "HorseHitchingPost/SM_HorseHitchingPost": ("Kram", "KP", (0.9, 1.1), JUNK),
    MS + "OldWoodenPole/SM_OldWoodenPole": ("Kram", "KSGP", (0.9, 1.1), BURNT),
    MS + "WornWoodenBeamA/SM_WornWoodenBeamA_00": ("Kram", "KSGP", (0.9, 1.1), BURNT),
    MS + "WoodenPost/SM_WoodenPost": ("Kram", "KSGP", (0.9, 1.2), BURNT),
    MS + "OldWoodenBench/SM_OldWoodenBench": ("Kram", "KP", (0.9, 1.1), JUNK),
}


def log(message):
    unreal.log_warning("[BuildKulisse] " + message)


class Graph:
    def __init__(self, material):
        self.material = material
        self.row = 0

    def node(self, cls, **props):
        self.row += 1
        e = library.create_material_expression(self.material, cls, -1100 + 300 * (self.row % 4), 110 * self.row)
        for k, val in props.items():
            e.set_editor_property(k, val)
        return e

    def link(self, a, b, b_input="", a_output=""):
        if not library.connect_material_expressions(a, a_output, b, b_input):
            log("link failed {}.{} -> {}".format(a.get_name(), a_output, b_input))

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=value)

    def scalar(self, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)

    def mask(self, source, channels, output=""):
        e = self.node(unreal.MaterialExpressionComponentMask, r="R" in channels, g="G" in channels, b="B" in channels, a="A" in channels)
        self.link(source, e, "", output)
        return e

    def op(self, cls, a, b, a_out="", b_out=""):
        e = self.node(cls)
        self.link(a, e, "A", a_out)
        self.link(b, e, "B", b_out)
        return e


SCENERY = r"""
float Up = saturate((Normal.z - 0.3) * 2.2);
float Coat = saturate(Up * Ash * 1.6 * (0.55 + 0.9 * N1) - 0.15);
float3 C = Albedo;
// Moss and fresh colours of the packs drained towards the grey of the Aschenmark
C = lerp(C, dot(C, float3(0.3, 0.59, 0.11)).xxx, Desat);
C *= Tint;
C *= lerp(1.0, 0.32, Soot * (0.45 + 0.55 * N2));
C = lerp(C, AshColour * 0.95, Coat);
// The foot of every piece runs out into the ash it stands in: no hard line where object and ground meet
float Foot = saturate(1.0 - Low / FootHeight) * (0.65 + 0.35 * N1);
Foot = Foot * Foot * (3.0 - 2.0 * Foot);
C = lerp(C, AshColour * 0.9, Foot * FootBlend);
EmberOut = Glow * pow(saturate(1.0 - N3 * 7.0), 3.0) * saturate(1.0 - Low / 45.0) * (1.0 - Coat);
return C;
"""


def usage(material):
    """The scenery stands as instanced meshes (and Nanite): without these flags the game draws the default material"""
    material.set_editor_property("used_with_instanced_static_meshes", True)
    material.set_editor_property("used_with_nanite", True)


def glow_material():
    """M_AM_Glutboden: the glowing floor of the cracks, embers flickering in patches"""
    path = FOLDER + "/M_AM_Glutboden"
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_AM_Glutboden", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    usage(material)
    g = Graph(material)
    n = g.node(unreal.MaterialExpressionNoise, scale=1.0 / 40.0, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU, levels=3,
               output_min=0.35, output_max=1.0, level_scale=2.0, quality=1, turbulence=True)
    glow = g.op(unreal.MaterialExpressionMultiply, n, g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(9.0, 2.4, 0.4, 1.0)))
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.05, 0.02, 0.01, 1.0)), "",
                                      unreal.MaterialProperty.MP_BASE_COLOR)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_master():
    path = FOLDER + "/M_AM_Kulisse"
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_AM_Kulisse", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    usage(material)
    g = Graph(material)
    default_albedo = unreal.load_asset(ROCK + "Rock_01/Textures/T_Rock_01_A")
    default_normal = unreal.load_asset(ROCK + "Rock_01/Textures/T_Rock_01_N")
    albedo = g.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name="Albedo", texture=default_albedo)
    normal = g.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name="Normal", texture=default_normal,
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    world = g.node(unreal.MaterialExpressionWorldPosition)
    ash_uv = g.node(unreal.MaterialExpressionDivide)
    g.link(g.mask(world, "RG"), ash_uv, "A")
    g.link(g.const(260.0), ash_uv, "B")
    ash = g.node(unreal.MaterialExpressionTextureSample, texture=unreal.load_asset("/Game/Vael/Welt/Boden/T_AM_Schicht0_BH"))
    g.link(ash_uv, ash, "UVs")

    def noise(scale, levels):
        e = g.node(unreal.MaterialExpressionNoise, scale=scale, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU,
                   levels=levels, output_min=0.0, output_max=1.0, level_scale=2.0, quality=1, turbulence=False)
        return e

    n1, n2 = noise(1.0 / 90.0, 3), noise(1.0 / 140.0, 2)
    n3 = g.node(unreal.MaterialExpressionNoise, scale=1.0 / 60.0, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU, levels=3,
                output_min=0.0, output_max=1.0, level_scale=2.2, quality=1, turbulence=True)
    local = g.node(unreal.MaterialExpressionLocalPosition)
    bounds = g.node(unreal.MaterialExpressionObjectLocalBounds)
    low = g.op(unreal.MaterialExpressionSubtract, g.mask(local, "B"), g.mask(bounds, "B", "Min"))
    vertex_normal = g.node(unreal.MaterialExpressionVertexNormalWS)
    tint = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="Tint", default_value=unreal.LinearColor(0.82, 0.8, 0.78, 1.0))
    inputs = {"Albedo": (albedo, "RGB"), "Normal": (vertex_normal, ""), "AshColour": (ash, "RGB"), "Ash": (g.scalar("Asche", 0.5), ""),
              "Soot": (g.scalar("Russ", 0.4), ""), "Glow": (g.scalar("Glut", 0.0), ""), "Desat": (g.scalar("Entsaettigung", 0.35), ""),
              "FootHeight": (g.scalar("Fusshoehe", 35.0), ""), "FootBlend": (g.scalar("Fussasche", 0.8), ""), "Tint": (tint, ""), "N1": (n1, ""), "N2": (n2, ""),
              "N3": (n3, ""), "Low": (low, "")}
    custom = g.node(unreal.MaterialExpressionCustom, code=SCENERY, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Aschenmark scenery")
    entries = []
    for name in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        entries.append(entry)
    custom.set_editor_property("inputs", entries)
    out = unreal.CustomOutput()
    out.set_editor_property("output_name", "EmberOut")
    out.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    custom.set_editor_property("additional_outputs", [out])
    for name, (source, output) in inputs.items():
        g.link(source, custom, name, output)
    library.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(g.scalar("Rauheit", 0.88), "", unreal.MaterialProperty.MP_ROUGHNESS)
    ember = g.node(unreal.MaterialExpressionMultiply)
    g.link(custom, ember, "A", "EmberOut")
    g.link(g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(8.0, 2.2, 0.4, 1.0)), ember, "B")
    library.connect_material_property(ember, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def pack_textures(mesh):
    """Albedo and normal texture of the first material of a pack mesh"""
    slots = mesh.get_editor_property("static_materials")
    mi = slots[0].get_editor_property("material_interface") if slots else None
    found = {}
    if isinstance(mi, unreal.MaterialInstance):
        for p in mi.get_editor_property("texture_parameter_values"):
            key = str(p.get_editor_property("parameter_info").get_editor_property("name")).upper()
            value = p.get_editor_property("parameter_value")
            if key in ("ALBEDO", "T_BASECOLOR", "BASECOLORTEXTURE", "BASE COLOR"):
                found["albedo"] = value
            elif key in ("NORMAL", "T_NORMALMAP", "NORMALTEXTURE"):
                found["normal"] = value
    return found.get("albedo"), found.get("normal"), len(slots)


def pack_instance(master, mesh_path, look):
    mesh = unreal.load_asset(mesh_path)
    if mesh is None:
        log("missing " + mesh_path)
        return None, 0
    albedo, normal, slots = pack_textures(mesh)
    name = "MI_AM_" + mesh_path.split("/")[-1].replace("SM_", "")
    path = FOLDER + "/Pack/" + name
    mi = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        name, FOLDER + "/Pack", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master)
    if albedo is not None:
        library.set_material_instance_texture_parameter_value(mi, "Albedo", albedo)
    if normal is not None:
        library.set_material_instance_texture_parameter_value(mi, "Normal", normal)
    ash, soot, glow, tint = look
    library.set_material_instance_scalar_parameter_value(mi, "Asche", ash)
    library.set_material_instance_scalar_parameter_value(mi, "Russ", soot)
    library.set_material_instance_scalar_parameter_value(mi, "Glut", glow)
    library.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint, 1.0))
    # Moss belongs to the pass only; elsewhere the green of the packs is drained
    library.set_material_instance_scalar_parameter_value(mi, "Entsaettigung", 0.15 if look is MOSS else 0.75 if look is RUIN else 0.5)
    library.update_material_instance(mi)
    assets.save_loaded_asset(mi, False)
    if albedo is None:
        log("no albedo found for " + mesh_path)
    return mi, slots


# Own pieces outdoors: plain colours through the scenery master (ash on top, soot, embers low down) instead of the cave
# materials of the kit, which look pale and smooth in daylight. slot -> (ash, soot, glow, colour)
OWN_LOOKS = {"Fels": (0.35, 0.35, 0.0, (0.11, 0.105, 0.1)), "Schlacke": (0.15, 0.3, 0.9, (0.06, 0.055, 0.05)),
             "Kohle": (0.2, 0.2, 0.35, (0.035, 0.03, 0.028)), "Holz": (0.4, 0.5, 0.0, (0.2, 0.13, 0.08)),
             "Knochen": (0.25, 0.25, 0.0, (0.55, 0.5, 0.42)), "Erde": (0.5, 0.2, 0.0, (0.13, 0.11, 0.09)),
             "Leder": (0.3, 0.35, 0.0, (0.3, 0.22, 0.14)), "Metall": (0.25, 0.5, 0.0, (0.09, 0.085, 0.08)),
             "Faden": (0.3, 0.3, 0.0, (0.42, 0.37, 0.28))}


def own_looks(master):
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    flat = unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")
    made = {}
    for slot, (ash, soot, glow, colour) in OWN_LOOKS.items():
        name = "MI_AM_Eigen_" + slot
        path = FOLDER + "/" + name
        mi = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
            name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", master)
        library.set_material_instance_texture_parameter_value(mi, "Albedo", white)
        if flat is not None:
            library.set_material_instance_texture_parameter_value(mi, "Normal", flat)
        library.set_material_instance_scalar_parameter_value(mi, "Asche", ash)
        library.set_material_instance_scalar_parameter_value(mi, "Russ", soot)
        library.set_material_instance_scalar_parameter_value(mi, "Glut", glow)
        library.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*colour, 1.0))
        library.update_material_instance(mi)
        assets.save_loaded_asset(mi, False)
        made[slot] = mi
    return made


def import_own(looks):
    materials = {slot: unreal.load_asset("{}/{}".format(KIT, name)) for slot, name in SLOTS.items()}
    materials.update({k: v for k, v in looks.items() if k != "Schlacke"})
    materials["Glut"] = glow_material()
    made = {}
    for name, (_, _, _, complex_collision) in OWN.items():
        fbx = os.path.join(SOURCE, name + ".fbx")
        if not os.path.isfile(fbx):
            log("missing " + fbx)
            continue
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
        ui.static_mesh_import_data.set_editor_property("auto_generate_collision", not complex_collision)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", fbx)
        task.set_editor_property("destination_path", FOLDER)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
        mesh = unreal.load_asset(FOLDER + "/" + name)
        if mesh is None:
            log("import failed: " + name)
            continue
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
            if slot_name == "Fels" and ("Schlacke" in name or "Glutspalte" in name):
                slot_name = "Schlacke"
                mesh.set_material(index, looks["Schlacke"])
            elif materials.get(slot_name) is not None:
                mesh.set_material(index, materials[slot_name])
        if complex_collision:
            mesh.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        assets.save_loaded_asset(mesh, False)
        made[name] = mesh
    return made


def write_kit(own, pack):
    kit = []
    for name, mesh in own.items():
        kind, zones, scale, _ = OWN[name]
        box = mesh.get_bounding_box()
        kit.append({"name": name, "mesh": FOLDER + "/" + name, "materials": [], "art": kind, "zonen": zones, "skala": scale,
                    "groesse": [round(box.max.x - box.min.x), round(box.max.y - box.min.y), round(box.max.z - box.min.z)]})
    for mesh_path, (mi, slots, kind, zones, scale, size) in pack.items():
        kit.append({"name": mesh_path.split("/")[-1], "mesh": mesh_path, "materials": [mi.get_path_name()] * max(slots, 1), "art": kind,
                    "zonen": zones, "skala": scale, "groesse": size})
    with open(os.path.join(DATA, "AM_Kulisse.json"), "w", encoding="utf-8") as f:
        json.dump({"info": "Baukasten der Aschenmark (Scripts/BuildKulisse.py); zonen: K Kesselgrund, S Schlackenhänge, G Glutfelder, P Aschenpass; "
                           "groesse in cm ohne Skalierung", "teile": kit}, f, ensure_ascii=False, indent=1)
    return kit


def build_probe(kit):
    """All pieces in rows of eight on ash ground, 9 m apart, for the preview sheet"""
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if assets.does_asset_exist(PROBE):
        assets.delete_asset(PROBE)
    levels.new_level(PROBE)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings()
    settings.set_editor_property("default_game_mode", unreal.load_class(None, "/Script/Vael.VaelGameMode"))
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(3200, 4000, -50), unreal.Rotator(0, 0, 0))
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    floor.set_actor_scale3d(unreal.Vector(120, 120, 1))
    floor.static_mesh_component.set_material(0, unreal.load_asset("/Game/Vael/Maps/Kulisse/MI_Aschenmark_Boden"))
    # Light as in L_Vael (Scripts/BuildVaelWelt.py): warm low sun, captured sky, atmosphere, haze
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), unreal.Rotator(0.0, -38.0, -40.0))
    sun.light_component.set_editor_property("intensity", 4.0)
    sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.82, 0.68, 1.0))
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1500), unreal.Rotator(0, 0, 0))
    sky.light_component.set_editor_property("real_time_capture", True)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    fog.component.set_editor_property("fog_density", 0.02)
    fog.component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.25, 0.2, 0.17, 1.0))
    actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-600, -600, 100), unreal.Rotator(0, 0, 0))
    for index, piece in enumerate(kit):
        x, y = (index // 8) * 600.0, (index % 8) * 600.0
        mesh = unreal.load_asset(piece["mesh"])
        lowest = mesh.get_bounding_box().min.z
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, -lowest - 3.0), unreal.Rotator(0, 0, 20))
        actor.static_mesh_component.set_static_mesh(mesh)
        for slot, material in enumerate(piece["materials"]):
            actor.static_mesh_component.set_material(slot, unreal.load_asset(material))
        actor.set_actor_label(piece["name"])
    unreal.EditorLoadingAndSavingUtils.save_map(world, PROBE)
    log("probe level with {} pieces, rows of 8, 6 m apart: piece i at x = (i // 8) * 6 m, y = (i % 8) * 6 m".format(len(kit)))


def main():
    master = build_master()
    own = import_own(own_looks(master))
    log("own pieces: {}".format(len(own)))
    pack = {}
    for mesh_path, (kind, zones, scale, look) in PACK.items():
        mi, slots = pack_instance(master, mesh_path, look)
        if mi is None:
            continue
        box = unreal.load_asset(mesh_path).get_bounding_box()
        pack[mesh_path] = (mi, slots, kind, zones, scale, [round(box.max.x - box.min.x), round(box.max.y - box.min.y), round(box.max.z - box.min.z)])
    log("pack instances: {}".format(len(pack)))
    kit = write_kit(own, pack)
    build_probe(kit)


if __name__ == "__main__":
    main()
