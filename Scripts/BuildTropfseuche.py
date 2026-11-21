# Die Tropfseuche (Boris und Team 21.11.2026: Optik wie echte Schleimzapfen aus Bakterien, Wirkung wie Cordyceps; danach:
# "zu lame" - dicker, in großen Flächen, Klumpen, ein leuchtendes Herz in den Rippen).
#
# In /Game/Vael/Kreaturen/Eisenschlund/Tropfseuche/:
# - M_Tropfseuche_Huelle: durchscheinender, selbstleuchtender Schleimfilm als Overlay (Schleimgänger) oder zweite Haut
#   (befallene Spieler). Fleckig: dick in Flächen, dazwischen nichts (Flecken = wie viel frei bleibt). Rinnt nach unten,
#   pulsiert, wabert.
# - M_Tropfseuche_Klumpen: dicker, nass glänzender Schleim, der von innen glüht (Klumpen, Fäden, das Seuchenherz in den
#   Rippen). Undurchsichtig, damit er wie Masse wirkt; wabert mit dem Puls.
# - SM_Tropfseuche_Klumpen_A/B/C aus Vael_Quellen/Tropfseuche/export (Tools/Nachtwesen/tropfseuche_klumpen.py): Klumpen,
#   die der Code auf Körper ohne eigene Klumpen setzt.
# Werte je Stamm setzt der Code (UVaelTropfseucheSettings): Farbe, Staerke, Deckung, Fluss, Puls, Zucken, Wabern, Flecken.
#
# Mit geschlossenem Editor:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildTropfseuche.py" -unattended -nosplash
import os

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
ORDNER = "/Game/Vael/Kreaturen/Eisenschlund/Tropfseuche"
QUELLEN = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "Vael_Quellen", "Tropfseuche", "export")


class Graph:
    def __init__(self, mat):
        self.mat = mat

    def n(self, cls, x, y, **props):
        e = mel.create_material_expression(self.mat, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, b, inp="", out=""):
        mel.connect_material_expressions(a, out, b, inp)

    def op(self, cls, x, y, a, b):
        e = self.n(cls, x, y)
        self.link(a, e, "A")
        self.link(b, e, "B")
        return e

    def mul(self, a, b, x=0, y=0):
        return self.op(unreal.MaterialExpressionMultiply, x, y, a, b)

    def add(self, a, b, x=0, y=0):
        return self.op(unreal.MaterialExpressionAdd, x, y, a, b)

    def sub(self, a, b, x=0, y=0):
        return self.op(unreal.MaterialExpressionSubtract, x, y, a, b)

    def c(self, v, x=0, y=0):
        return self.n(unreal.MaterialExpressionConstant, x, y, r=v)

    def p(self, name, wert, x, y):
        return self.n(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=wert)

    def sat(self, a, x=0, y=0):
        e = self.n(unreal.MaterialExpressionSaturate, x, y)
        self.link(a, e)
        return e

    def rauschen(self, pos, skala, x, y):
        e = self.n(unreal.MaterialExpressionNoise, x, y, scale=skala, levels=3, output_min=0.0, output_max=1.0, quality=1)
        self.link(pos, e, "Position")
        return e

    def puls(self, zeit, puls, zucken, akteur, x, y):
        """0.55..1.15: soft (sine) or jagged (sawtooth to the 6th) by Zucken; each creature in its own beat"""
        versatz = self.n(unreal.MaterialExpressionDotProduct, x, y)
        self.link(akteur, versatz, "A")
        self.link(self.n(unreal.MaterialExpressionConstant3Vector, x - 100, y + 60, constant=unreal.LinearColor(0.011, 0.017, 0.0, 1.0)), versatz, "B")
        takt = self.add(self.op(unreal.MaterialExpressionDivide, x + 100, y - 40, zeit, puls), versatz, x + 200, y)
        sinus = self.n(unreal.MaterialExpressionSine, x + 300, y, period=1.0)
        self.link(takt, sinus)
        weich = self.add(self.mul(sinus, self.c(0.5), x + 400, y), self.c(0.5), x + 500, y)
        bruch = self.n(unreal.MaterialExpressionFrac, x + 300, y + 120)
        self.link(self.mul(takt, self.c(2.0), x + 250, y + 120), bruch)
        zackig = self.n(unreal.MaterialExpressionPower, x + 400, y + 120)
        self.link(self.sub(self.c(1.0), bruch, x + 350, y + 160), zackig, "Base")
        self.link(self.c(6.0), zackig, "Exp")
        schlag = self.n(unreal.MaterialExpressionLinearInterpolate, x + 600, y + 40)
        self.link(weich, schlag, "A")
        self.link(zackig, schlag, "B")
        self.link(zucken, schlag, "Alpha")
        return self.add(self.mul(schlag, self.c(0.6), x + 700, y + 40), self.c(0.55), x + 800, y + 40)


