# Creates the entries of Albrun's compendium and keeps them complete.
#
# - An entry that doesn't exist yet is created with the values below.
# - An entry that exists only gets values filled in that are still at the class default.
#   Values that differ from the default are never overwritten, so texts edited in the editor stay.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateCompendiumAssets.py" -EnablePlugins=PythonScriptPlugin
#
# The facts follow the game (weaknesses, loot, behaviour) and the Lore-Bibel; the voice is Albrun's.

import unreal

FOLDER = "/Game/Vael/Compendium"

Book = unreal.VaelCompendiumBook

ENTRIES = [
    {
        "asset": "DA_Compendium_Glutkriecher",
        "subject_id": "Glutkriecher",
        "book": Book.BESTIARY,
        "sort_order": 10,
        "display_name": unreal.Text("Glutkriecher"),
        "region": unreal.Text("Aschenmark"),
        "glimpse": unreal.Text("Ein flacher Hügel Asche, der sich bewegt, wenn kein Wind weht. Wo er stehen bleibt, glüht es darunter."),
        "behaviour": unreal.Text("Gräbt sich in verbrannten Boden und wandert unter der Asche auf alles zu, was warm ist und atmet. Ist es nah genug, bricht es heraus, die Drüse am Hinterleib schwillt an, und es zerplatzt in Glut. Wer zu lange zusieht, steht danach im Feuer."),
        "weakness": unreal.Text("Unter der Asche kaum zu verletzen, nur Erde erreicht es dort. Draußen trifft Wasser es hart, und ein Guss löscht die Zündung, danach kämpft es mit den Zangen weiter. Feuer kitzelt es nur. Lässt oft eine Glutdrüse zurück."),
        "uses": unreal.Text("Glutdrüsen halten Hitze erstaunlich lange. Ein findiger Ingenieur baut daraus Brandminen, die sich selbst eingraben. Und wer die Explosion aus nächster Nähe übersteht, hat danach eine Ahnung davon, wie man Feuer zu einem Strahl bündelt."),
        "marked_form": unreal.Text("Gezeichnete Kriecher tragen violette Adern in der Drüse und vertragen mehr. Ihre Glut ist dunkler, als Glut sein sollte."),
        "albrun_note": unreal.Text("Die Brut der Glutkönigin, jeder einzelne ein Satz, den sie nicht zu Ende sprechen kann. Ich habe sieben gezählt, die sich gegenseitig ausgruben, bevor einer mich bemerkte. Die anderen sechs bemerkten mich danach. Lehrreicher Nachmittag."),
        "observe_seconds": 6.0,
        "study_count": 3,
    },
    {
        "asset": "DA_Compendium_Aschharpyie",
        "subject_id": "Aschharpyie",
        "book": Book.BESTIARY,
        "sort_order": 20,
        "display_name": unreal.Text("Aschharpyie"),
        "region": unreal.Text("Aschenmark"),
        "glimpse": unreal.Text("Ein Schatten, der über den Feldern Kreise zieht. Rußgraue Schwingen, viel zu lange Krallen."),
        "behaviour": unreal.Text("Kreist um ihr Revier, dann um ihre Beute, enger und enger. Kurz bevor sie zustößt, steht sie still in der Luft und zielt; dann stürzt sie herab und steigt wieder auf. Wer das Stillstehen erkennt, hat einen Atemzug Zeit."),
        "weakness": unreal.Text("Erde holt sie vom Himmel, auch Wind wirft sie aus der Bahn. Am verwundbarsten ist sie, wenn sie nach dem Sturz wieder steigt. Lässt manchmal eine Rußfeder fallen."),
        "uses": unreal.Text("Rußfedern sind leicht und fast nicht zu verbrennen, gut für Pfeile und Gleiter. Wer ihren Sturzflug oft genug aus der Nähe gesehen hat, versteht, wie Erde und Wind zusammen einen Sturm aus Sand ergeben."),
        "marked_form": unreal.Text("Gezeichnete Harpyien kreischen mit zwei Stimmen, und ihre Federn bluten violett, wenn sie fallen."),
        "albrun_note": unreal.Text("Sie halten mich für ein sehr langsames, sehr altes Huhn. Ich lasse sie in dem Glauben. Es ist für beide Seiten bequemer."),
        "observe_seconds": 6.0,
        "study_count": 3,
    },
    {
        "asset": "DA_Compendium_HarpyienAelteste",
        "subject_id": "HarpyienAelteste",
        "book": Book.BESTIARY,
        "sort_order": 30,
        "display_name": unreal.Text("Harpyien-Älteste"),
        "region": unreal.Text("Aschenmark, Felder"),
        "glimpse": unreal.Text("Größer als die anderen, schwerer im Flug, und die anderen machen ihr Platz."),
        "behaviour": unreal.Text("Zieht weite, langsame Kreise und ruft ihre Schwestern zu sich. Ihr Sturz kommt seltener, aber er trifft wie ein fallender Baum. Sie lässt sich kaum wegstoßen."),
        "weakness": unreal.Text("Erde trifft sie härter als alles andere. Wer sie besiegt, findet eine Ältestenschwinge und oft ein Fragment einer Formel, die sie im Wind gelernt hat."),
        "uses": unreal.Text("Ältestenschwingen tragen den Wind in sich; Gelehrte haben aus ihnen einst den Wirbelsturm gelesen. Für einen Tamer wäre sie ein stolzer, sehr eigensinniger Gefährte."),
        "marked_form": unreal.Text("Eine gezeichnete Älteste ruft nicht nur Schwestern. Was dann kommt, möchte ich nicht noch einmal sehen."),
        "albrun_note": unreal.Text("Sie ist älter als das Dorf unter ihr. Vielleicht älter als ich. Wir haben uns einmal lange angesehen und beschlossen, einander nicht zu erwähnen."),
        "observe_seconds": 8.0,
        "study_count": 1,
    },
    {
        "asset": "DA_Compendium_Glutkoenigin",
        "subject_id": "Glutkoenigin",
        "book": Book.BESTIARY,
        "sort_order": 40,
        "display_name": unreal.Text("Glutkönigin"),
        "region": unreal.Text("Aschenmark, Krater"),
        "glimpse": unreal.Text("Im Krater liegt etwas Großes und atmet Rauch. Es schläft. Hoffentlich."),
        "behaviour": unreal.Text("Schläft, bis man ihr zu nahe kommt oder sie verletzt. Dann beißt sie, was neben ihr steht, speit Feuer in Salven und ruft ihre Brut aus der Asche. Geschwächt schickt sie Ringe aus Flammen und stürmt los."),
        "weakness": unreal.Text("Wasser verlangsamt und schwächt sie, Feuer berührt sie kaum. Ihr Leib hinterlässt das Herz der Glut."),
        "uses": unreal.Text("Das Herz der Glut ist der heißeste Stoff, den ich je in der Hand hielt, und ich hielt es nicht lange. Daraus lässt sich Mächtiges schmieden, aber nichts, was je wieder ganz abkühlt."),
        "marked_form": unreal.Text("Sie ist bereits gezeichnet. Einst war sie eine gewöhnliche Aschkäferkönigin; das Mark hat sie zur Mutter gemacht, die nie aufhört zu gebären. Unter ihr brach die Flut aus."),
        "albrun_note": unreal.Text("Ihr Sterbeschrei klingt wie der des Quellwächters. Ich habe ihn aufgeschrieben, so gut man einen Schrei aufschreiben kann. Ich glaube, es ist nicht ihre Stimme. Ich glaube, sie leiht sie nur."),
        "observe_seconds": 10.0,
        "study_count": 1,
    },
    {
        "asset": "DA_Compendium_Quellwaechter",
        "subject_id": "Quellwaechter",
        "book": Book.BESTIARY,
        "sort_order": 50,
        "display_name": unreal.Text("Quellwächter"),
        "region": unreal.Text("an jeder Mark-Quelle"),
        "glimpse": unreal.Text("Er steigt aus der Quelle, wenn man ihr etwas opfert. Viele Tiere in einem Leib."),
        "behaviour": unreal.Text("Schlägt zu wie ein Tier, platzt wie ein Kriecher, stürzt wie eine Harpyie und ruft seinen Schwarm aus der Quelle. Wird er verwundet, schreit er, und der Schwarm wird dichter. Er handelt wie ein Bewusstsein aus vielen."),
        "weakness": unreal.Text("Wasser und Erde treffen ihn hart, das Mark nur halb. Fällt er, versiegelt sich die Quelle, und das Land darum atmet auf."),
        "uses": unreal.Text("Er lässt nichts zurück als eine versiegelte Quelle. Das ist mehr, als die meisten Dinge auf dieser Welt hinterlassen."),
        "marked_form": unreal.Text("Er ist aus dem Mark gemacht. Jede Region formt ihren eigenen Wächter aus ihren eigenen Bewohnern."),
        "albrun_note": unreal.Text("In seinem Schrei liegen zwei Dinge: der Hilferuf der Schlafenden und ihre Wut. Die Hainwesen hören beides. Ich höre nur, dass es wehtut. Das genügt mir, um mitzuschreiben."),
        "observe_seconds": 8.0,
        "study_count": 1,
    },
    # First creature of the creature factory (Bestiarium row Spannhornkäfer, look by Boris 2026-10-10)
    {
        "asset": "DA_Compendium_Spannhornkaefer",
        "subject_id": "Spannhornkaefer",
        "book": Book.BESTIARY,
        "sort_order": 110,
        "display_name": unreal.Text("Spannhornkäfer"),
        "region": unreal.Text("Wurzelforst"),
        "glimpse": unreal.Text("Ein Käfer so groß wie ein Hund, grün gefleckt wie das Moos, mit Hörnern wie ein Hirschgeweih. Zwischen den Spitzen glänzt etwas Dünnes."),
        "behaviour": unreal.Text("Hält Abstand. Dann spreizt er die Hörner, und zwischen ihren Spitzen spannt sich ein Faden; das ratscht hörbar, Kerbe für Kerbe. Mit einem Klack rastet der Faden am Mittelhorn ein, und was in dessen Rinne liegt, fliegt im Bogen dorthin, wo du eben noch gestanden hast. Wo es einschlagen wird, verfärbt sich vorher der Boden."),
        "weakness": unreal.Text("Feuer brennt den gespannten Faden durch, dann steht er eine Weile wehrlos da und verträgt kaum etwas. Auch sonst nimmt er Feuer schlecht. Lässt oft seinen Spannfaden zurück."),
        "uses": unreal.Text("Der Spannfaden ist zäher als jede Sehne. Ein Ingenieur bespannt damit seinen ersten richtigen Bogen, später eine Wurfmaschine. Ein Tamer, der den Faden im gespannten Moment durchtrennt, statt ihn zu verbrennen, hat einen Schützen, der nie verfehlt."),
        "marked_form": unreal.Text("Gezeichnete Käfer spannen einen Faden, der violett glimmt. Was sie verschießen, zischt beim Einschlag."),
        "albrun_note": unreal.Text("Wer spannt wie der Käfer, verfehlt nie, sagen die Jäger im Forst. Ich habe einem zugesehen, der drei Mal hintereinander denselben Pilz traf. Der Pilz hat es überlebt. Ich habe ihn trotzdem notiert."),
        "observe_seconds": 7.0,
        "study_count": 3,
    },
]


