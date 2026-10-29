"""Step 9: the Pfuhlnatter's own body material, /Game/Vael/Kreaturen/Pfuhlnatter/M_Pfuhlnatter_Koerper, set on SK_Pfuhlnatter.

  The Tripo textures (base colour, normal, roughness, metallic) as in M_Kreatur_Master, plus:
    FlossenVerbergen  0..1: hides the fin and crown membranes (the red of the vertex colour, written by 03_skelett.py)
                      while they are drawn in, dithered so they fade; the body stays (masked, clip 0.5)
    GlutStaerke, GlutFarbe  the throat glow before a spit: only the yellow belly scales round KehleOrt (world, set by
                      AVaelPfuhlnatter below the neck top) within KehleRadius glow; before 2026-10-29 every warm
                      texel glowed and the whole neck and crown turned lime green
    Pale crown skin left over the neck (the Tripo model has no neck skin under it) is tinted towards the body's wine red
    round the same place while FlossenVerbergen is up, so the crawling neck shows no grey patch

Run with the editor closed, after 06_unreal_import.py:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>"
"""
import unreal

ZIEL = "/Game/Vael/Kreaturen/Pfuhlnatter"
NAME = "M_Pfuhlnatter_Koerper"
lib = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
notizen = []


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

    def k(self, wert, spalte):
        return self.node(unreal.MaterialExpressionConstant, spalte, r=wert)

    def skalar(self, name, wert, spalte):
        return self.node(unreal.MaterialExpressionScalarParameter, spalte, parameter_name=name, default_value=wert)

    def op(self, klasse, spalte, a, b):
        e = self.node(klasse, spalte)
        self.link(a, e, "A")
        self.link(b, e, "B")
        return e

    def mal(self, s, a, b):
        return self.op(unreal.MaterialExpressionMultiply, s, a, b)

    def minus(self, s, a, b):
        return self.op(unreal.MaterialExpressionSubtract, s, a, b)

    def sat(self, quelle, spalte):
        e = self.node(unreal.MaterialExpressionSaturate, spalte)
        self.link(quelle, e)
        return e

    def textur(self, name, spalte, sampler):
        return self.node(unreal.MaterialExpressionTextureSample, spalte, texture=eal.load_asset("{}/{}".format(ZIEL, name)), sampler_type=sampler)

    def maske(self, quelle, spalte, kanaele, ausgang=""):
        e = self.node(unreal.MaterialExpressionComponentMask, spalte, r="r" in kanaele, g="g" in kanaele, b="b" in kanaele, a="a" in kanaele)
        self.link(quelle, e, "", ausgang)
        return e


