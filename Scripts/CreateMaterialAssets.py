# Creates the crafting material data assets of Vael and keeps them complete.
#
# - A material that doesn't exist yet is created with the values below.
# - A material that exists only gets values filled in that are still at the class default.
#   Values that differ from the default are never overwritten, so tuning done in the editor stays.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/CreateMaterialAssets.py" -EnablePlugins=PythonScriptPlugin
#
# The drops come from the browser prototype; which creature drops what is set in the creature data.

import unreal

FOLDER = "/Game/Vael/Items/Materials"


def color(hex_code):
    """Linear color from an sRGB hex code like 'ff7a2e'"""
    def to_linear(channel):
        value = int(channel, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4

    return unreal.LinearColor(r=to_linear(hex_code[0:2]), g=to_linear(hex_code[2:4]), b=to_linear(hex_code[4:6]), a=1.0)


MATERIALS = [
    {
        "asset": "DA_Material_Glutdruese",
        "display_name": unreal.Text("Glutdrüse"),
        "description": unreal.Text("Aus einem Glutkriecher. Glimmt noch."),
        "region": unreal.Text("Aschenmark"),
        "color": color("ff8a3d"),
    },
    {
        "asset": "DA_Material_Russfeder",
        "display_name": unreal.Text("Rußfeder"),
        "description": unreal.Text("Feder einer Aschharpyie."),
        "region": unreal.Text("Aschenmark"),
        "color": color("8a817a"),
    },
    {
        "asset": "DA_Material_Rotnoppe",
        "display_name": unreal.Text("Rotnoppe"),
        "description": unreal.Text("Saugnoppe vom Bauch einer Draculaupe. Haftet an allem, auch an den Fingern."),
        "region": unreal.Text("Wurzelforst"),
        "color": color("c2262e"),
    },
    {
        "asset": "DA_Material_Kokonseide",
        "display_name": unreal.Text("Kokonseide"),
        "description": unreal.Text("Faden aus einem zerstörten Kokon. Zäh, leicht angesengt, stellenweise rot."),
        "region": unreal.Text("Wurzelforst"),
        "color": color("d9d2c2"),
    },
    {
        "asset": "DA_Material_Blutschuppen",
        "display_name": unreal.Text("Blutschuppen"),
        "description": unreal.Text("Roter Flügelstaub eines Nosferalters. Färbt ab und geht nie wieder heraus."),
        "region": unreal.Text("Wurzelforst"),
        "color": color("8f1620"),
    },
    {
        "asset": "DA_Material_Kelchkapsel",
        "display_name": unreal.Text("Kelchkapsel"),
        "description": unreal.Text("Samenkapsel eines Erntekelchs. Er wächst nur dort, wo der Regen eines Falters die Erde getränkt hat."),
        "region": unreal.Text("Wurzelforst"),
        "color": color("6fc3e0"),
    },
    {
        "asset": "DA_Material_Aeltestenschwinge",
        "display_name": unreal.Text("Ältestenschwinge"),
        "description": unreal.Text("Schwinge der Harpyien-Ältesten."),
        "region": unreal.Text("Aschenmark"),
        "color": color("d6c8af"),
    },
    {
        "asset": "DA_Material_Ordenssiegel",
        "display_name": unreal.Text("Ordenssiegel"),
        "description": unreal.Text("Siegel eines Predigers der Narbe."),
        "region": unreal.Text("Aschenmark"),
        "color": color("b3405f"),
    },
    {
        "asset": "DA_Material_HerzDerGlut",
        "display_name": unreal.Text("Herz der Glut"),
        "description": unreal.Text("Das Herz der Glutkönigin. Es glüht noch in der Asche."),
        "region": unreal.Text("Aschenmark"),
        "color": color("ff6a1a"),
    },
    {
        "asset": "DA_Material_Markkristall",
        "display_name": unreal.Text("Markkristall"),
        "description": unreal.Text("Verdichtetes Mark aus einem gezeichneten Wesen. Opfergabe an Mark-Quellen."),
        "region": unreal.Text("Mark"),
        "color": color("a24dff"),
    },
    # Sold by the Gildenkontor; every player starts with two
    {
        "asset": "DA_Material_Heiltrank",
        "display_name": unreal.Text("Heiltrank"),
        "description": unreal.Text("Kelchtau, abgekocht und mit Gildensiegel verkorkt. Heilt vier Zehntel des Lebens. Für den Notfall, nicht für jeden Kratzer."),
        "region": unreal.Text("Vorrat"),
        "color": color("d0443a"),
        "heal_fraction": 0.4,
    },
    # Creatures of the creature factory
    {
        "asset": "DA_Material_Spannfaden",
        "display_name": unreal.Text("Spannfaden"),
        "description": unreal.Text("Der Faden zwischen den Hörnern eines Spannhornkäfers. Zäher als jede Sehne; Bogensehne für den Ingenieur."),
        "region": unreal.Text("Wurzelforst"),
        "color": color("d8cfb4"),
    },
    # Bestiarium: Schleieraffe (Boris, 12. Oktober 2026: Sammler für den Tamer, Spürbrille und Beutel für den Ingenieur)
    {
        "asset": "DA_Material_Schleierpelz",
        "display_name": unreal.Text("Schleierpelz"),
        "description": unreal.Text("Rußschwarzes Fell voller eingenähter Taschen: Der Schleieraffe trägt darin, was er findet. Der Ingenieur näht daraus Futter für einen größeren Beutel."),
        "region": unreal.Text("überall, vor allem im Hain"),
        "color": color("2b2a2e"),
    },
    {
        "asset": "DA_Material_Glimmauge",
        "display_name": unreal.Text("Glimmauge"),
        "description": unreal.Text("Die Linse aus dem Auge eines Schleieraffen. Sie sammelt das letzte Licht und zeigt, was im Dunkeln liegt. Der Ingenieur schleift daraus eine Spürbrille, die Materialien in der Umgebung sichtbar macht."),
        "region": unreal.Text("überall, vor allem im Hain"),
        "color": color("f2e68c"),
    },
    # Bestiarium row Sichelpuppe
    {
        "asset": "DA_Material_Glimmstroh",
        "display_name": unreal.Text("Glimmstroh"),
        "description": unreal.Text("Verkohltes Stroh aus einer Sichelpuppe. Es glimmt weiter, ohne zu verbrennen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("8a5a2e"),
    },
    {
        "asset": "DA_Material_Sichelklinge",
        "display_name": unreal.Text("Sichelklinge"),
        "description": unreal.Text("Die Klinge einer Erntesichel, schartig und ruÃschwarz, aber scharf. Der Ingenieur baut daraus seinen Sichelkreisel."),
        "region": unreal.Text("Aschenmark"),
        "color": color("9aa0a8"),
    },
    # Bestiarium row Aschenroller
    {
        "asset": "DA_Material_Rollpanzer",
        "display_name": unreal.Text("Rollpanzer"),
        "description": unreal.Text("Ein Band aus dem Panzer eines Aschenrollers, hart gebrannt von der Glut. Der Ingenieur baut daraus seinen Rammschild."),
        "region": unreal.Text("Aschenmark"),
        "color": color("6b5a4a"),
    },
    # Plants, fungi and stones of the Aschenmark (Kompendium tab, "Arten der Regionen")
    {
        "asset": "DA_Material_Glutsamen",
        "display_name": unreal.Text("Glutsamen"),
        "description": unreal.Text("Samen der Glutdistel. Warm in der Hand, heiß in der Tasche."),
        "region": unreal.Text("Aschenmark"),
        "color": color("ff7a2e"),
    },
    {
        "asset": "DA_Material_Kelchtau",
        "display_name": unreal.Text("Kelchtau"),
        "description": unreal.Text("Leuchtende Tropfen aus dem Tränenkelch. Grundzutat für Heiltränke."),
        "region": unreal.Text("Aschenmark"),
        "color": color("bfe3ff"),
    },
    {
        "asset": "DA_Material_Rauchblase",
        "display_name": unreal.Text("Rauchstaub"),
        "description": unreal.Text("Eine heile Blase der Aschblase, voll mit eingeschlossenem Rauch. Nicht drücken."),
        "region": unreal.Text("Aschenmark"),
        "color": color("9aa3a8"),
    },
    {
        "asset": "DA_Material_Glockenglut",
        "display_name": unreal.Text("Glockenglut"),
        "description": unreal.Text("Die glimmende Mitte einer Laternenglocke. Leuchtet heller, wo das Land gesund ist."),
        "region": unreal.Text("Aschenmark"),
        "color": color("ffb259"),
    },
    {
        "asset": "DA_Material_Russmoos",
        "display_name": unreal.Text("Keimpelzflocken"),
        "description": unreal.Text("Grüner Moospelz, der mit dem Regen aus der Asche kommt. Futter, mit dem Tiere rein wachsen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("5c6648"),
    },
    {
        "asset": "DA_Material_Ueberraschungssaat",
        "display_name": unreal.Text("Keimsaat"),
        "description": unreal.Text("Samenkapseln vom Keimpelz. Niemand weiß, was daraus wird, bevor Albrun sie im Topf gezogen hat."),
        "region": unreal.Text("Aschenmark"),
        "color": color("9be36a"),
    },
    {
        "asset": "DA_Material_Samen_Glimmklee",
        "display_name": unreal.Text("Glimmklee-Samen"),
        "description": unreal.Text("Dreilappig, bernsteinfarben, mit glimmendem Rand. Von Albrun im Topf gezogen und erforscht; lässt sich säen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("ffa12a"),
    },
    {
        "asset": "DA_Material_Samen_Russrose",
        "display_name": unreal.Text("Rußrose-Samen"),
        "description": unreal.Text("Schwarz und samtig, mit einem Riss, durch den es grün leuchtet. Von Albrun im Topf gezogen und erforscht; lässt sich säen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("35e07a"),
    },
    {
        "asset": "DA_Material_Samen_Taublatt",
        "display_name": unreal.Text("Taublatt-Samen"),
        "description": unreal.Text("Ein Schälchen, in dem ein Wassertropfen steht. Von Albrun im Topf gezogen und erforscht; lässt sich säen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("4fd0ff"),
    },
    {
        "asset": "DA_Material_Samen_Milchstern",
        "display_name": unreal.Text("Milchstern-Samen"),
        "description": unreal.Text("Ein perlweißer, fünfzackiger Stern. Von Albrun im Topf gezogen und erforscht; lässt sich säen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("dcd2ff"),
    },
    {
        "asset": "DA_Material_Samen_Funkenmohn",
        "display_name": unreal.Text("Funkenmohn-Samen"),
        "description": unreal.Text("Eine orangerote Kapsel mit Krönchen. Sie knistert. Von Albrun im Topf gezogen und erforscht; lässt sich säen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("ff5a1f"),
    },
    {
        "asset": "DA_Material_Samen_Windhalm",
        "display_name": unreal.Text("Windhalm-Samen"),
        "description": unreal.Text("Schlank und silbern, mit einer Granne wie ein kleiner Pfeil. Von Albrun im Topf gezogen und erforscht; lässt sich säen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("7ef0ff"),
    },
    {
        "asset": "DA_Material_Samen_Schlotwinde",
        "display_name": unreal.Text("Schlotwinde-Samen"),
        "description": unreal.Text("Eine gelbgrüne Spirale mit einer Ranke. Von Albrun im Topf gezogen und erforscht; lässt sich säen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("ffd23a"),
    },
    {
        "asset": "DA_Material_Samen_Hainschoessling",
        "display_name": unreal.Text("Hainschössling-Samen"),
        "description": unreal.Text("Wie eine Eichel aus lebendem Holz, aus der ein leuchtendes Blatt treibt. Von Albrun im Topf gezogen und erforscht; lässt sich säen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("ffe27a"),
    },
    {
        "asset": "DA_Material_Kelchfleisch",
        "display_name": unreal.Text("Kelchfleisch"),
        "description": unreal.Text("Ein Stück vom Fleischkelch, durchzogen von violetten Adern. Mark-Zutat."),
        "region": unreal.Text("Aschenmark"),
        "color": color("b0506a"),
    },
    {
        "asset": "DA_Material_Pfeifensporen",
        "display_name": unreal.Text("Pfeifenmorchel-Sporen"),
        "description": unreal.Text("Rostroter Staub aus den Röhren der Pfeifenmorchel."),
        "region": unreal.Text("Aschenmark"),
        "color": color("b0442a"),
    },
    {
        "asset": "DA_Material_Staeublingspulver",
        "display_name": unreal.Text("Aschenstäubling"),
        "description": unreal.Text("Graues Pulver des Aschenstäublings. Erstickt Flammen; Grundzutat für den Löschtrank."),
        "region": unreal.Text("Aschenmark"),
        "color": color("a49c92"),
    },
    {
        "asset": "DA_Material_Myzelprobe",
        "display_name": unreal.Text("Adernflechte"),
        "description": unreal.Text("Feine weiße Fäden der Adernflechte. Färben sich violett, wenn Mark in der Nähe ist."),
        "region": unreal.Text("Aschenmark"),
        "color": color("eae4d6"),
    },
    {
        "asset": "DA_Material_Schlacke",
        "display_name": unreal.Text("Schlacke"),
        "description": unreal.Text("Schwarzer, glasiger Abfall alter Schmelzen. Grundmaterial für den Ingenieur."),
        "region": unreal.Text("Aschenmark"),
        "color": color("4a4650"),
    },
    {
        "asset": "DA_Material_Glutstein",
        "display_name": unreal.Text("Glutstein"),
        "description": unreal.Text("Ein Brocken, der Hitze speichert. Grundlage für Wärmetrank und Feuermunition."),
        "region": unreal.Text("Aschenmark"),
        "color": color("ff6a1a"),
    },
    {
        "asset": "DA_Material_Russquarz",
        "display_name": unreal.Text("Rußquarz"),
        "description": unreal.Text("Rauchgrauer Kristall vom Kraterrand. Klar geschliffen eine gute Linse."),
        "region": unreal.Text("Aschenmark"),
        "color": color("8c8a90"),
    },
]


# Ingredients of the witch's kitchen (Lore-Bibel, tabs Herbarium "Ernte" and Rezeptbuch): kind, element and way.
# New ones are created; on the materials above, only the ingredient fields still at the class default are filled in.
K = unreal.VaelIngredientKind
E = unreal.VaelElement
W = unreal.VaelWay

INGREDIENTS = [
    # asset name, shown name, what it does, color, kind, element (None = none), way
    # Seed plants, light forms
    ("Glimmblatt", "Glimmblatt", "Blatt des Glimmklees, die Ränder glimmen warm. Wärmt und schützt vor Feuer.", "ffa040", K.LEAF, E.FIRE, W.LIGHT),
    ("Glimmstaub", "Glimmstaub", "Goldener Staub vom Glimmklee. Für langsame Regeneration.", "e8c060", K.DUST, E.EARTH, W.LIGHT),
    ("Blatttau", "Blatttau", "Klares Wasser aus den Schalen des Taublatts. Basis der Licht-Tränke.", "8fd8ff", K.DEW, E.WATER, W.LIGHT),
    ("Silberhalm", "Silberhalm", "Zähe Silberfaser des Windhalms. Für Schleuderbänder und Befiederung.", "cfe0e8", K.FIBER, E.AIR, W.LIGHT),
    ("Halmsamen", "Halmsamen", "Leichte Samen des Windhalms, die der Wind trägt. Machen schnell.", "d8eef0", K.SEED, E.AIR, W.LIGHT),
    ("Trichterpollen", "Trichterpollen", "Gelber Pollen aus den Trichtern der Schlotwinde. Verstärkt Schutz.", "ffd84a", K.POLLEN, E.EARTH, W.LIGHT),
    ("Rankenfaser", "Rankenfaser", "Ranke der Schlotwinde. Für Netze und Bindungen.", "7fae4a", K.FIBER, E.EARTH, W.LIGHT),
    ("Russbluetenblatt", "Rußblütenblatt", "Samtig schwarzes Blütenblatt der Rußrose. Heilt.", "3a3a40", K.PETAL, E.WATER, W.LIGHT),
    ("RosenherzNektar", "Rosenherz-Nektar", "Nektar aus dem hellgrünen Herz der Rußrose. Verstärkt Heilung.", "9cf0a0", K.NECTAR, E.WATER, W.LIGHT),
    ("Sternmilch", "Sternmilch", "Heller Saft des Milchsterns. Basis der Statustränke; der Ingenieur macht Latex daraus.", "f4f2ff", K.SAP, E.WATER, W.LIGHT),
    ("Sternpollen", "Sternpollen", "Glitzernder Pollen des Milchsterns. Verlängert Wirkungen.", "d8d0ff", K.GLITTER, E.AIR, W.LIGHT),
    ("Knistersamen", "Knistersamen", "Samen des Funkenmohns, sie knistern leise. Zünder und Brandmunition.", "ff7a2e", K.SEED, E.FIRE, W.LIGHT),
    ("Mohnbluetenblatt", "Mohnblütenblatt", "Orangerotes Blütenblatt des Funkenmohns. Schützt vor Feuer.", "ff5a2a", K.PETAL, E.FIRE, W.LIGHT),
    ("Hainblatt", "Hainblatt", "Das eine leuchtende Blatt des Hainschösslings. Reinigt und trifft Markwesen hart.", "ffe27a", K.LEAF, E.EARTH, W.LIGHT),
    ("Lichttropfen", "Lichttropfen", "Ein Tropfen reines Licht vom Hainschössling. Der stärkste Licht-Verstärker.", "fff4c0", K.GLITTER, E.WATER, W.LIGHT),
    ("Traenenglitzer", "Tränenglitzer", "Glitzer aus den Tropfen des Tränenkelchs. Heilt die Gruppe und reinigt.", "d0f0ff", K.GLITTER, E.WATER, W.LIGHT),
    # Seed plants, shadow forms
    ("Schwelblatt", "Schwelblatt", "Violett schwelendes Blatt des Schwelklees. Zehrt und erschöpft Gegner.", "7a3a8c", K.LEAF, E.FIRE, W.SHADOW),
    ("Schwelstaub", "Schwelstaub", "Staub des Schwelklees. Ein Schwelbrand, der lange brennt.", "8c3a5a", K.DUST, E.FIRE, W.SHADOW),
    ("Faultau", "Faultau", "Trübes, violettes Wasser aus dem Faulblatt. Basis der Gifte.", "6a4a8a", K.DEW, E.WATER, W.SHADOW),
    ("Fluesterfaser", "Flüsterfaser", "Faser des Flüsterhalms. Munition daraus verwirrt.", "8a7aa0", K.FIBER, E.AIR, W.SHADOW),
    ("Raunsamen", "Raunsamen", "Samen des Flüsterhalms, die leise raunen. Verwirren.", "6a5a80", K.SEED, E.AIR, W.SHADOW),
    ("Wuergranke", "Würgranke", "Dornige violette Ranke der Würgwinde. Fesselt.", "5a2a5a", K.FIBER, E.EARTH, W.SHADOW),
    ("Dornpollen", "Dornpollen", "Spitzer Pollen der Würgwinde. Lässt bluten.", "a02a4a", K.POLLEN, E.EARTH, W.SHADOW),
    ("Blutblatt", "Blutblatt", "Dunkelrotes Blütenblatt der Blutrose. Raubt Leben.", "8a1020", K.PETAL, E.WATER, W.SHADOW),
    ("Blutnektar", "Blutnektar", "Nektar der Blutrose. Verstärkt Schattenwirkungen.", "b01830", K.NECTAR, E.WATER, W.SHADOW),
    ("Gallmilch", "Gallmilch", "Bitterer, dunkler Saft des Gallsterns. Basis der Flüche; wird zu schwarzem Latex.", "3a2a40", K.SAP, E.WATER, W.SHADOW),
    ("Gallpollen", "Gallpollen", "Schwarz glitzernder Pollen des Gallsterns. Verlängert Schattenwirkungen.", "5a3a6a", K.GLITTER, E.AIR, W.SHADOW),
    ("Schwarzfunken", "Schwarzfunken", "Samen des Schwarzmohns mit violetten Funken. Ihr Feuer lässt sich kaum löschen.", "6a2a8a", K.SEED, E.FIRE, W.SHADOW),
    ("Schwarzmohnblatt", "Schwarzmohnblatt", "Schwarzes Blütenblatt des Schwarzmohns. Für die Brandphiole.", "2a1a2a", K.PETAL, E.FIRE, W.SHADOW),
    ("Narbenholz", "Narbenholz", "Verdrehtes, violett geadertes Holz des Narbentriebs. Ein späterer Waffenkern.", "4a2a4a", K.FIBER, E.EARTH, W.SHADOW),
    ("Narbensaft", "Narbensaft", "Saft des Narbentriebs. Der stärkste Schatten-Verstärker.", "7a1a4a", K.SAP, E.EARTH, W.SHADOW),
    ("Kelchschleim", "Kelchschleim", "Schleim aus dem Fleischkelch. Für Gift und Fluch.", "9a4a7a", K.SAP, E.EARTH, W.SHADOW),
    ("Verdauungssaeure", "Verdauungssäure", "Säure aus dem Magen des Fleischkelchs. Zerfrisst Rüstung.", "a0c040", K.SAP, E.EARTH, W.SHADOW),
    # Wild plants
    ("Distelflaum", "Distelflaum", "Flaum der Glutdistel, fängt sofort Feuer. Für Brandmunition.", "e0a080", K.FIBER, E.FIRE, W.NEUTRAL),
    ("Pelzwasser", "Pelzwasser", "Regenwasser, aus dem Keimpelz gedrückt. Basis der Basistränke für alle.", "a0d0b0", K.DEW, E.WATER, W.NEUTRAL),
]

# Materials that exist already and are ingredients too: kind, element (None = none), way
EXISTING_INGREDIENTS = {
    "DA_Material_Glutsamen": (K.SEED, E.FIRE, W.NEUTRAL),
    "DA_Material_Glockenglut": (K.EMBER, E.FIRE, W.NEUTRAL),
    "DA_Material_Rauchblase": (K.DUST, E.AIR, W.NEUTRAL),
    "DA_Material_Russmoos": (K.FIBER, E.EARTH, W.NEUTRAL),
    "DA_Material_Ueberraschungssaat": (K.SEED, None, W.NEUTRAL),
    "DA_Material_Kelchtau": (K.DEW, E.WATER, W.LIGHT),
    "DA_Material_Kelchkapsel": (K.SEED, E.WATER, W.NEUTRAL),
    "DA_Material_Pfeifensporen": (K.CATALYST, E.AIR, W.NEUTRAL),
    "DA_Material_Staeublingspulver": (K.CATALYST, E.EARTH, W.NEUTRAL),
    "DA_Material_Myzelprobe": (K.CATALYST, E.EARTH, W.NEUTRAL),
    "DA_Material_Schlacke": (K.PART, E.EARTH, W.NEUTRAL),
    "DA_Material_Glutstein": (K.PART, E.FIRE, W.NEUTRAL),
    "DA_Material_Russquarz": (K.PART, E.EARTH, W.NEUTRAL),
    "DA_Material_Markkristall": (K.PART, E.MARK, W.SHADOW),
    "DA_Material_Glutdruese": (K.PART, E.FIRE, W.NEUTRAL),
    "DA_Material_Russfeder": (K.PART, E.AIR, W.NEUTRAL),
    "DA_Material_Aeltestenschwinge": (K.PART, E.AIR, W.NEUTRAL),
}


def ingredient_fields(kind, element, way):
    fields = {"kind": kind, "way": way}
    if element is not None:
        fields["has_element"] = True
        fields["element"] = element
    return fields


for definition in MATERIALS:
    if definition["asset"] in EXISTING_INGREDIENTS:
        definition.update(ingredient_fields(*EXISTING_INGREDIENTS.pop(definition["asset"])))

for missing in EXISTING_INGREDIENTS:
    unreal.log_warning("Vael: ingredient {} is not in the material list".format(missing))

for asset, name, description, hex_code, kind, element, way in INGREDIENTS:
    definition = {
        "asset": "DA_Material_" + asset,
        "display_name": unreal.Text(name),
        "description": unreal.Text(description),
        "region": unreal.Text("Aschenmark"),
        "color": color(hex_code),
    }
    definition.update(ingredient_fields(kind, element, way))
    MATERIALS.append(definition)


def comparable(value):
    """Brings Unreal values into a form that can be compared with =="""
    if isinstance(value, unreal.Text):
        return str(value)
    if isinstance(value, unreal.LinearColor):
        return (round(value.r, 3), round(value.g, 3), round(value.b, 3), round(value.a, 3))
    if isinstance(value, float):
        return round(value, 3)
    return value


def create_or_complete_materials():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    defaults = unreal.get_default_object(unreal.VaelMaterial)
    created = 0
    completed = 0

    for definition in MATERIALS:
        values = dict(definition)
        name = values.pop("asset")
        path = "{}/{}".format(FOLDER, name)

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            material = unreal.EditorAssetLibrary.load_asset(path)
            filled = []

            for key, value in values.items():
                current = comparable(material.get_editor_property(key))
                untouched = current == comparable(defaults.get_editor_property(key))

                if untouched and current != comparable(value):
                    material.set_editor_property(key, value)
                    filled.append(key)

            if filled and unreal.EditorAssetLibrary.save_loaded_asset(material, False):
                completed += 1
                unreal.log("Vael: completed material {}: {}".format(path, ", ".join(filled)))
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelMaterial)

        material = asset_tools.create_asset(name, FOLDER, unreal.VaelMaterial, factory)
        if material is None:
            unreal.log_error("Vael: material {} could not be created".format(path))
            continue

        for key, value in values.items():
            material.set_editor_property(key, value)

        if unreal.EditorAssetLibrary.save_loaded_asset(material, False):
            created += 1
            unreal.log("Vael: created material {}".format(path))
        else:
            unreal.log_error("Vael: material {} could not be saved".format(path))

    unreal.log("Vael: {} material assets created, {} completed".format(created, completed))


create_or_complete_materials()
