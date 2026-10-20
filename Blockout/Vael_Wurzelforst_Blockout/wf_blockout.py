# -*- coding: utf-8 -*-
"""
Vael · Region II Wurzelforst · Graubox-Blockout
================================================
Ausfuehren im Unreal Editor (Python Editor Script Plugin aktiv):
  Werkzeuge > Python-Skript ausfuehren ... > diese Datei
  oder im Ausgabelog (Python):  py "PFAD/wf_blockout.py"

Voraussetzung: Im offenen Level gibt es eine Landschaft, importiert aus
WF_Hoehenkarte_2017.png mit Position 0/0/0 und Massstab X=200, Y=200, Z=100.

Das Skript ist wiederholbar: Es loescht zuerst nur Actors mit dem Tag
"WF_Blockout" und baut dann alles neu auf.
Masse: 1 Pixel des Kartenentwurfs = 2 m. Wasserspiegel = Hoehe 0.
"""
import json
import math
import os
import zlib
import unreal

# ------------------------------------------------------------------
# EINSTELLUNGEN
# ------------------------------------------------------------------
# Euer eigenes Wassermaterial (Objektpfad), z. B. "/Game/Vael/FX/Wasser/MI_Teich".
# Leer lassen = graues Platzhalter-Wasser.
WASSER_MATERIAL = "/Game/VFX/Water/Materials/MI_Water_Pond"

# Fertige Modelle fuer die Tripo-Platzhalter (Objektpfade). Leer = Graubox-Formen.
# Ist ein Pfad eingetragen, setzt das Skript pro Baum/Pilz/... EIN Modell statt der Einzelteile:
# Fuss auf den Boden, Hoehe (bzw. Laenge beim Wurzelbogen) wie der Platzhalter, Drehung leicht zufaellig.
# Mehrere Varianten als Liste, z. B. "TRIPO_2": ["/Game/.../SM_Riesenbaum_A", "/Game/.../SM_Riesenbaum_B"].
MODELLE = {
    "TRIPO_1": "",  # Urstamm: Stamm und Wurzelbasis (1x, ersetzt Stamm + 10 Wurzeln)
    "TRIPO_2": "",  # Riesenbaum, modular (54x im Hain und Forstsaum, ersetzt Stamm + Brettwurzeln)
    "TRIPO_3": "",  # Riesenpilz (32x am Kammweg, ersetzt Stiel + Hut; Farbe spaeter ueber Material)
    "TRIPO_4": "",  # Mangrove mit Stelzwurzeln (56x im Pfuhlmoor, ersetzt Stamm + Stelzen)
    "TRIPO_5": "",  # Wurzelbogen (34x im Hain)
}
# Woran die Groesse des Modells ausgerichtet wird: "hoehe" oder "laenge" (laengste waagrechte Seite)
MODELL_MASS = {"TRIPO_1": "hoehe", "TRIPO_2": "hoehe", "TRIPO_3": "hoehe", "TRIPO_4": "hoehe", "TRIPO_5": "laenge"}
# Diese Modelle behalten die Drehung des Platzhalters (Richtung ist wichtig), alle anderen drehen sich zufaellig
MODELL_DREHUNG_BEHALTEN = ("TRIPO_5",)

# Ordner mit wf_blockout.py, WF_Platzhalter.json und T_WF_Zonenkarte.png
try:
    DATEN_ORDNER = os.path.dirname(os.path.abspath(__file__))
except NameError:
    DATEN_ORDNER = r"C:\BITTE\PFAD\EINTRAGEN\Vael_Wurzelforst_Blockout"

ASSET_ORDNER = "/Game/Vael/Wurzelforst/Blockout"
MAT_ORDNER = ASSET_ORDNER + "/Materialien"
TAG = "WF_Blockout"