def nature(asset, subject_id, book, sort_order, name, region, glimpse, behaviour, weakness, uses, marked_form, albrun_note):
    """An entry of the Herbarium, the Pilzbuch or the Buch der Gesteine; these kinds are quick to watch"""
    return {
        "asset": asset, "subject_id": subject_id, "book": book, "sort_order": sort_order,
        "display_name": unreal.Text(name), "region": unreal.Text(region),
        "glimpse": unreal.Text(glimpse), "behaviour": unreal.Text(behaviour), "weakness": unreal.Text(weakness),
        "uses": unreal.Text(uses), "marked_form": unreal.Text(marked_form), "albrun_note": unreal.Text(albrun_note),
        "observe_seconds": 4.0, "study_count": 3,
    }


# Plants, fungi and stones of the Aschenmark (Lore-Bibel, tab Kompendium, "Arten der Regionen")
ENTRIES += [
    nature("DA_Compendium_Glutdistel", "Glutdistel", Book.HERBARIUM, 10, "Glutdistel", "Aschenmark, verbrannter Boden",
           "Schwarze, verkohlte Stängel. Oben glimmt etwas in den Blütenköpfen, als hätte jemand Laternen in die Asche gesteckt.",
           "Wächst nur dort, wo es schon einmal gebrannt hat. Wer sie streift, steht in Flammen: Die Blüten entzünden sich bei jeder Berührung und brennen nieder, ohne die Pflanze zu verzehren.",
           "Wasser löscht sie für eine Weile. Gepflückt gibt sie Glutsamen, die noch Stunden warm bleiben.",
           "Feuerzauber in ihrer Nähe kosten weniger und brennen heißer. Glutsamen sind eine Feuerzutat für Tränke und für Brandmunition des Ingenieurs.",
           "Gezeichnete Disteln glühen violett und entzünden sich schon, wenn man nur in ihrer Nähe atmet.",
           "Ich habe mir an der ersten die Ärmel verbrannt, an der zweiten den Bart. Die dritte habe ich mit einem langen Stock gepflückt. Wissenschaft ist Wiederholung."),
    nature("DA_Compendium_Traenenkelch", "Traenenkelch", Book.HERBARIUM, 20, "Tränenkelch", "Aschenmark, am Teich",
           "Eine hängende Blüte, aus der Licht tropft. Die Tropfen fallen langsam, als hätten sie es nicht eilig.",
           "Wächst nur am Wasser und nur, wo das Land noch wenig verdorben ist. Alle paar Atemzüge fällt ein leuchtender, milchiger Tropfen. Wer darunter steht, spürt, wie Wunden sich schließen.",
           "Verträgt keine Verderbnis: Steigt sie, welkt der Kelch. Gepflückt gibt er Kelchtau.",
           "Kelchtau ist die Grundzutat für Heiltränke. In seiner Nähe kosten Wasserzauber weniger.",
           "Gezeichnete Kelche weinen dunkel. Ihr Tau heilt immer noch, aber er hinterlässt einen Geschmack von Eisen.",
           "Die Leute am Teich sagen, die Schlafende weint hier um ihre Kinder. Ich habe die Tropfen gezählt, gewogen und gekostet. Sie könnten recht haben."),
    nature("DA_Compendium_Aschblase", "Aschblase", Book.HERBARIUM, 30, "Aschblase", "Aschenmark, Krater und Aschefelder",
           "Dünne, dunkle Stiele, an denen Glaskugeln hängen. In den Kugeln bewegt sich grauer Rauch.",
           "Die Pflanze fängt den Rauch der Brände in ihren Blasen und hält ihn fest. Ein Treffer genügt, und die Blase platzt zu einer dichten Wolke, die alles darin blendet.",
           "Platzt bei jedem Treffer, egal womit. Vorsichtig gepflückt bleibt eine Rauchblase heil.",
           "Luftzauber in ihrer Nähe kosten weniger. Der Ingenieur füllt Rauchblasen in Wurfgeschosse, um Gegner zu blenden.",
           "Gezeichnete Blasen tragen violetten Rauch, der nicht nur blendet, sondern auch im Hals brennt.",
           "Ich habe eine Blase in meine Tasche gesteckt, um sie im Lager zu öffnen. Das Lager hat das anders entschieden als ich."),
    nature("DA_Compendium_Laternenglocke", "Laternenglocke", Book.HERBARIUM, 40, "Laternenglocke", "Aschenmark, an Wegen und am Lager",
           "Nickende Glocken aus grauen, ledrigen Blättern. In jeder glimmt ein kleines, warmes Licht.",
           "Wächst an Wegen, wo Menschen gehen, als wolle sie ihnen leuchten. Ihr Licht ist ein Maß für das Land: Je reiner die Region, desto heller brennt die Glocke; wird das Land krank, wird sie trüb.",
           "Ohne Licht ist sie nur ein graues Kraut. Gepflückt gibt sie Glockenglut.",
           "Wer auf ihre Helligkeit achtet, liest den Verderbnis-Grad ohne Karte. Glockenglut leuchtet noch Tage in einer Laterne.",
           "Gezeichnete Glocken leuchten nicht mehr warm, sondern kalt und violett. Wo sie stehen, ist es schon zu spät.",
           "Die Leute im Lager sagen, die Glocken stehen am Weg, damit die Toten heimfinden. Ich sage, sie wachsen dort, wo der Boden festgetreten ist. Wir haben beide recht, nur ich habe es aufgeschrieben."),
    nature("DA_Compendium_Russmoos", "Russmoos", Book.HERBARIUM, 50, "Rußmoos", "Aschenmark, tote Stämme und Ruinen",
           "Ein schwarzgrauer Pelz auf totem Holz. Man hält es für Ruß, bis man es anfasst.",
           "Frisst Asche und Ruß und wächst darauf. Bei Regen saugt es sich voll und wird dunkelgrün; dann sieht man, dass es lebt.",
           "Brennt nicht, auch wenn es so aussieht. Gepflückt gibt es eine Handvoll Rußmoos.",
           "Tiere, die damit gefüttert werden, wachsen rein, auch wenn das Land ringsum verdorben ist. Für den Tamer das wichtigste Futter der Aschenmark.",
           "Gezeichnetes Moos wird nie grün, auch nicht im Regen. Tiere meiden es, und das zu Recht.",
           "Das bescheidenste Gewächs der Aschenmark und das fleißigste. Es räumt hinter allen anderen auf. Ich mag es sehr."),
    nature("DA_Compendium_Fleischkelch", "Fleischkelch", Book.HERBARIUM, 60, "Fleischkelch", "Aschenmark, rund um die Mark-Quelle",
           "Eine fleischige Knolle mit Löchern, so groß wie ein Kind. Sie hebt und senkt sich.",
           "Wächst nur bei hoher Verderbnis. Die Knolle atmet, und durch die Löcher saugt sie Insekten ein, die vom Mark angezogen werden. Je näher die Quelle, desto größer wird sie.",
           "Feuer lässt sie zusammenschrumpfen. Geschnitten gibt sie Kelchfleisch mit violetten Adern.",
           "Kelchfleisch ist eine Mark-Zutat: stark, aber mit einem Preis. Wo Fleischkelche wachsen, ist die Region krank.",
           "Er hat keine reine Form. Er ist die gezeichnete Form von etwas, das es hier nicht mehr gibt.",
           "Ich habe lange überlegt, ob er ins Bestiarium gehört. Er atmet, er frisst, er schaut einen nicht an. Fürs Erste bleibt er hier, unter Beobachtung."),
    nature("DA_Compendium_Pfeifenmorchel", "Pfeifenmorchel", Book.FUNGI, 10, "Pfeifenmorchel", "Aschenmark, tote Stämme und Friedhof",
           "Ein rostroter, spitzer Hut. Darunter stehen Röhren ab wie bei einer kleinen Orgel.",
           "Wenn Wind aufkommt, pfeifen die Röhren, lange bevor man ihn spürt. Je lauter der Pilz, desto härter der Sturm, der kommt. Dabei stößt er Sporen aus.",
           "Bei Windstille stumm und leicht zu übersehen. Gesammelt gibt sie Pfeifensporen.",
           "Wer zuhört, weiß, wann ein Sturm kommt. Für den Propheten ein Vorzeichen, für alle anderen eine Warnung, die man ernst nehmen sollte.",
           "Gezeichnete Morcheln pfeifen auch ohne Wind, einen Ton, den man eher im Bauch hört als in den Ohren.",
           "Die Dorfleute nennen sie Totenflöte, weil sie am Friedhof wächst. Ich nenne sie Wetterbericht. Wir sollten uns auf einen Namen einigen, bevor der nächste Sturm kommt."),
    nature("DA_Compendium_Aschenstaeubling", "Aschenstaeubling", Book.FUNGI, 20, "Aschenstäubling", "Aschenmark, Aschefelder",
           "Graue Kugeln im grauen Boden. Man sieht sie erst, wenn man draufgetreten ist.",
           "Wer auf ihn tritt, steht in einer Wolke aus feiner Asche. Diese Wolke erstickt Flammen: Wer brennt, hört auf zu brennen.",
           "Einmal geplatzt, ist er leer, bis er nachwächst. Gesammelt gibt er Stäublingspulver.",
           "Stäublingspulver ist die Grundzutat für den Löschtrank. Klug ist, wer brennend auf einen Stäubling zuläuft.",
           "Gezeichnete Stäublinge löschen nicht mehr, ihre Wolke glimmt violett und brennt selbst.",
           "Ich bin einmal brennend über ein ganzes Feld davon gelaufen. Danach war ich gelöscht, grau und um eine Erkenntnis reicher."),
    nature("DA_Compendium_Adernflechte", "Adernflechte", Book.FUNGI, 30, "Adernflechte", "Aschenmark, zwischen Feldern und Mark-Quelle",
           "Feine weiße Fäden, die über den Boden laufen wie Adern unter Haut.",
           "Die Fäden ziehen sich durch die ganze Erde. Wo Mark nah ist, färben sie sich violett, und die Färbung zeigt den Weg zur Quelle. Manchmal zucken sie, alle zugleich.",
           "Reißt leicht. Gesammelt gibt sie eine Myzelprobe.",
           "Wer den violetten Fäden folgt, findet die Mark-Quelle. Albrun hält das Geflecht für die Nerven der Schlafenden.",
           "Die Flechte selbst wird nicht gezeichnet. Sie zeigt nur, wo es schon geschehen ist.",
           "Wenn sie alle zugleich zucken, zuckt etwas sehr Großes unter uns. Ich schreibe die Zeiten auf. Noch ergibt es kein Muster. Noch."),
    nature("DA_Compendium_Schlacke", "Schlacke", Book.STONES, 10, "Schlacke", "Aschenmark, Dorf und Wegränder",
           "Schwarze, glasige Brocken, scharf an den Kanten. Kein Stein, eher etwas, das einmal geschmolzen war.",
           "Abfall der alten Schmelzen, als hier noch Erz verhüttet wurde. Liegt dort, wo Menschen gearbeitet haben, nicht dort, wo die Erde ihn gemacht hätte.",
           "Bricht leicht und splittert. Abgebaut gibt sie Schlacke.",
           "Grundmaterial für den Ingenieur: Splitter für Munition, Glas für einfache Vorrichtungen.",
           "Schlacke wird nicht gezeichnet. Sie ist schon tot.",
           "Das einzige Gestein in diesem Buch, das die Schlafende nicht gemacht hat. Ich habe lange gezögert, es aufzunehmen. Aber auch Narben gehören zum Körper."),
    nature("DA_Compendium_Glutstein", "Glutstein", Book.STONES, 20, "Glutstein", "Aschenmark, Krater und Felder",
           "Ein rissiger Brocken, in dessen Nähten es orange glüht, auch in der Nacht.",
           "Speichert Hitze über Tage. Trifft ihn Wasser, entweicht sie auf einmal als heißer Dampfstoß. Feuer in seiner Nähe lädt ihn wieder auf.",
           "Wasser kühlt ihn kurz ab. Abgebaut gibt er Glutstein.",
           "Feuerzauber in seiner Nähe kosten weniger. Glutstein ist die Grundlage für den Wärmetrank und für Feuermunition.",
           "Gezeichnete Glutsteine glühen violett, und ihr Dampf riecht nach Blut.",
           "Ich wärme mir abends die Füße an einem. Die anderen im Lager halten das für Leichtsinn. Die anderen im Lager haben kalte Füße."),
    nature("DA_Compendium_Russquarz", "Russquarz", Book.STONES, 30, "Rußquarz", "Aschenmark, Kraterrand",
           "Rauchgraue Kristalle in schwarzem Geröll. Hält man sie ins Licht, sieht man Schlieren darin.",
           "Er wird klarer, je weiter die Verderbnis der Region sinkt. In einem kranken Land bleibt er trüb wie Rauch.",
           "Spröde. Abgebaut gibt er Rußquarz.",
           "Erdzauber in seiner Nähe kosten weniger. Klar geschliffen ergibt er eine Linse, mit der man weiter und schärfer sieht.",
           "Gezeichneter Quarz wird nie klar. In ihm stehen violette Schlieren still.",
           "Die Linse in meinem Glas war einmal ein Rußquarz. Ich habe drei Jahre gewartet, bis das Land um ihn gesund genug war. Geduld ist auch ein Werkzeug."),
    nature("DA_Compendium_Markdruse", "Markdruse", Book.STONES, 40, "Markdruse", "Aschenmark, an der Mark-Quelle",
           "Violette Kristalle, die stufig aus dem Fels brechen, als wären sie in Schüben gewachsen.",
           "Wächst nur an einer offenen Mark-Quelle. Jede Stufe ist ein Schub: Wer sie zählt, weiß, wie lange die Quelle schon offen ist.",
           "Bricht unter Erde und schweren Schlägen. Abgebaut gibt sie Markkristalle.",
           "Markkristalle sind die Opfergabe an die Quelle, mit der man ihren Wächter ruft.",
           "Sie ist das Mark selbst, in Stein gefasst.",
           "Ich habe die Stufen an der Quelle der Aschenmark gezählt. Wie lange eine Stufe braucht, weiß ich noch nicht. Ich hoffe auf Jahre und fürchte, es sind Tage."),
]


