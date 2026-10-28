"""Step 7: the material of the Pfuhlnatter's poison puddles, /Game/Vael/Kreaturen/Pfuhlnatter/M_Pfuhlnatter_Gift.

  On the engine plane: a round blob with a wobbly, soft-edged rim (noise on the world position, masked), glossy and wet
  (roughness 0.04), a darker rim and a faint glow from inside. Parameters set by AVaelPoisonPuddle:
    Farbe      the poison's colour (acid green; violet in a corrupted region; ice blue when frozen)
    Leuchten   glow from inside (0 frozen)
    Rauheit    roughness (higher when frozen: matte ice)
  Several blobs of the same puddle overlap into one organic shape.

Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>"
"""
import unreal

ZIEL = "/Game/Vael/Kreaturen/Pfuhlnatter"
NAME = "M_Pfuhlnatter_Gift"
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

    def plus(self, s, a, b):
        return self.op(unreal.MaterialExpressionAdd, s, a, b)

    def minus(self, s, a, b):
        return self.op(unreal.MaterialExpressionSubtract, s, a, b)

    def sat(self, quelle, spalte):
        e = self.node(unreal.MaterialExpressionSaturate, spalte)
        self.link(quelle, e)
        return e


def main():
    pfad = "{}/{}".format(ZIEL, NAME)
    if eal.does_asset_exist(pfad):
        m = eal.load_asset(pfad)
        lib.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, ZIEL, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    g = Graph(m)

    # Distance from the blob's middle: 0 in the middle, 0.5 at the plane's edge
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 6)
    mitte = g.node(unreal.MaterialExpressionConstant2Vector, 6, r=0.5, g=0.5)
    abstand = g.node(unreal.MaterialExpressionDistance, 5)
    g.link(uv, abstand, "A")
    g.link(mitte, abstand, "B")

    # A wobbly rim: noise on the world position (so neighbouring blobs share it and flow together)
    welt = g.node(unreal.MaterialExpressionWorldPosition, 6)
    rauschen = g.node(unreal.MaterialExpressionNoise, 5, scale=0.012, levels=3, output_min=-1.0, output_max=1.0,
                      noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, turbulence=False)
    g.link(welt, rauschen)
    # 1 inside, 0 outside: 0.42 - distance + noise, sharpened
    innen = g.plus(4, g.minus(4, g.k(0.42, 5), abstand), g.mal(4, rauschen, g.k(0.06, 5)))
    maske = g.sat(g.mal(3, innen, g.k(18.0, 4)), 2)

    # Darker towards the rim, glowing a little from the middle
    farbe_p = g.node(unreal.MaterialExpressionVectorParameter, 4, parameter_name="Farbe", default_value=unreal.LinearColor(0.32, 0.62, 0.04, 1))
    farbe = g.node(unreal.MaterialExpressionComponentMask, 3, r=True, g=True, b=True, a=False)
    g.link(farbe_p, farbe)
    rand = g.sat(g.mal(3, g.minus(4, g.k(0.42, 5), abstand), g.k(6.0, 4)), 2)
    dunkel = g.mal(2, farbe, g.plus(2, g.k(0.35, 3), g.mal(3, rand, g.k(0.65, 4))))
    leuchten = g.mal(1, g.mal(2, farbe, rand), g.skalar("Leuchten", 0.6, 2))

    for ausdruck, eigenschaft in ((dunkel, unreal.MaterialProperty.MP_BASE_COLOR), (leuchten, unreal.MaterialProperty.MP_EMISSIVE_COLOR),
                                  (g.skalar("Rauheit", 0.04, 1), unreal.MaterialProperty.MP_ROUGHNESS),
                                  (g.k(0.9, 1), unreal.MaterialProperty.MP_SPECULAR), (maske, unreal.MaterialProperty.MP_OPACITY_MASK)):
        if not lib.connect_material_property(ausdruck, "", eigenschaft):
            notizen.append("{} nicht verbunden".format(eigenschaft))
    lib.recompile_material(m)
    eal.save_asset(pfad, only_if_is_dirty=False)
    unreal.log_warning("[Pfuhlnatter] {} gebaut{}".format(NAME, "; " + "; ".join(notizen) if notizen else ""))


main()