# Farbe (R, G, B) linear 0..1, Leuchten (Emissive-Staerke)
PALETTE = {
    "Rinde": ((0.22, 0.15, 0.09), 0), "Urstamm": ((0.25, 0.19, 0.11), 0), "Krone": ((0.10, 0.30, 0.10), 0),
    "KroneHell": ((0.22, 0.42, 0.13), 0), "Wurzel": ((0.27, 0.29, 0.11), 0), "Moos": ((0.16, 0.34, 0.11), 0),
    "Fels": ((0.34, 0.34, 0.31), 0), "FelsMoos": ((0.24, 0.31, 0.19), 0), "Totholz": ((0.24, 0.23, 0.21), 0),
    "Mangrove": ((0.20, 0.17, 0.11), 0), "MangroveKrone": ((0.08, 0.26, 0.16), 0),
    "Pilz_Tuerkis": ((0.12, 0.60, 0.55), 0.6), "Pilz_Orange": ((0.75, 0.38, 0.12), 0), "Pilz_Rot": ((0.58, 0.10, 0.16), 0),
    "Pilzstiel": ((0.74, 0.70, 0.60), 0), "Feenlicht": ((0.40, 1.00, 0.90), 4), "Rosa": ((0.95, 0.48, 0.68), 0.6),
    "Nid": ((0.55, 0.20, 0.95), 8), "Waechter": ((0.24, 0.11, 0.34), 0.3), "Nymphe": ((0.30, 0.95, 0.85), 2),
    "Nixe": ((0.60, 0.25, 0.90), 2), "Weg": ((0.60, 0.52, 0.36), 0), "Holz": ((0.40, 0.27, 0.15), 0),
    "Zahn": ((0.85, 0.82, 0.70), 0), "Seerose": ((0.18, 0.44, 0.14), 0), "Lampe": ((1.00, 0.75, 0.35), 5),
    "Dunkel": ((0.02, 0.02, 0.02), 0), "Spieler": ((0.90, 0.20, 0.10), 0.5),
}
TRANSLUZENT = {  # Farbe, Deckkraft, Leuchten
    "WASSER": ((0.10, 0.35, 0.40), 0.6, 0.0), "Lichtschacht": ((1.00, 0.95, 0.85), 0.12, 1.5),
    "Wasserfall": ((0.80, 0.95, 1.00), 0.6, 0.8),
}

MESHES = {n: "/Engine/BasicShapes/" + n + "." + n for n in ("Cube", "Cylinder", "Sphere", "Cone", "Plane")}

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[Wurzelforst] " + str(msg))


# ------------------------------------------------------------------
# Materialien
# ------------------------------------------------------------------
def lade_oder_erzeuge(name, ordner, klasse, factory):
    pfad = ordner + "/" + name
    if eal.does_asset_exist(pfad):
        return unreal.load_asset(pfad), False
    return asset_tools.create_asset(name, ordner, klasse, factory), True


