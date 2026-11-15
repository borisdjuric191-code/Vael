# Leuchten und Aura der Eisenschlund-Kristalle (Lore-Bibel "Leuchten und Aura", Boris: jeder Kristall leuchtet in seiner Farbe, mit Aura,
# die langsam wie eine Lavalampe die Farbe wechselt; 15.11.2026).
#
# - /Game/Vael/Materialien/M_Kristall_Master: eine Kopie des Kreatur-Masters (der bleibt unberührt), dazu die Aura:
#   Was am Modell farbig oder hell ist (die Kristalle, nicht der graue Fels), leuchtet an den Kanten stärker als in der Mitte,
#   in einer Farbe, die langsam zwischen AuraFarbe und AuraFarbe2 wandert (AuraZyklus Sekunden, jedes Stück im eigenen Takt).
# - Die elf Material-Instanzen MI_<Erz> bekommen den neuen Master und ihre Farben. Damit leuchten die erntebaren Erze und die
#   Wandstücke der Höhlen gleich.
#
# Mit geschlossenem Editor:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildKristallAura.py" -AllowCommandletRendering -unattended -nosplash
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
QUELLE = "/Game/Vael/Materialien/M_Kreatur_Master"
ZIEL = "/Game/Vael/Materialien/M_Kristall_Master"

# Erz: (Stärke, Farbe, zweite Farbe, Sekunden je Wechsel) - die Farben nach Docs/Eisenschlund_Erze_Pflanzen_Wesen.md;
# je seltener, desto stärker die Aura (das Besondere soll man von Weitem sehen)
AURA = {
    "Rosteisen":    (0.6, (1.0, 0.45, 0.12), (1.0, 0.7, 0.3), 14.0),
    "Schwefelkies": (0.5, (1.0, 0.85, 0.3), (0.85, 1.0, 0.4), 12.0),
    "Funkenerz":    (2.0, (1.0, 0.35, 0.05), (1.0, 0.6, 0.15), 4.0),
    "Gruenspan":    (1.2, (0.1, 0.9, 0.5), (0.15, 0.4, 1.0), 10.0),
    "Salzspat":     (1.8, (1.0, 0.55, 0.65), (1.0, 0.85, 0.75), 11.0),
    "Drahtsilber":  (1.2, (0.7, 0.85, 1.0), (0.4, 0.7, 1.0), 9.0),
    "Blutzinnober": (1.3, (1.0, 0.12, 0.25), (1.0, 0.45, 0.6), 8.0),
    "Kobaltbluete": (1.3, (1.0, 0.2, 0.6), (0.6, 0.1, 0.45), 9.0),
    "Siechglas":    (2.2, (0.75, 1.0, 0.2), (0.45, 0.9, 0.1), 7.0),
    "Rippenspat":   (1.6, (1.0, 0.65, 0.3), (0.3, 0.75, 1.0), 6.0),
    "Kernerz":      (3.0, (0.3, 1.0, 0.95), (1.0, 0.7, 1.0), 5.0),
}