def comparable(value):
    """Brings Unreal values into a form that can be compared with =="""
    if isinstance(value, unreal.Text):
        return str(value)
    if isinstance(value, float):
        return round(value, 3)
    if isinstance(value, unreal.Name):
        return str(value)
    return value


def create_or_complete_entries():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    defaults = unreal.get_default_object(unreal.VaelCompendiumEntry)
    created = 0
    completed = 0

    for definition in ENTRIES:
        values = dict(definition)
        name = values.pop("asset")
        path = "{}/{}".format(FOLDER, name)

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            entry = unreal.EditorAssetLibrary.load_asset(path)
            filled = []

            for key, value in values.items():
                current = comparable(entry.get_editor_property(key))
                untouched = current == comparable(defaults.get_editor_property(key))

                if untouched and current != comparable(value):
                    entry.set_editor_property(key, value)
                    filled.append(key)

            if filled and unreal.EditorAssetLibrary.save_loaded_asset(entry, False):
                completed += 1
                unreal.log("Vael: completed compendium entry {}: {}".format(path, ", ".join(filled)))
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelCompendiumEntry)

        entry = asset_tools.create_asset(name, FOLDER, unreal.VaelCompendiumEntry, factory)
        if entry is None:
            unreal.log_error("Vael: compendium entry {} could not be created".format(path))
            continue

        for key, value in values.items():
            entry.set_editor_property(key, value)

        if unreal.EditorAssetLibrary.save_loaded_asset(entry, False):
            created += 1
            unreal.log("Vael: created compendium entry {}".format(path))
        else:
            unreal.log_error("Vael: compendium entry {} could not be saved".format(path))

    unreal.log("Vael: {} compendium entries created, {} completed".format(created, completed))


create_or_complete_entries()