def master_material(name, transluzent):
    mat, neu = lade_oder_erzeuge(name, MAT_ORDNER, unreal.Material, unreal.MaterialFactoryNew())
    if not neu:
        return mat
    farbe = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    farbe.set_editor_property("parameter_name", "Farbe")
    farbe.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1))
    mel.connect_material_property(farbe, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 250)
    glow.set_editor_property("parameter_name", "Leuchten")
    glow.set_editor_property("default_value", 0.0)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 150)
    mel.connect_material_expressions(farbe, "", mul, "A")
    mel.connect_material_expressions(glow, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rau = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 350)
    rau.set_editor_property("r", 0.85)
    mel.connect_material_property(rau, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if transluzent:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        deck = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 450)
        deck.set_editor_property("parameter_name", "Deckkraft")
        deck.set_editor_property("default_value", 0.6)
        mel.connect_material_property(deck, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def instanz(name, parent, farbe, leuchten, deckkraft=None):
    mi, _ = lade_oder_erzeuge("MI_WF_" + name, MAT_ORDNER, unreal.MaterialInstanceConstant,
                              unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_vector_parameter_value(mi, "Farbe", unreal.LinearColor(farbe[0], farbe[1], farbe[2], 1))
    mel.set_material_instance_scalar_parameter_value(mi, "Leuchten", float(leuchten))
    if deckkraft is not None:
        mel.set_material_instance_scalar_parameter_value(mi, "Deckkraft", float(deckkraft))
    eal.save_loaded_asset(mi)
    return mi


def baue_materialien():
    m_opak = master_material("M_WF_Graubox", False)
    m_trans = master_material("M_WF_Graubox_Transluzent", True)
    mats = {k: instanz(k, m_opak, v[0], v[1]) for k, v in PALETTE.items()}
    for k, (f, d, l) in TRANSLUZENT.items():
        mats[k] = instanz(k, m_trans, f, l, d)
    if WASSER_MATERIAL:
        eigen = unreal.load_asset(WASSER_MATERIAL)
        if eigen:
            mats["WASSER"] = eigen
            log("Eigenes Wassermaterial: " + WASSER_MATERIAL)
        else:
            unreal.log_warning("[Wurzelforst] Wassermaterial nicht gefunden, nutze Platzhalter: " + WASSER_MATERIAL)
    return mats


def zonen_material(textur_aufloesung_cm):
    datei = os.path.join(DATEN_ORDNER, "T_WF_Zonenkarte.png")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", datei)
    task.set_editor_property("destination_path", ASSET_ORDNER)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    tex = unreal.load_asset(ASSET_ORDNER + "/T_WF_Zonenkarte")
    if not tex:
        unreal.log_warning("[Wurzelforst] Zonenkarte konnte nicht importiert werden: " + datei)
        return None
    mat, neu = lade_oder_erzeuge("M_WF_Zonenkarte", MAT_ORDNER, unreal.Material, unreal.MaterialFactoryNew())
    if neu:
        wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 0)
        maske = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -700, 0)
        maske.set_editor_property("r", True)
        maske.set_editor_property("g", True)
        maske.set_editor_property("b", False)
        maske.set_editor_property("a", False)
        mel.connect_material_expressions(wp, "", maske, "")
        teil = mel.create_material_expression(mat, unreal.MaterialExpressionDivide, -500, 0)
        teil.set_editor_property("const_b", float(textur_aufloesung_cm))
        mel.connect_material_expressions(maske, "", teil, "A")
        samp = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -300, 0)
        samp.set_editor_property("texture", tex)
        mel.connect_material_expressions(teil, "", samp, "UVs")
        mel.connect_material_property(samp, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        rau = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 300)
        rau.set_editor_property("r", 0.9)
        mel.connect_material_property(rau, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(mat)
        eal.save_loaded_asset(mat)
    return mat


# ------------------------------------------------------------------
# Actors
# ------------------------------------------------------------------
def aufraeumen():
    n = 0
    for a in eas.get_all_level_actors():
        if any(str(t) == TAG for t in a.tags):
            a.destroy_actor()
            n += 1
    log("Alten Blockout entfernt: %d Actors" % n)


def markiere(actor, name, ordner, extra=()):
    actor.set_actor_label(name)
    actor.set_folder_path(unreal.Name(ordner))
    actor.set_editor_property("tags", [unreal.Name(TAG)] + [unreal.Name(t) for t in extra])


VERSATZ = (0.0, 0.0, 0.0)


def vec(l):
    return unreal.Vector(float(l[0]) + VERSATZ[0], float(l[1]) + VERSATZ[1], float(l[2]) + VERSATZ[2])


def rot(r):
    return unreal.Rotator(roll=float(r[2]), pitch=float(r[0]), yaw=float(r[1]))


def spawne_mesh(mesh, l, r, s, mat, name, ordner, extra=()):
    actor = eas.spawn_actor_from_object(mesh, vec(l), rot(r))
    if not actor:
        return None
    actor.set_actor_scale3d(unreal.Vector(float(s[0]), float(s[1]), float(s[2])))
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    if comp and mat:
        comp.set_material(0, mat)
    markiere(actor, name, ordner, extra)
    return actor


def punktlicht(l, farbe, staerke, radius, name, ordner):
    a = eas.spawn_actor_from_class(unreal.PointLight, vec(l), unreal.Rotator())
    c = a.get_component_by_class(unreal.PointLightComponent)
    c.set_light_color(unreal.LinearColor(farbe[0], farbe[1], farbe[2], 1))
    setze(c, ["intensity_units"], unreal.LightUnits.UNITLESS)
    c.set_intensity(float(staerke))
    c.set_attenuation_radius(float(radius))
    try:
        c.set_editor_property("mobility", unreal.ComponentMobility.STATIONARY)
    except Exception:
        pass
    markiere(a, name, ordner)


def beschriftung(t):
    a = eas.spawn_actor_from_class(unreal.TextRenderActor, vec(t["l"]), unreal.Rotator(roll=0, pitch=0, yaw=180))
    c = a.get_component_by_class(unreal.TextRenderComponent)
    try:
        c.set_text(t["t"])
    except Exception:
        c.set_editor_property("text", t["t"])
    c.set_world_size(float(t["s"]))
    c.set_text_render_color(unreal.Color(r=t["c"][0], g=t["c"][1], b=t["c"][2], a=255))
    try:
        c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    except Exception:
        pass
    markiere(a, "Text_" + t["t"].replace(" ", "_"), "Wurzelforst/_Beschriftung")


def setze(comp, namen, wert):
    for n in namen:
        try:
            comp.set_editor_property(n, wert)
            return True
        except Exception:
            continue
    return False


def licht_und_atmosphaere(mitte):
    ordner = "Wurzelforst/_Licht_Atmosphaere"
    sonne = eas.spawn_actor_from_class(unreal.DirectionalLight, vec([mitte[0], mitte[1], 30000]),
                                       unreal.Rotator(roll=0, pitch=-38, yaw=-55))
    sc = sonne.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_intensity(6.0)
    sc.set_light_color(unreal.LinearColor(1.0, 0.93, 0.80, 1))
    setze(sc, ["atmosphere_sun_light"], True)
    markiere(sonne, "Sonne", ordner)
    himmel = eas.spawn_actor_from_class(unreal.SkyAtmosphere, vec([mitte[0], mitte[1], 0]), unreal.Rotator())
    markiere(himmel, "Himmelsatmosphaere", ordner)
    sky = eas.spawn_actor_from_class(unreal.SkyLight, vec([mitte[0], mitte[1], 20000]), unreal.Rotator())
    setze(sky.get_component_by_class(unreal.SkyLightComponent), ["real_time_capture"], True)
    markiere(sky, "Himmelslicht", ordner)
    nebel = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, vec([mitte[0], mitte[1], -500]), unreal.Rotator())
    fc = nebel.get_component_by_class(unreal.ExponentialHeightFogComponent)
    setze(fc, ["fog_density"], 0.02)
    setze(fc, ["fog_height_falloff"], 0.08)
    setze(fc, ["enable_volumetric_fog"], True)
    gruen = unreal.LinearColor(0.30, 0.45, 0.40, 1)
    setze(fc, ["fog_inscattering_luminance", "fog_inscattering_color"], gruen)
    markiere(nebel, "Bodennebel", ordner)