def neues_material(name, blend, shading):
    pfad = ORDNER + "/" + name
    if eal.does_asset_exist(pfad):
        eal.delete_asset(pfad)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ORDNER, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", blend)
    mat.set_editor_property("shading_model", shading)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mat.set_editor_property("used_with_static_lighting", True)
    mat.set_editor_property("used_with_nanite", True)
    return mat, pfad


def huelle():
    mat, pfad = neues_material("M_Tropfseuche_Huelle", unreal.BlendMode.BLEND_TRANSLUCENT, unreal.MaterialShadingModel.MSM_UNLIT)
    g = Graph(mat)
    farbe = g.n(unreal.MaterialExpressionVectorParameter, -1400, -300, parameter_name="Farbe", default_value=unreal.LinearColor(0.35, 1.0, 0.2, 1.0))
    staerke, deckung, fluss = g.p("Staerke", 3.0, -1400, -200), g.p("Deckung", 0.8, -1400, -100), g.p("Fluss", 6.0, -1400, 0)
    puls, zucken, wabern = g.p("Puls", 2.6, -1400, 100), g.p("Zucken", 0.0, -1400, 200), g.p("Wabern", 1.5, -1400, 300)
    sichtbar, flecken_frei = g.p("Sichtbar", 1.0, -1400, 400), g.p("Flecken", 0.45, -1400, 500)
    zeit = g.n(unreal.MaterialExpressionTime, -1400, 600)
    welt = g.n(unreal.MaterialExpressionWorldPosition, -1400, 700)
    akteur = g.n(unreal.MaterialExpressionActorPositionWS, -1400, 800)

    # The pattern runs down: place relative to the creature, plus time * Fluss in Z
    ort = g.sub(welt, akteur, -1200, 700)
    rinnen = g.n(unreal.MaterialExpressionAppendVector, -1100, 620)
    g.link(g.n(unreal.MaterialExpressionConstant2Vector, -1200, 580, r=0.0, g=0.0), rinnen, "A")
    g.link(g.mul(zeit, fluss, -1200, 640), rinnen, "B")
    ort_fliesst = g.add(ort, rinnen, -1000, 680)
    # Patches: big blotches thick with slime, bare in between; a finer grain inside them
    grob = g.rauschen(ort_fliesst, 0.06, -850, 600)
    fein = g.rauschen(ort_fliesst, 0.3, -850, 760)
    roh = g.add(g.mul(grob, g.c(0.8), -720, 600), g.mul(fein, g.c(0.35), -720, 760), -620, 680)
    flecken = g.sat(g.mul(g.sub(roh, flecken_frei, -520, 680), g.c(3.5), -420, 680), -320, 680)
    pulsf = g.puls(zeit, puls, zucken, akteur, -1200, 900)

    fres = g.n(unreal.MaterialExpressionFresnel, -650, 300, exponent=2.0)
    kante = g.add(g.mul(fres, g.c(1.2), -550, 300), g.c(0.45), -450, 300)
    leuchten = g.mul(g.mul(g.mul(farbe, staerke, -300, -250), g.mul(kante, pulsf, -300, -120), -200, -200),
                     g.add(g.mul(flecken, g.c(1.0), -300, -20), g.c(0.08), -200, -60), -100, -200)
    mel.connect_material_property(g.mul(leuchten, sichtbar, 50, -200), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Thick where the patches are: almost opaque there, nothing in between
    deck = g.sat(g.mul(g.mul(g.mul(deckung, g.add(g.mul(fres, g.c(0.4)), g.c(0.75)), -250, 200), flecken, -150, 220), sichtbar, -80, 260), 0, 200)
    mel.connect_material_property(deck, "", unreal.MaterialProperty.MP_OPACITY)
    # The patches stand off the skin, the more the thicker
    normale = g.n(unreal.MaterialExpressionVertexNormalWS, -450, 1100)
    hub = g.mul(g.mul(normale, wabern, -350, 1060), g.add(g.mul(flecken, pulsf, -450, 1200), g.c(0.1), -350, 1160), -250, 1100)
    mel.connect_material_property(hub, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(mat)
    eal.save_asset(pfad, only_if_is_dirty=False)
    unreal.log_warning("[Tropfseuche] %s angelegt" % pfad)


def klumpen():
    mat, pfad = neues_material("M_Tropfseuche_Klumpen", unreal.BlendMode.BLEND_OPAQUE, unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(mat)
    farbe = g.n(unreal.MaterialExpressionVectorParameter, -1400, -300, parameter_name="Farbe", default_value=unreal.LinearColor(0.35, 1.0, 0.2, 1.0))
    staerke, puls, zucken = g.p("Staerke", 2.0, -1400, -200), g.p("Puls", 2.6, -1400, -100), g.p("Zucken", 0.0, -1400, 0)
    wabern, sichtbar = g.p("Wabern", 0.5, -1400, 100), g.p("Sichtbar", 1.0, -1400, 200)
    zeit = g.n(unreal.MaterialExpressionTime, -1400, 300)
    welt = g.n(unreal.MaterialExpressionWorldPosition, -1400, 400)
    akteur = g.n(unreal.MaterialExpressionActorPositionWS, -1400, 500)
    ort = g.sub(welt, akteur, -1200, 450)
    # Something moves inside: a slow noise drifting through the mass
    drift = g.n(unreal.MaterialExpressionAppendVector, -1100, 360)
    g.link(g.n(unreal.MaterialExpressionConstant2Vector, -1200, 330, r=0.0, g=0.0), drift, "A")
    g.link(g.mul(zeit, g.c(3.0), -1200, 380), drift, "B")
    innen = g.rauschen(g.add(ort, drift, -1000, 420), 0.25, -850, 420)
    pulsf = g.puls(zeit, puls, zucken, akteur, -1200, 650)
    fres = g.n(unreal.MaterialExpressionFresnel, -700, 100, exponent=3.0)
    # Dark, wet mass that glows from inside and at its rim
    mel.connect_material_property(g.mul(farbe, g.c(0.55), -300, -350), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(g.c(0.08, -300, -280), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.c(1.0, -300, -230), "", unreal.MaterialProperty.MP_SPECULAR)
    glut = g.add(g.add(g.c(0.25), g.mul(innen, g.c(0.9), -550, 380), -450, 300), g.mul(fres, g.c(0.8), -550, 150), -350, 250)
    leuchten = g.mul(g.mul(g.mul(farbe, staerke, -300, -150), g.mul(glut, pulsf, -250, 250), -150, -50), sichtbar, 0, -50)
    mel.connect_material_property(leuchten, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    normale = g.n(unreal.MaterialExpressionVertexNormalWS, -450, 900)
    hub = g.mul(g.mul(normale, wabern, -350, 900), g.mul(g.add(innen, g.c(-0.3)), pulsf, -350, 1000), -250, 950)
    mel.connect_material_property(hub, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(mat)
    eal.save_asset(pfad, only_if_is_dirty=False)
    unreal.log_warning("[Tropfseuche] %s angelegt" % pfad)
    return mat


def klumpen_modelle(mat):
    for name in ("A", "B", "C"):
        datei = os.path.join(QUELLEN, "SM_Tropfseuche_Klumpen_%s.fbx" % name)
        if not os.path.exists(datei):
            unreal.log_error("[Tropfseuche] fehlt: " + datei)
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", datei)
        task.set_editor_property("destination_path", ORDNER)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        mesh = eal.load_asset("%s/SM_Tropfseuche_Klumpen_%s" % (ORDNER, name))
        if mesh is None:
            unreal.log_error("[Tropfseuche] Import fehlgeschlagen: " + name)
            continue
        mesh.set_material(0, mat)
        eal.save_asset(mesh.get_path_name(), only_if_is_dirty=False)
        unreal.log_warning("[Tropfseuche] Klumpen %s importiert" % name)
    for pfad in eal.list_assets(ORDNER, recursive=False):
        asset = eal.load_asset(pfad)
        if isinstance(asset, (unreal.Material, unreal.MaterialInstance)) and not pfad.split(".")[-1].startswith("M_Tropfseuche"):
            eal.delete_asset(pfad)          # vom FBX-Import mitgebrachtes Material


huelle()
klumpen_modelle(klumpen())
