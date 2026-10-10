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
        "display_name": unreal.Text("Rauchblase"),
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
        "display_name": unreal.Text("Rußmoos"),
        "description": unreal.Text("Schwarzgraues Moos von toten Stämmen. Futter, mit dem Tiere rein wachsen."),
        "region": unreal.Text("Aschenmark"),
        "color": color("5c6648"),
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
        "display_name": unreal.Text("Pfeifensporen"),
        "description": unreal.Text("Rostroter Staub aus den Röhren der Pfeifenmorchel."),
        "region": unreal.Text("Aschenmark"),
        "color": color("b0442a"),
    },
    {
        "asset": "DA_Material_Staeublingspulver",
        "display_name": unreal.Text("Stäublingspulver"),
        "description": unreal.Text("Graues Pulver des Aschenstäublings. Erstickt Flammen; Grundzutat für den Löschtrank."),
        "region": unreal.Text("Aschenmark"),
        "color": color("a49c92"),
    },
    {
        "asset": "DA_Material_Myzelprobe",
        "display_name": unreal.Text("Myzelprobe"),
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