def landschaft_finden():
    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.LandscapeProxy):
            return a
    return None


# ------------------------------------------------------------------
# Tripo-Modelle statt Graubox-Formen (siehe MODELLE oben)
# ------------------------------------------------------------------
def modell_tag(p):
    for t in p["t"]:
        if t in MODELLE and MODELLE[t]:
            return t
    return None


def gruppen_name(name):
    # "Riesenpilz_07_Hut" -> "Riesenpilz_07", "Urstamm_Wurzel_03" -> "Urstamm", "Wurzelbogen_12" -> "Wurzelbogen_12"
    teile = name.split("_")
    if len(teile) > 1 and teile[1].isdigit():
        return "_".join(teile[:2])
    return teile[0]


def teil_box(p):
    # Eckpunkte der Grundform (1 m Wuerfel um die Mitte) in der Welt, ohne Versatz
    t = unreal.Transform(unreal.Vector(*[float(v) for v in p["l"]]), rot(p["r"]),
                         unreal.Vector(*[float(v) for v in p["s"]]))
    ecken = [t.transform_location(unreal.Vector(x, y, z)) for x in (-50, 50) for y in (-50, 50) for z in (-50, 50)]
    return ([min(e.x for e in ecken), min(e.y for e in ecken), min(e.z for e in ecken)],
            [max(e.x for e in ecken), max(e.y for e in ecken), max(e.z for e in ecken)])


def spawne_modelle(prims):
    gruppen = {}
    for p in prims:
        tag = modell_tag(p)
        if tag:
            gruppen.setdefault((tag, gruppen_name(p["n"])), []).append(p)

    meshes = {}
    n = 0
    for (tag, name), teile in sorted(gruppen.items()):
        pfade = MODELLE[tag] if isinstance(MODELLE[tag], (list, tuple)) else [MODELLE[tag]]
        pfad = pfade[zlib.crc32(name.encode("utf-8")) % len(pfade)]
        if pfad not in meshes:
            meshes[pfad] = unreal.load_asset(pfad)
            if not meshes[pfad]:
                unreal.log_warning("[Wurzelforst] Modell nicht gefunden: " + pfad)
        mesh = meshes[pfad]
        if not mesh:
            continue

        lo = [min(teil_box(p)[0][i] for p in teile) for i in range(3)]
        hi = [max(teil_box(p)[1][i] for p in teile) for i in range(3)]
        box = mesh.get_bounding_box()
        if MODELL_MASS.get(tag) == "laenge":
            soll = max(hi[0] - lo[0], hi[1] - lo[1])
            ist = max(box.max.x - box.min.x, box.max.y - box.min.y)
        else:
            soll = hi[2] - lo[2]
            ist = box.max.z - box.min.z
        s = soll / max(ist, 1.0)

        if tag in MODELL_DREHUNG_BEHALTEN:
            r = list(max(teile, key=lambda p: p["s"][0] * p["s"][1] * p["s"][2])["r"])
        else:
            r = [0, zlib.crc32(name.encode("utf-8")) % 360, 0]

        # Fuss-Mitte des Modells auf die Fuss-Mitte der Platzhalter-Teile setzen
        yaw = math.radians(r[1])
        mx = (box.min.x + box.max.x) * 0.5 * s
        my = (box.min.y + box.max.y) * 0.5 * s
        ort = [(lo[0] + hi[0]) * 0.5 - (mx * math.cos(yaw) - my * math.sin(yaw)),
               (lo[1] + hi[1]) * 0.5 - (mx * math.sin(yaw) + my * math.cos(yaw)),
               lo[2] - box.min.z * s]
        extra = [tag, "MODELL"] + ["FARBE_" + p["mat"] for p in teile if p["mat"].startswith("Pilz_")]
        if spawne_mesh(mesh, ort, r, [s, s, s], None, name, teile[0]["f"], extra):
            n += 1
    if n:
        log("%d Tripo-Modelle gesetzt" % n)
    return n


