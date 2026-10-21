"""Step 8: the Rankenluchs materials with a light and a shadow form, all driven by the scalar parameter "Verderbnis" (0..1).

  M_Rankenluchs        on the Tripo textures. Verderbnis 1: the fur desaturated to a dark ash grey, the green parts
                       (moss, leaves: where green beats red and blue) black violet, fine violet glowing veins (noise
                       lines on the pre-skinned position, so they stay on the body while it moves)
  M_Rankenkamm         light: saturated green with lighter veins; shadow: black violet with deep red tips along the
                       leaf's V coordinate and violet glowing veins. Texture parameter "Blatt_Textur" (default plain
                       white) for a leaf texture set later in the editor; its alpha is the opacity mask
  M_Rankenluchs_Augen  glowing: amber (light) to violet (shadow)
  Instances: MI_Rankenluchs_Licht / _Schatten, MI_Rankenkamm_Licht / _Schatten, MI_Rankenluchs_Augen_Licht / _Schatten.
  SK_Rankenluchs gets the light instances on its slots (body, Kamm, Augen).

Everything under /Game/Vael/Kreaturen/Rankenluchs/. Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>" -EnablePlugins=PythonScriptPlugin
"""
import unreal

ZIEL = "/Game/Vael/Kreaturen/Rankenluchs"
WEISS = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"
lib = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
notizen = []


def log(text):
    unreal.log_warning("[Rankenluchs] " + text)


