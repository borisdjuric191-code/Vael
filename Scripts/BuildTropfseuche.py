# Die Hülle der Tropfseuche (Boris und Team 21.11.2026: Optik wie echte Schleimzapfen aus Bakterien, Wirkung wie Cordyceps).
#
# /Game/Vael/Kreaturen/Eisenschlund/Tropfseuche/M_Tropfseuche_Huelle: ein durchscheinender, selbstleuchtender Schleimfilm, den
# der Code als Overlay über jedes Modell legt (UMeshComponent::SetOverlayMaterial): Schleimgänger, befallene Spieler, später
# Kobolde und Gildenleute. Er leuchtet an den Kanten stärker, fleckig wie Bakterienrasen, das Muster rinnt langsam nach unten,
# der Film wabert ein wenig und pulsiert. Jeder Stamm setzt seine Werte (UVaelTropfseucheSettings im Code):
#   Farbe, Staerke (Leuchten), Deckung (wie dicht der Film ist), Fluss (cm/s, wie schnell das Muster rinnt), Puls (Sekunden je
#   Herzschlag), Zucken (0 = weich, 1 = zackig wie der Rauschschleim), Wabern (cm).
#
# Mit geschlossenem Editor:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildTropfseuche.py" -unattended -nosplash
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
ORDNER = "/Game/Vael/Kreaturen/Eisenschlund/Tropfseuche"
NAME = "M_Tropfseuche_Huelle"
PFAD = ORDNER + "/" + NAME


