# Materials of the engineer's hunt.
#   2026-11-08: the Bruchstück, the break part a creature leaves when the engineer breaks its head or armour.
#   2026-11-14 (Docs/Ingenieur_Aschenmark_Materialien.md): the new parts of the Aschenmark creatures, their own break pieces,
#   and the Werkstoffe he refines at the forge, the grindstone or a campfire (Schlackeneisen, Rußglas, Latex …).
# Existing assets are updated (name, text, colour, rarity, region); mesh and icon come from a similar material.
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateEngineerMaterials.py" -unattended -nosplash
import unreal

MATERIALS = "/Game/Vael/Items/Materials"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
R = unreal.VaelRarity

PIECES = [
    # asset, name, description, colour, rarity, look from
    ("Bruchstueck", "Bruchstück", "Ein abgebrochenes Stück Horn, Panzer oder Klaue. Nur wer gezielt schießt, bricht es heraus. Der Ingenieur baut daraus seine besten Stücke.",
     (0.78, 0.52, 0.22), R.RARE, "DA_Material_Rollpanzer"),

    # Glutkriecher and Glutkönigin
    ("Krustenplatte", "Krustenplatte", "Eine schwarze, überlappende Platte vom Rücken eines Glutkriechers. In den Rissen glimmt es noch. Rüstungsplatte und Bolzenkern.",
     (0.11, 0.09, 0.08), R.COMMON, "DA_Material_Glutdruese"),
    ("Kriechersehne", "Kriechersehne", "Die zähe Sehne, die beim Glutkriecher die Platten beim Graben zusammenhält. Bindung für Rüstung und Riemen.",
     (0.55, 0.35, 0.22), R.MAGIC, "DA_Material_Spannfaden"),
    ("Glutkiefer", "Glutkiefer", "Bruchstück: der Kiefer eines Glutkriechers, aus dem Kopf gebrochen. Schwer und noch warm. Für die Meisterschleuder.",
     (0.6, 0.18, 0.05), R.RARE, "DA_Material_Glutdruese"),
    ("Brutkrone", "Brutkrone", "Der Kamm aus Krustenplatten der Glutkönigin, durchzogen von feinen violetten Adern. Helm der Glutkronen-Rüstung.",
     (0.25, 0.07, 0.12), R.LEGENDARY, "DA_Material_HerzDerGlut"),
    ("Koeniginnenauge", "Königinnenauge", "Ein Auge aus dem glühenden Augenkranz der Glutkönigin. Als Linse sieht es Wärme hinter Fels.",
     (1.0, 0.45, 0.08), R.LEGENDARY, "DA_Material_HerzDerGlut"),
    ("Legestachel", "Legestachel", "Der Stachel, mit dem die Glutkönigin ihre Brut legt. Hohl, hart und gebogen. Abzug der Meisterarmbrust.",
     (0.35, 0.15, 0.1), R.RARE, "DA_Material_HerzDerGlut"),
    ("Glutkrone", "Glutkrone", "Bruchstück: ein Stück aus der Krone der Glutkönigin, im Kampf herausgebrochen. Spannkopf der Meisterarmbrust.",
     (0.95, 0.35, 0.1), R.LEGENDARY, "DA_Material_HerzDerGlut"),

    # Aschharpyie and Harpyien-Älteste
    ("Greifklaue", "Greifklaue", "Eine Hakenklaue der Aschharpyie. Haken, Abzug, Kletterhaken.",
     (0.2, 0.18, 0.16), R.MAGIC, "DA_Material_Russfeder"),
    ("Hohlkiel", "Hohlkiel", "Ein hohler Federkiel der Harpyien. Der Ingenieur füllt ihn: eine Nadel, die im Ziel stecken bleibt.",
     (0.84, 0.78, 0.68), R.MAGIC, "DA_Material_Aeltestenschwinge"),
    ("Kreischbalg", "Kreischbalg", "Der Kehlbalg der Ältesten, mit dem sie kreischt. Für Pfeifen und Heulpfeile.",
     (0.55, 0.42, 0.4), R.RARE, "DA_Material_Aeltestenschwinge"),
    ("Harpyientand", "Harpyientand", "Gestohlener Tand aus der Krone der Ältesten: Knöpfe, Ringe, Messingbeschläge. Die einzige Messingquelle der Aschenmark.",
     (0.84, 0.64, 0.32), R.MAGIC, "DA_Material_Aeltestenschwinge"),
    ("Harpyienschnabel", "Harpyienschnabel", "Bruchstück: ein Hakenschnabel, aus dem Kopf einer Harpyie gebrochen. Für den Meisterbogen.",
     (0.3, 0.27, 0.22), R.RARE, "DA_Material_Russfeder"),

    # Aschenroller, Sichelpuppe, Schleieraffe, Prediger
    ("Rollsehne", "Rollsehne", "Die Sehne, mit der sich der Aschenroller einrollt. Federt kräftig zurück: Feder für Fangeisen, Armbrust und Spannschleuder.",
     (0.62, 0.48, 0.36), R.MAGIC, "DA_Material_Rollpanzer"),
    ("Rollpanzerkern", "Rollpanzerkern", "Bruchstück: das harte Mittelband eines Aschenrollers, aus der Flanke gebrochen.",
     (0.3, 0.25, 0.2), R.RARE, "DA_Material_Rollpanzer"),
    ("Puppengarn", "Puppengarn", "Erntegarn, das die letzte Garbe einer Sichelpuppe zusammenhält. Es glimmt und brennt nicht: feuerfeste Schnur.",
     (0.66, 0.5, 0.28), R.MAGIC, "DA_Material_Glimmstroh"),
    ("Sichelherz", "Sichelherz", "Bruchstück: der Nid-Knoten im Inneren einer Sichelpuppe, der sie bewegt hat.",
     (0.4, 0.12, 0.45), R.RARE, "DA_Material_Sichelklinge"),
    ("Klammerfinger", "Klammerfinger", "Ein langer, knochiger Finger des Schleieraffen. Greift und lässt nicht los: für den Wurfhaken.",
     (0.16, 0.15, 0.17), R.RARE, "DA_Material_Schleierpelz"),
    ("Ordenstuch", "Ordenstuch", "Stoff aus der Robe eines Predigers der Narbe. Dicht gewebt: Polster unter der Rüstung.",
     (0.45, 0.16, 0.24), R.MAGIC, "DA_Material_Ordenssiegel"),
    ("Siegelwachs", "Siegelwachs", "Das Wachs der Ordenssiegel. Versiegelt Kapseln und Töpfe.",
     (0.7, 0.2, 0.25), R.MAGIC, "DA_Material_Ordenssiegel"),

    # Werkstoffe: refined in the workshop or at a campfire
    ("Schlackeneisen", "Schlackeneisen", "Aus Schlacke ausgeschmolzenes Eisen: das Grundmetall des Ingenieurs für Fassungen, Nieten und Spitzen.",
     (0.43, 0.41, 0.43), R.COMMON, "DA_Material_Schlacke"),
    ("Schlackenglas", "Schlackenglas", "Dünnwandige Kugeln aus geschmolzener Schlacke. Zerspringen beim Aufschlag: Hülle für Feuer und Säure.",
     (0.27, 0.3, 0.27), R.COMMON, "DA_Material_Schlacke"),
    ("Russglas", "Rußglas", "Klar geschliffener Rußquarz: die Linse der Zielhilfebrille.",
     (0.59, 0.58, 0.63), R.MAGIC, "DA_Material_Russquarz"),
    ("Latex", "Latex", "Eingedickte Sternmilch: dehnbare Haut für Kapseln, Dichtungen und Bänder.",
     (0.94, 0.92, 0.86), R.COMMON, "DA_Material_Sternmilch"),
    ("SchwarzerLatex", "Schwarzer Latex", "Latex aus Gallmilch, zäher als heller. Schatten.",
     (0.2, 0.16, 0.22), R.MAGIC, "DA_Material_Gallmilch"),
    ("Blasenhaut", "Blasenhaut", "Eine leer gedrückte Aschblase: dünne Haut, die beim Aufschlag weich platzt. Hülle für Rauch und Licht.",
     (0.67, 0.69, 0.71), R.COMMON, "DA_Material_Rauchblase"),
    ("GehaertetePlatte", "Gehärtete Platte", "Zwei Krustenplatten, in der Glut gehärtet: der Kern schwerer Rüstung.",
     (0.16, 0.13, 0.13), R.MAGIC, "DA_Material_Glutdruese"),
    ("Messingblech", "Messingblech", "Eingeschmolzener Harpyientand, zu Blech gehämmert: Linsenringe und edle Fassungen.",
     (0.84, 0.64, 0.32), R.MAGIC, "DA_Material_Aeltestenschwinge"),
]