class Graph:
    def __init__(self, material):
        self.m = material
        self.zeilen = {}

    def node(self, klasse, spalte, **eig):
        zeile = self.zeilen.get(spalte, 0)
        self.zeilen[spalte] = zeile + 1
        e = lib.create_material_expression(self.m, klasse, -300 * spalte, 150 * zeile)
        for k, v in eig.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, b, eingang="", ausgang=""):
        if not lib.connect_material_expressions(a, ausgang, b, eingang):
            notizen.append("Verbindung fehlgeschlagen: {} -> {}.{}".format(a.get_name(), b.get_name(), eingang))

    def skalar(self, name, wert, spalte):
        return self.node(unreal.MaterialExpressionScalarParameter, spalte, parameter_name=name, default_value=wert)

    def k(self, wert, spalte):
        return self.node(unreal.MaterialExpressionConstant, spalte, r=wert)

    def farbe(self, name, rgb, spalte):
        p = self.node(unreal.MaterialExpressionVectorParameter, spalte, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))
        return self.maske(p, spalte - 1, "rgb")

    def op(self, klasse, spalte, a, b):
        e = self.node(klasse, spalte)
        self.link(a, e, "A")
        self.link(b, e, "B")
        return e

    def mal(self, s, a, b):
        return self.op(unreal.MaterialExpressionMultiply, s, a, b)

    def plus(self, s, a, b):
        return self.op(unreal.MaterialExpressionAdd, s, a, b)

    def minus(self, s, a, b):
        return self.op(unreal.MaterialExpressionSubtract, s, a, b)

    def maske(self, quelle, spalte, kanaele, ausgang=""):
        e = self.node(unreal.MaterialExpressionComponentMask, spalte, r="r" in kanaele, g="g" in kanaele, b="b" in kanaele, a="a" in kanaele)
        self.link(quelle, e, "", ausgang)
        return e

    def mix(self, a, b, alpha, spalte):
        e = self.node(unreal.MaterialExpressionLinearInterpolate, spalte)
        self.link(a, e, "A")
        self.link(b, e, "B")
        self.link(alpha, e, "Alpha")
        return e

    def sat(self, quelle, spalte):
        e = self.node(unreal.MaterialExpressionSaturate, spalte)
        self.link(quelle, e)
        return e

    def grau(self, quelle, spalte):
        e = self.node(unreal.MaterialExpressionDesaturation, spalte)
        self.link(quelle, e)
        self.link(self.k(1.0, spalte + 1), e, "Fraction")
        return e

    def textur(self, pfad, spalte, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        return self.node(unreal.MaterialExpressionTextureSample, spalte, texture=eal.load_asset(pfad), sampler_type=sampler)

    def adern(self, spalte, skala_name, skala, schaerfe):
        """Thin wavy vein lines: where gradient noise on the pre-skinned position crosses zero"""
        # Pre-skinned position exists only in the vertex shader: handed to the pixel shader by an interpolator
        roh = self.node(unreal.MaterialExpressionPreSkinnedPosition, spalte + 3)
        pos = self.node(unreal.MaterialExpressionVertexInterpolator, spalte + 2)
        self.link(roh, pos)
        rauschen = self.node(unreal.MaterialExpressionNoise, spalte + 1, scale=1.0, levels=3, output_min=-1.0, output_max=1.0,
                             noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, turbulence=False)
        self.link(self.mal(spalte + 2, pos, self.skalar(skala_name, skala, spalte + 3)), rauschen)
        betrag = self.node(unreal.MaterialExpressionAbs, spalte)
        self.link(rauschen, betrag)
        return self.sat(self.minus(spalte - 1, self.k(1.0, spalte), self.mal(spalte, betrag, self.skalar("AderSchaerfe", schaerfe, spalte + 1))), spalte - 2)


def neues_material(name, zweiseitig=False, maskiert=False):
    pfad = "{}/{}".format(ZIEL, name)
    if eal.does_asset_exist(pfad):
        m = eal.load_asset(pfad)
        lib.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ZIEL, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("two_sided", zweiseitig)
    if maskiert:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    return m


def fertig(m, ausgaenge):
    for ausdruck, eigenschaft, ausgang in ausgaenge:
        if not lib.connect_material_property(ausdruck, ausgang, eigenschaft):
            notizen.append("{}: {} nicht verbunden".format(m.get_name(), eigenschaft))
    lib.recompile_material(m)
    eal.save_asset(m.get_path_name(), only_if_is_dirty=False)
    return m


def koerper():
    m = neues_material("M_Rankenluchs")
    g = Graph(m)
    verderbnis = g.skalar("Verderbnis", 0.0, 4)
    basis = g.maske(g.textur(ZIEL + "/T_Rankenluchs_basecolor", 9), 8, "rgb")
    rm = g.textur(ZIEL + "/T_Rankenluchs_rm", 6, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    normal = g.textur(ZIEL + "/T_Rankenluchs_normal", 3, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    # Green parts: where green beats both red and blue
    r = g.maske(basis, 7, "r")
    gr = g.maske(basis, 7, "g")
    b = g.maske(basis, 7, "b")
    hoechstes = g.op(unreal.MaterialExpressionMax, 6, r, b)
    gruen = g.sat(g.mal(5, g.minus(6, gr, hoechstes), g.skalar("GruenSchwelle", 6.0, 6)), 4)
    # Shadow fur: desaturated, dark ash grey
    asche = g.mal(5, g.grau(basis, 7), g.farbe("Aschfarbe", (0.42, 0.4, 0.42), 6))
    schatten = g.mix(asche, g.farbe("Schwarzviolett", (0.035, 0.005, 0.05), 5), gruen, 3)
    farbe = g.mix(basis, schatten, verderbnis, 2)
    adern = g.adern(7, "AderDichte", 0.06, 22.0)
    leuchten = g.mal(1, g.mal(2, adern, g.farbe("Aderfarbe", (0.55, 0.08, 1.0), 3)), g.mal(2, verderbnis, g.skalar("AderLeuchten", 6.0, 3)))
    rau = g.maske(rm, 5, "g")
    return fertig(m, [(farbe, unreal.MaterialProperty.MP_BASE_COLOR, ""), (leuchten, unreal.MaterialProperty.MP_EMISSIVE_COLOR, ""),
                      (normal, unreal.MaterialProperty.MP_NORMAL, "RGB"), (rau, unreal.MaterialProperty.MP_ROUGHNESS, ""),
                      (g.k(0.0, 1), unreal.MaterialProperty.MP_METALLIC, "")])


def kamm():
    m = neues_material("M_Rankenkamm", zweiseitig=True, maskiert=True)
    g = Graph(m)
    verderbnis = g.skalar("Verderbnis", 0.0, 4)
    blatt = g.node(unreal.MaterialExpressionTextureSampleParameter2D, 8, parameter_name="Blatt_Textur", texture=eal.load_asset(WEISS))
    blatt_farbe = g.maske(blatt, 7, "rgb")
    v = g.maske(g.node(unreal.MaterialExpressionTextureCoordinate, 9), 8, "g")
    adern = g.adern(9, "AderDichte", 0.25, 14.0)
    # Light: saturated green, lighter veins
    licht = g.mix(g.farbe("Gruen", (0.05, 0.38, 0.04), 6), g.farbe("AderGruen", (0.25, 0.7, 0.15), 6), g.mal(5, adern, g.k(0.6, 6)), 4)
    # Shadow: black violet, deep red toward the tips (V 0 at the base, 1 at the tip)
    spitze = g.node(unreal.MaterialExpressionSmoothStep, 5)
    g.link(g.skalar("SpitzeAb", 0.55, 6), spitze, "Min")
    g.link(g.k(1.0, 6), spitze, "Max")
    g.link(v, spitze, "Value")
    schatten = g.mix(g.farbe("Schwarzviolett", (0.035, 0.005, 0.05), 6), g.farbe("Tiefrot", (0.32, 0.0, 0.02), 6), spitze, 4)
    farbe = g.mal(2, g.mix(licht, schatten, verderbnis, 3), blatt_farbe)
    leuchten = g.mal(1, g.mal(2, adern, g.farbe("Aderfarbe", (0.55, 0.08, 1.0), 3)), g.mal(2, verderbnis, g.skalar("AderLeuchten", 5.0, 3)))
    deckung = g.maske(blatt, 2, "r", "A")  # the sample's default output is RGB: alpha from its own output
    return fertig(m, [(farbe, unreal.MaterialProperty.MP_BASE_COLOR, ""), (leuchten, unreal.MaterialProperty.MP_EMISSIVE_COLOR, ""),
                      (g.k(0.6, 1), unreal.MaterialProperty.MP_ROUGHNESS, ""), (deckung, unreal.MaterialProperty.MP_OPACITY_MASK, "")])


def augen():
    m = neues_material("M_Rankenluchs_Augen")
    g = Graph(m)
    verderbnis = g.skalar("Verderbnis", 0.0, 4)
    farbe = g.mix(g.farbe("Bernstein", (1.0, 0.55, 0.08), 4), g.farbe("Violett", (0.55, 0.08, 1.0), 4), verderbnis, 3)
    leuchten = g.mal(2, farbe, g.skalar("Leuchten", 12.0, 3))
    return fertig(m, [(g.k(0.0, 2), unreal.MaterialProperty.MP_BASE_COLOR, ""), (leuchten, unreal.MaterialProperty.MP_EMISSIVE_COLOR, ""),
                      (g.k(0.2, 2), unreal.MaterialProperty.MP_ROUGHNESS, "")])


def instanz(name, eltern, verderbnis):
    pfad = "{}/{}".format(ZIEL, name)
    mi = eal.load_asset(pfad) if eal.does_asset_exist(pfad) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, ZIEL, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mi, eltern)
    lib.set_material_instance_scalar_parameter_value(mi, "Verderbnis", verderbnis)
    lib.update_material_instance(mi)
    eal.save_asset(pfad, only_if_is_dirty=False)
    return mi


def main():
    instanzen = {}
    for basis, eltern in (("MI_Rankenluchs", koerper()), ("MI_Rankenkamm", kamm()), ("MI_Rankenluchs_Augen", augen())):
        instanzen[basis] = (instanz(basis + "_Licht", eltern, 0.0), instanz(basis + "_Schatten", eltern, 1.0))
        log("{}: Licht und Schatten".format(basis))

    skm = eal.load_asset(ZIEL + "/SK_Rankenluchs")
    plaetze = list(skm.get_editor_property("materials"))
    for i, platz in enumerate(plaetze):
        name = str(platz.get_editor_property("material_slot_name"))
        basis = "MI_Rankenkamm" if name == "Kamm" else "MI_Rankenluchs_Augen" if name == "Augen" else "MI_Rankenluchs"
        platz.set_editor_property("material_interface", instanzen[basis][0])
        plaetze[i] = platz
        log("Platz {} -> {}_Licht".format(name, basis))
    skm.modify()
    skm.set_editor_property("materials", plaetze)
    box = skm.get_bounds().box_extent
    log("Ausdehnung (halb, cm): {:.0f} x {:.0f} x {:.0f}".format(box.x, box.y, box.z))
    eal.save_asset(skm.get_path_name(), only_if_is_dirty=False)
    for n in notizen:
        log("HINWEIS " + n)


main()