def master():
    import os
    if eal.does_asset_exist(ZIEL) and not os.environ.get("VAEL_AURA_NEU"):
        return eal.load_asset(ZIEL)                      # schon mit Aura
    if eal.does_asset_exist(ZIEL):
        eal.delete_asset(ZIEL)
    eal.duplicate_asset(QUELLE, ZIEL)
    mat = eal.load_asset(ZIEL)
    alt = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    farbe_tex = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)

    def n(cls, x, y, **props):
        e = mel.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, b, inp="", out=""):
        mel.connect_material_expressions(a, out, b, inp)

    def op(cls, x, y, a, b, out_a="", out_b=""):
        e = n(cls, x, y)
        link(a, e, "A", out_a)
        link(b, e, "B", out_b)
        return e

    def c(v, x, y):
        return n(unreal.MaterialExpressionConstant, x, y, r=v)

    staerke = n(unreal.MaterialExpressionScalarParameter, -900, 1200, parameter_name="AuraStaerke", default_value=0.0)
    f1 = n(unreal.MaterialExpressionVectorParameter, -900, 1300, parameter_name="AuraFarbe", default_value=unreal.LinearColor(0.3, 1.0, 0.9, 1.0))
    f2 = n(unreal.MaterialExpressionVectorParameter, -900, 1400, parameter_name="AuraFarbe2", default_value=unreal.LinearColor(1.0, 0.6, 1.0, 1.0))
    zyklus = n(unreal.MaterialExpressionScalarParameter, -900, 1500, parameter_name="AuraZyklus", default_value=8.0)
    # Lavalampe: 0,5 + 0,5 * sin(Zeit * 2pi / Zyklus + Versatz nach Ort), jedes Stück im eigenen Takt
    zeit = n(unreal.MaterialExpressionTime, -900, 1600)
    welt = n(unreal.MaterialExpressionActorPositionWS, -900, 1700)
    versatz = n(unreal.MaterialExpressionDotProduct, -760, 1700)
    link(welt, versatz, "A")
    link(n(unreal.MaterialExpressionConstant3Vector, -900, 1780, constant=unreal.LinearColor(0.0131, 0.0173, 0.0, 1.0)), versatz, "B")
    winkel = op(unreal.MaterialExpressionAdd, -560, 1600,
                op(unreal.MaterialExpressionMultiply, -660, 1560, op(unreal.MaterialExpressionDivide, -760, 1560, zeit, zyklus), c(6.2832, -760, 1620)),
                versatz)
    sinus = n(unreal.MaterialExpressionSine, -460, 1600, period=6.2832)
    link(winkel, sinus)
    welle = op(unreal.MaterialExpressionAdd, -360, 1600, op(unreal.MaterialExpressionMultiply, -420, 1650, sinus, c(0.5, -500, 1700)), c(0.5, -420, 1700))
    farbe = n(unreal.MaterialExpressionLinearInterpolate, -260, 1350)
    link(f1, farbe, "A")
    link(f2, farbe, "B")
    link(welle, farbe, "Alpha")
    # Maske: farbig (Sättigung) oder hell - die Kristalle; grauer Fels bleibt dunkel
    rgb = farbe_tex
    r = n(unreal.MaterialExpressionComponentMask, -760, 1900, r=True, g=False, b=False, a=False)
    g = n(unreal.MaterialExpressionComponentMask, -760, 1960, r=False, g=True, b=False, a=False)
    b = n(unreal.MaterialExpressionComponentMask, -760, 2020, r=False, g=False, b=True, a=False)
    for m in (r, g, b):
        link(rgb, m, "", "RGB")
    hoch = op(unreal.MaterialExpressionMax, -560, 1920, op(unreal.MaterialExpressionMax, -660, 1920, r, g), b)
    tief = op(unreal.MaterialExpressionMin, -560, 2000, op(unreal.MaterialExpressionMin, -660, 2000, r, g), b)
    saett = op(unreal.MaterialExpressionSubtract, -460, 1950, hoch, tief)
    roh = op(unreal.MaterialExpressionAdd, -260, 1950, op(unreal.MaterialExpressionMultiply, -360, 1920, saett, c(2.5, -460, 1890)),
             op(unreal.MaterialExpressionMultiply, -360, 2010, op(unreal.MaterialExpressionSubtract, -460, 2030, hoch, c(0.55, -560, 2060)), c(2.0, -460, 2090)))
    maske = n(unreal.MaterialExpressionSaturate, -160, 1950)
    link(roh, maske)
    # an den Kanten stärker: der Schein um den Kristall
    fres = n(unreal.MaterialExpressionFresnel, -460, 1450, exponent=2.0)
    kante = op(unreal.MaterialExpressionAdd, -260, 1450, op(unreal.MaterialExpressionMultiply, -360, 1450, fres, c(1.6, -460, 1520)), c(0.35, -360, 1520))
    aura = op(unreal.MaterialExpressionMultiply, 100, 1400,
              op(unreal.MaterialExpressionMultiply, 0, 1400, op(unreal.MaterialExpressionMultiply, -100, 1400, farbe, kante), maske),
              staerke)
    summe = op(unreal.MaterialExpressionAdd, 300, 1000, alt, aura) if alt is not None else aura
    mel.connect_material_property(summe, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property("used_with_static_lighting", True)
    mel.recompile_material(mat)
    eal.save_asset(ZIEL, only_if_is_dirty=False)
    unreal.log_warning("[Aura] Master mit Aura angelegt")
    return mat


m = master()
for erz, (st, a, b, z) in AURA.items():
    path = "/Game/Vael/Requisiten/Eisenschlund/{0}/MI_{0}".format(erz)
    mi = eal.load_asset(path)
    if mi is None:
        unreal.log_error("[Aura] fehlt: " + path)
        continue
    mel.set_material_instance_parent(mi, m)
    mel.set_material_instance_scalar_parameter_value(mi, "AuraStaerke", st)
    mel.set_material_instance_scalar_parameter_value(mi, "AuraZyklus", z)
    mel.set_material_instance_vector_parameter_value(mi, "AuraFarbe", unreal.LinearColor(a[0], a[1], a[2], 1.0))
    mel.set_material_instance_vector_parameter_value(mi, "AuraFarbe2", unreal.LinearColor(b[0], b[1], b[2], 1.0))
    eal.save_asset(path, only_if_is_dirty=False)
unreal.log_warning("[Aura] fertig")