def main():
    pfad = "{}/{}".format(ZIEL, NAME)
    if eal.does_asset_exist(pfad):
        m = eal.load_asset(pfad)
        lib.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, ZIEL, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    m.set_editor_property("dither_opacity_mask", True)
    g = Graph(m)

    farbe = g.textur("T_Pfuhlnatter_basecolor", 5, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = g.textur("T_Pfuhlnatter_normal", 5, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rau = g.textur("T_Pfuhlnatter_roughness", 5, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    metall = g.textur("T_Pfuhlnatter_metallic", 5, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)

    # Fins hidden while drawn in: 1 - FlossenVerbergen * (fin share sharpened: up to 0.35 body, from 0.52 membrane), so
    # only the fade itself is dithered, never a membrane half held by the body
    vertex = g.node(unreal.MaterialExpressionVertexColor, 5)
    flosse = g.sat(g.mal(4, g.minus(4, g.maske(vertex, 5, "r"), g.k(0.35, 5)), g.k(6.0, 5)), 3)
    deckung = g.minus(2, g.k(1.0, 3), g.mal(3, flosse, g.skalar("FlossenVerbergen", 0.0, 4)))

    # Throat glow: yellow belly scales (green well above blue) near the throat glow in GlutFarbe
    rgb_roh = g.maske(farbe, 4, "rgb")
    rot = g.maske(farbe, 4, "r")
    gruen = g.maske(farbe, 4, "g")
    blau = g.maske(farbe, 4, "b")
    gelb = g.sat(g.mal(3, g.minus(3, gruen, blau), g.k(4.0, 4)), 2)
    ort = g.node(unreal.MaterialExpressionWorldPosition, 5)
    kehle = g.node(unreal.MaterialExpressionVectorParameter, 5, parameter_name="KehleOrt", default_value=unreal.LinearColor(0, 0, -100000, 0))
    abstand = g.op(unreal.MaterialExpressionDistance, 4, ort, g.maske(kehle, 4, "rgb"))
    nah = g.sat(g.minus(3, g.k(1.0, 4), g.op(unreal.MaterialExpressionDivide, 3, abstand, g.skalar("KehleRadius", 60.0, 4))), 2)
    nah_weich = g.mal(2, nah, nah)
    glut_farbe = g.node(unreal.MaterialExpressionVectorParameter, 4, parameter_name="GlutFarbe", default_value=unreal.LinearColor(0.16, 0.48, 0.06, 1))
    # Pale texels (crown petals, fin membranes, fangs): never part of the throat
    blass = g.sat(g.mal(3, g.minus(3, g.op(unreal.MaterialExpressionMin, 4, g.op(unreal.MaterialExpressionMin, 4, rot, gruen), blau), g.k(0.26, 4)), g.k(6.0, 4)), 2)
    kein_blass = g.minus(2, g.k(1.0, 3), blass)
    glut = g.mal(1, g.mal(1, g.mal(1, g.mal(2, g.mal(3, rgb_roh, g.maske(glut_farbe, 3, "rgb")), gelb), nah_weich), kein_blass), g.skalar("GlutStaerke", 0.0, 2))

    # Pale crown skin over the neck, crawling: towards the body's wine red (fangs lie ahead of the sphere and stay)
    nacken = g.sat(g.mal(2, g.minus(2, g.k(1.0, 3), g.op(unreal.MaterialExpressionDivide, 3, abstand, g.mal(3, g.skalar("KehleRadius", 60.0, 4), g.k(1.6, 4)))), g.k(2.0, 3)), 1)
    toenen = g.mal(1, g.mal(1, blass, nacken), g.mal(2, g.skalar("FlossenVerbergen", 0.0, 3), g.k(0.85, 3)))
    wein = g.node(unreal.MaterialExpressionConstant3Vector, 2, constant=unreal.LinearColor(0.11, 0.015, 0.035, 1))
    rgb = g.node(unreal.MaterialExpressionLinearInterpolate, 0)
    g.link(rgb_roh, rgb, "A")
    g.link(wein, rgb, "B")
    g.link(toenen, rgb, "Alpha")

    for ausdruck, eigenschaft, ausgang in ((rgb, unreal.MaterialProperty.MP_BASE_COLOR, ""), (normal, unreal.MaterialProperty.MP_NORMAL, "RGB"),
                                           (rau, unreal.MaterialProperty.MP_ROUGHNESS, "R"), (metall, unreal.MaterialProperty.MP_METALLIC, "R"),
                                           (glut, unreal.MaterialProperty.MP_EMISSIVE_COLOR, ""), (deckung, unreal.MaterialProperty.MP_OPACITY_MASK, "")):
        if not lib.connect_material_property(ausdruck, ausgang, eigenschaft):
            notizen.append("{} nicht verbunden".format(eigenschaft))
    lib.recompile_material(m)
    eal.save_asset(pfad, only_if_is_dirty=False)

    skm = eal.load_asset(ZIEL + "/SK_Pfuhlnatter")
    mats = list(skm.get_editor_property("materials"))
    for slot in mats:
        slot.set_editor_property("material_interface", m)
    skm.modify()
    skm.set_editor_property("materials", mats)
    eal.save_asset(skm.get_path_name(), only_if_is_dirty=False)
    unreal.log_warning("[Pfuhlnatter] {} gebaut und gesetzt{}".format(NAME, "; " + "; ".join(notizen) if notizen else ""))


main()
