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