for ident, name, text, rgb, rarity, look in PIECES:
    path = "{}/DA_Material_{}".format(MATERIALS, ident)
    asset = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset("DA_Material_" + ident, MATERIALS, unreal.VaelMaterial, unreal.DataAssetFactory())
    if asset is None:
        unreal.log_error("[Ingenieur] Material {} could not be created".format(ident))
        continue
    look_path = "{}/{}".format(MATERIALS, look)
    source = unreal.load_asset(look_path) if assets.does_asset_exist(look_path) else None
    asset.set_editor_property("display_name", unreal.Text(name))
    asset.set_editor_property("description", unreal.Text(text))
    asset.set_editor_property("color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    asset.set_editor_property("category", unreal.VaelMaterialCategory.MATERIAL)
    asset.set_editor_property("rarity", rarity)
    if ident != "Bruchstueck":
        asset.set_editor_property("region", unreal.Text("Aschenmark"))
    try:
        asset.set_editor_property("kind", unreal.VaelIngredientKind.PART)
    except Exception as error:
        unreal.log_warning("[Ingenieur] kind: {}".format(error))
    if source is not None:
        for prop in ("mesh", "icon"):
            try:
                asset.set_editor_property(prop, source.get_editor_property(prop))
            except Exception:
                pass
    else:
        unreal.log_warning("[Ingenieur] look {} missing for {}".format(look, ident))
    assets.save_loaded_asset(asset, False)
    unreal.log_warning("[Ingenieur] Material {} angelegt".format(ident))