def bauen():
    if eal.does_asset_exist(PFAD):
        eal.delete_asset(PFAD)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, ORDNER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mat.set_editor_property("two_sided", False)

    def n(cls, x, y, **props):
        e = mel.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, b, inp="", out=""):
        mel.connect_material_expressions(a, out, b, inp)

    def op(cls, x, y, a, b):
        e = n(cls, x, y)
        link(a, e, "A")
        link(b, e, "B")
        return e

    def c(v, x, y):
        return n(unreal.MaterialExpressionConstant, x, y, r=v)

    def p(name, wert, x, y):
        return n(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=wert)

    farbe = n(unreal.MaterialExpressionVectorParameter, -1400, -300, parameter_name="Farbe", default_value=unreal.LinearColor(0.35, 1.0, 0.2, 1.0))
    staerke = p("Staerke", 3.0, -1400, -200)
    deckung = p("Deckung", 0.55, -1400, -100)
    fluss = p("Fluss", 6.0, -1400, 0)
    puls = p("Puls", 2.6, -1400, 100)
    zucken = p("Zucken", 0.0, -1400, 200)
    wabern = p("Wabern", 0.8, -1400, 300)
    sichtbar = p("Sichtbar", 1.0, -1400, 400)

    zeit = n(unreal.MaterialExpressionTime, -1400, 500)
    welt = n(unreal.MaterialExpressionWorldPosition, -1400, 600)
    akteur = n(unreal.MaterialExpressionActorPositionWS, -1400, 700)

    # Das Muster rinnt nach unten: Ort relativ zum Akteur, plus Zeit * Fluss in Z
    ort = op(unreal.MaterialExpressionSubtract, -1200, 620, welt, akteur)
    rinnen = n(unreal.MaterialExpressionAppendVector, -1100, 520)
    null2 = n(unreal.MaterialExpressionConstant2Vector, -1200, 480, r=0.0, g=0.0)
    link(null2, rinnen, "A")
    link(op(unreal.MaterialExpressionMultiply, -1200, 540, zeit, fluss), rinnen, "B")
    ort_fliesst = op(unreal.MaterialExpressionAdd, -1000, 600, ort, rinnen)
    # Bakterienrasen: zwei Rauschen, grob und fein
    grob = n(unreal.MaterialExpressionNoise, -850, 560, scale=0.09, levels=2, output_min=0.0, output_max=1.0, quality=1)
    fein = n(unreal.MaterialExpressionNoise, -850, 700, scale=0.35, levels=2, output_min=0.0, output_max=1.0, quality=1)
    link(ort_fliesst, grob, "Position")
    link(ort_fliesst, fein, "Position")
    flecken_roh = op(unreal.MaterialExpressionAdd, -650, 620, op(unreal.MaterialExpressionMultiply, -720, 580, grob, c(0.7, -820, 520)),
                     op(unreal.MaterialExpressionMultiply, -720, 680, fein, c(0.45, -820, 780)))
    flecken = n(unreal.MaterialExpressionSaturate, -450, 620)
    link(op(unreal.MaterialExpressionMultiply, -550, 620, op(unreal.MaterialExpressionSubtract, -620, 700, flecken_roh, c(0.35, -700, 760)), c(2.2, -620, 760)), flecken)

    # Puls: weich (Sinus) oder zackig (Sägezahn hoch 6), je nach Zucken; jedes Wesen im eigenen Takt
    phase_versatz = n(unreal.MaterialExpressionDotProduct, -1100, 800)
    link(akteur, phase_versatz, "A")
    link(n(unreal.MaterialExpressionConstant3Vector, -1200, 860, constant=unreal.LinearColor(0.011, 0.017, 0.0, 1.0)), phase_versatz, "B")
    takt = op(unreal.MaterialExpressionAdd, -950, 820, op(unreal.MaterialExpressionDivide, -1050, 780, zeit, puls), phase_versatz)
    sinus = n(unreal.MaterialExpressionSine, -820, 820, period=1.0)
    link(takt, sinus)
    weich = op(unreal.MaterialExpressionAdd, -700, 820, op(unreal.MaterialExpressionMultiply, -760, 860, sinus, c(0.5, -820, 900)), c(0.5, -760, 900))
    bruch = n(unreal.MaterialExpressionFrac, -820, 960)
    link(op(unreal.MaterialExpressionMultiply, -900, 960, takt, c(2.0, -1000, 1000)), bruch)
    zackig = n(unreal.MaterialExpressionPower, -700, 960)
    link(op(unreal.MaterialExpressionSubtract, -760, 1000, c(1.0, -820, 1040), bruch), zackig, "Base")
    link(c(6.0, -760, 1060), zackig, "Exp")
    schlag = n(unreal.MaterialExpressionLinearInterpolate, -550, 880)
    link(weich, schlag, "A")
    link(zackig, schlag, "B")
    link(zucken, schlag, "Alpha")
    pulsfaktor = op(unreal.MaterialExpressionAdd, -420, 880, op(unreal.MaterialExpressionMultiply, -480, 920, schlag, c(0.6, -550, 980)), c(0.55, -480, 980))

    # Kanten leuchten stärker: der Film ist am Rand dicker
    fres = n(unreal.MaterialExpressionFresnel, -650, 300, exponent=2.2)
    kante = op(unreal.MaterialExpressionAdd, -450, 300, op(unreal.MaterialExpressionMultiply, -550, 300, fres, c(1.5, -650, 380)), c(0.25, -550, 380))

    leuchten = op(unreal.MaterialExpressionMultiply, -100, -200,
                  op(unreal.MaterialExpressionMultiply, -200, -200,
                     op(unreal.MaterialExpressionMultiply, -300, -250, farbe, staerke),
                     op(unreal.MaterialExpressionMultiply, -300, -120, kante, pulsfaktor)),
                  op(unreal.MaterialExpressionAdd, -200, -60, op(unreal.MaterialExpressionMultiply, -300, -20, flecken, c(0.8, -400, 20)), c(0.2, -300, 40)))
    mel.connect_material_property(op(unreal.MaterialExpressionMultiply, 50, -200, leuchten, sichtbar), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    deck_roh = op(unreal.MaterialExpressionMultiply, -150, 200,
                  op(unreal.MaterialExpressionMultiply, -250, 200, deckung, op(unreal.MaterialExpressionAdd, -350, 260,
                                                                               op(unreal.MaterialExpressionMultiply, -450, 220, fres, c(0.7, -550, 200)), c(0.25, -450, 280))),
                  op(unreal.MaterialExpressionAdd, -250, 320, op(unreal.MaterialExpressionMultiply, -350, 360, flecken, c(0.85, -450, 360)), c(0.15, -350, 400)))
    deck = n(unreal.MaterialExpressionSaturate, 0, 200)
    link(op(unreal.MaterialExpressionMultiply, -80, 260, deck_roh, sichtbar), deck)
    mel.connect_material_property(deck, "", unreal.MaterialProperty.MP_OPACITY)

    # Wabern: der Film hebt sich mit den Flecken und dem Puls ein wenig von der Haut
    normale = n(unreal.MaterialExpressionVertexNormalWS, -450, 1100)
    hub = op(unreal.MaterialExpressionMultiply, -250, 1100,
             op(unreal.MaterialExpressionMultiply, -350, 1060, normale, wabern),
             op(unreal.MaterialExpressionAdd, -350, 1160, op(unreal.MaterialExpressionMultiply, -450, 1200, flecken, pulsfaktor), c(0.4, -450, 1260)))
    mel.connect_material_property(hub, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    mel.recompile_material(mat)
    eal.save_asset(PFAD, only_if_is_dirty=False)
    unreal.log_warning("[Tropfseuche] %s angelegt" % PFAD)


bauen()