# Beispiel im Ausgabelog:  platzhalter_ersetzen("TRIPO_3", "/Game/Vael/Wurzelforst/Modelle/SM_Riesenpilz_A")
# Traegt das Modell ein und baut den Blockout neu (dauerhaft wird es erst durch den Eintrag in MODELLE oben).
def platzhalter_ersetzen(tag, mesh_pfad):
    MODELLE[tag] = mesh_pfad
    main()


# ------------------------------------------------------------------
# Hauptablauf
# ------------------------------------------------------------------
def main():
    with open(os.path.join(DATEN_ORDNER, "WF_Platzhalter.json"), "r", encoding="utf-8") as f:
        d = json.load(f)

    land = landschaft_finden()
    if not land:
        unreal.log_error("[Wurzelforst] Keine Landschaft im Level. Bitte zuerst WF_Hoehenkarte_2017.png importieren "
                         "(Position 0/0/0, Massstab 200/200/100) und das Skript dann erneut ausfuehren.")
        return

    # Lage und Massstab der Landschaft uebernehmen, falls Unreal sie beim Import verschoben hat
    global VERSATZ
    lp = land.get_actor_location()
    VERSATZ = (lp.x, lp.y, lp.z)
    sk = land.get_actor_scale3d()
    if abs(sk.x - 200) > 0.5 or abs(sk.y - 200) > 0.5 or abs(sk.z - 100) > 0.5:
        unreal.log_error("[Wurzelforst] Landschafts-Massstab ist %.0f/%.0f/%.0f, erwartet 200/200/100. "
                         "Bitte Landschaft mit 200/200/100 neu importieren." % (sk.x, sk.y, sk.z))
        return
    if any(abs(v) > 1 for v in VERSATZ):
        log("Landschaft liegt bei %.0f/%.0f/%.0f, alle Platzhalter werden mitverschoben." % VERSATZ)

    aufraeumen()
    mats = baue_materialien()
    zm = zonen_material((d["N"]) * d["cm_per_px"])
    if zm:
        land.set_editor_property("landscape_material", zm)
        log("Zonenkarte auf die Landschaft gelegt")

    meshes = {k: unreal.load_asset(v) for k, v in MESHES.items()}
    mitte = d["center"]

    # Wasser (Spiegel auf Hoehe 0)
    groesse_m = d["N"] * d["cm_per_px"] / 100.0
    spawne_mesh(meshes["Plane"], [mitte[0], mitte[1], 0], [0, 0, 0], [groesse_m, groesse_m, 1], mats["WASSER"],
                "Wasserspiegel_Ringsee_Westmeer", "Wurzelforst/_Wasser", ["WASSER"])

    licht_und_atmosphaere(mitte)

    prims = d["prims"]
    total = len(prims) + len(d["lights"]) + len(d["texts"])
    with unreal.ScopedSlowTask(total, "Wurzelforst-Blockout wird gebaut ...") as task:
        task.make_dialog(True)
        for p in prims:
            if task.should_cancel():
                break
            task.enter_progress_frame(1)
            if modell_tag(p):
                continue
            spawne_mesh(meshes[p["m"]], p["l"], p["r"], p["s"], mats.get(p["mat"]), p["n"], p["f"], p["t"])
        spawne_modelle(prims)
        for l in d["lights"]:
            task.enter_progress_frame(1)
            punktlicht(l["l"], l["c"], l["i"], l["r"], l["n"], l["f"])
        for t in d["texts"]:
            task.enter_progress_frame(1)
            beschriftung(t)

    sp = d["spawn"]
    start = eas.spawn_actor_from_class(unreal.PlayerStart, vec(sp["l"]), unreal.Rotator(roll=0, pitch=0, yaw=sp["yaw"]))
    markiere(start, "Spielerstart_Aschenpass", "Wurzelforst/1_Forstsaum/Start")

    log("Fertig: %d Platzhalter, %d Lichter, %d Beschriftungen." % (len(prims), len(d["lights"]), len(d["texts"])))
    log("Tipp: Ordner 'Kronen' im Outliner ausblenden, um unter die Baumkronen zu sehen.")


main()
