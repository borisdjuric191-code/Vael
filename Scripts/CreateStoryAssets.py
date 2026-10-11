# Creates the dialogue data assets of Vael, like the one of Edda Krell.
#
# - A dialogue that doesn't exist yet is created with the values below.
# - Existing dialogues are never touched, so text written in the editor stays.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/CreateStoryAssets.py" -EnablePlugins=PythonScriptPlugin

import unreal

FOLDER = "/Game/Vael/Story"

Condition = unreal.VaelHintCondition


def hint(condition, line):
    return unreal.VaelDialogueHint(condition=condition, line=unreal.Text(line))


DIALOGUES = [
    {
        # Lines of the browser prototype. Changed: Mark awakens only in Act III, so the hint about the source
        # now points to the offering of Markkristalle instead of awakening the Mark there.
        "asset": "DA_Dialogue_Edda",
        "speaker_name": unreal.Text("Edda Krell"),
        "intro_lines": [
            unreal.Text("Ruhig. Du hast drei Tage im Fieber gelegen. Die Mark-Flut hat Kesselgrund verschlungen. Außer uns ist niemand mehr übrig."),
            unreal.Text("Im Fieber hast du gesprochen. Feuer, Wasser, Erde, Luft. Jetzt glüht etwas in deinen Händen. Die Alten nannten es den Geist der Schlafenden."),
            unreal.Text("Probier es aus. Wähle Elemente und wirke sie gemeinsam. Manche Verbindungen zeigen sich, wenn du experimentierst. Die meisten stehen in alten Schriftrollen oder in den Angriffen der Bestien."),
            unreal.Text("Die Felder sind voller Brut. Glutkriecher graben sich in die Asche, Harpyien kreisen über den Feldern. Im Nordosten, im alten Krater, brütet ihre Mutter."),
            unreal.Text("Und wenn du im Osten das violette Leuchten siehst: Das ist Mark. Es macht stark, aber es nimmt sich etwas dafür. Komm ans Feuer, wenn es dich zu sehr zeichnet."),
        ],
        "hints": [
            hint(Condition.FEW_FORMULAS_DISCOVERED, "Versuch Paare. Zwei gleiche Elemente, Feuer mit Erde, Wasser mit Erde. Manches gelingt nicht beim ersten Mal."),
            hint(Condition.SCROLLS_LEFT, "Im Dorf stand ein Brunnen, die Kapelle liegt dahinter. Am Teich im Westen haben die Alten einen Schrein gebaut. Such dort nach Schriftrollen."),
            hint(Condition.MARK_SOURCE_OPEN, "Die Quelle im Osten blutet. Gezeichnete Wesen tragen Kristalle davon in sich. Bring genug davon hin, dann zeigt sich, was sie bewacht."),
            hint(Condition.BOSS_ALIVE, "Die Glutkönigin hasst Wasser. Steh nicht in ihrem Feuer, und weich ihrem Flammenring aus."),
            hint(Condition.BOSS_DEFEATED, "Du hast sie wirklich getötet. Vielleicht hat dieses Land doch noch eine Zukunft."),
        ],
        "max_hints_per_talk": 3,
    },
    {
        # Faktorin of the Gildenkontor (Storyleitfaden, "Akt I im Detail"): believes in the guild and does not know everything.
        # Contracts and their lines are quests (Scripts/CreateQuestAssets.py, K*).
        "asset": "DA_Dialogue_Maren",
        "speaker_name": unreal.Text("Maren Holt"),
        "intro_lines": [
            unreal.Text("Überlebende aus Kesselgrund? Dann habt ihr Glück gehabt. Maren Holt, Faktorin der Gilde. Willkommen im Kontor."),
            unreal.Text("Die Gilde zahlt für Arbeit, nicht für Mitleid. Wer Kontrakte erfüllt, bekommt Gildenmünzen, und für Münzen bekommt ihr bei mir Tränke und Vorräte."),
            unreal.Text("Wenn das Land wieder sauber ist, kauft die Gilde es zu einem fairen Preis. Ein Neuanfang für alle. So hat es Kesselgrund nie gehabt."),
        ],
        "hints": [
            hint(Condition.BOSS_ALIVE, "Solange die Königin im Krater brütet, kommt die Brut immer wieder. Die Gilde zahlt jeden, der sie kleinhält."),
            hint(Condition.MARK_SOURCE_OPEN, "Das Leuchten im Osten? Halt dich fern davon. Die Gilde hat Leute, die sich darum kümmern. Irgendwann."),
            hint(Condition.ALWAYS, "Der Orden erzählt viel über die Gilde. Brot verteilen ist leicht, wenn man nichts aufbauen muss."),
            hint(Condition.BOSS_DEFEATED, "Die Königin ist tot? Dann wird das Land bald etwas wert sein. Gut gemacht, wirklich."),
        ],
        "max_hints_per_talk": 2,
    },
    {
        # Albrun keeps the compendium (Lore-Bibel, tab Kompendium): very old, from the Hain, calm, curious, playful.
        # Albrun speaks of Albrun by name, never "ich bin ein Mann/eine Frau"; texts about Albrun use the name instead of er/sie.
        # Every talk studies the samples the group brought (research stage 4).
        "asset": "DA_Dialogue_Albrun",
        "speaker_name": unreal.Text("Albrun"),
        "intro_lines": [
            unreal.Text("Ah. Hände, die leuchten. Albrun hat davon gelesen, aber gelesen ist nicht gesehen. Darf Albrun? Nein? Später vielleicht."),
            unreal.Text("Albrun kommt aus dem Hain. Die Flut hier war laut genug, dass man sie bis unter die Wurzeln gehört hat. Also ist Albrun dem Lärm nachgegangen."),
            unreal.Text("Was die anderen spüren, muss Albrun verstehen. Dafür gibt es das Kompendium: jedes Tier, jedes Kraut, jeden Pilz und jeden Stein dieser Welt, sauber aufgeschrieben."),
            unreal.Text("Seht hin, bevor ihr zuschlagt. Und bringt Albrun Proben mit: drei von einer Art genügen meist. Den Rest erledigen Lupe, Tinte und Geduld."),
        ],
        "hints": [
            hint(Condition.BOSS_ALIVE, "Im Krater sitzt eine Mutter, die nicht aufhören kann zu gebären. Albrun hat sie gesehen, von weitem. Das ist keine Bosheit. Das ist ein Befehl, den jemand anderes gegeben hat."),
            hint(Condition.MARK_SOURCE_OPEN, "Die Quelle im Osten summt. Ihr hört es nicht? Beneidenswert."),
            hint(Condition.ALWAYS, "Mit dem Glas beobachten, dann erlegen oder ernten, dann zu Albrun. In dieser Reihenfolge, wenn es sich einrichten lässt."),
            hint(Condition.BOSS_DEFEATED, "Sie hat geschrien, als sie starb. Albrun kennt diesen Ton. Er kommt nicht aus einer Kehle."),
        ],
        "max_hints_per_talk": 2,
        "studies_samples": True,
        "study_done_line": unreal.Text("So. Lupe weg, Tinte trocken. Neu im Kompendium, vollständig: {0}. Albrun bedankt sich bei allen Beteiligten, auch bei den unfreiwilligen."),
    },
]


def gossip(line, flag=""):
    return unreal.VaelGossipLine(line=unreal.Text(line), required_flag=unreal.Name(flag))


# Gerede (Storyleitfaden "Akt I im Detail"): people mutter in the background. For the mage about hunger, the guild and the glow
# in the hands; after the choice at the end of the act about what the group did with the proof.
GOSSIP = [
    {
        "asset": "DA_Gossip_Lager",
        "speaker_name": unreal.Text("Überlebende"),
        "lines": [
            gossip("Drei Tage Fieber, und jetzt dieses Leuchten in den Händen."),
            gossip("Wenn auf den Feldern nichts kommt, essen wir im Winter Asche."),
            gossip("Die Gilde zahlt in Münzen, die nur sie selbst annimmt. Kluge Leute."),
            gossip("Edda schläft nicht mehr. Sie zählt nur noch."),
            gossip("Hast du das Leuchten gesehen? Sag nichts. Schau einfach nicht hin."),
            gossip("Die Kutten verteilen Brot. Ich frage mich, was sie dafür zurückwollen."),
            gossip("Sie haben es den Kutten gegeben. Jetzt predigen die mit unserem Unglück.", "Beweis.Orden"),
            gossip("Wenigstens weiß es jetzt jeder. Zwei Wochen. Zwei Wochen!", "Beweis.Orden"),
            gossip("Verkauft. Für Gildenmünzen. Mein Bruder liegt unter dem Schlamm.", "Beweis.Gilde"),
            gossip("Man sagt, im Speicher lag etwas. Jetzt liegt es bei der Faktorin.", "Beweis.Gilde"),
            gossip("Edda hat das Siegel in ihrer Truhe. Soll die Gilde nur kommen.", "Beweis.Lager"),
            gossip("Kein Gold, kein Brot. Aber es gehört uns.", "Beweis.Lager"),
        ],
    },
    {
        "asset": "DA_Gossip_Kontor",
        "speaker_name": unreal.Text("Gildenwache"),
        "lines": [
            gossip("Weitergehen. Oder Kontrakte holen. Eins von beiden."),
            gossip("Früher habe ich Kernerz bewacht. Jetzt bewache ich Tränke."),
            gossip("Die Faktorin rechnet nachts. Ich höre die Kugeln klacken."),
            gossip("Ihr habt Nerven, euch hier blicken zu lassen.", "Beweis.Orden"),
            gossip("Die Faktorin sagt, ihr seid in Ordnung. Dann seid ihr es.", "Beweis.Gilde"),
            gossip("Man erzählt, ihr habt etwas, das uns gehört.", "Beweis.Lager"),
        ],
    },
    {
        "asset": "DA_Gossip_Albrun",
        "speaker_name": unreal.Text("Albrun"),
        "min_pause": 18.0,
        "max_pause": 34.0,
        "lines": [
            gossip("Sechs Beine, zwei Drüsen … nein. Albrun zählt noch einmal."),
            gossip("Asche ist auch nur ein sehr gründlicher Zustand von Holz."),
            gossip("Wer Pilze ins Herbarium klebt, sortiert auch Fledermäuse zu den Vögeln."),
            gossip("Dreihundert Jahre, und die Tinte ist immer noch zu dünn."),
        ],
    },
]


def create_gossip():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = 0

    for definition in GOSSIP:
        values = dict(definition)
        name = values.pop("asset")
        path = "{}/{}".format(FOLDER, name)

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log("Vael: gossip {} exists, left untouched".format(path))
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelGossip)

        asset = asset_tools.create_asset(name, FOLDER, unreal.VaelGossip, factory)
        if asset is None:
            unreal.log_error("Vael: gossip {} could not be created".format(path))
            continue

        for key, value in values.items():
            asset.set_editor_property(key, value)

        if unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            created += 1
            unreal.log("Vael: created gossip {}".format(path))

    unreal.log("Vael: {} gossip assets created".format(created))


create_gossip()


def create_dialogues():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = 0

    for definition in DIALOGUES:
        values = dict(definition)
        name = values.pop("asset")
        path = "{}/{}".format(FOLDER, name)

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log("Vael: dialogue {} exists, left untouched".format(path))
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelDialogue)

        dialogue = asset_tools.create_asset(name, FOLDER, unreal.VaelDialogue, factory)
        if dialogue is None:
            unreal.log_error("Vael: dialogue {} could not be created".format(path))
            continue

        for key, value in values.items():
            dialogue.set_editor_property(key, value)

        if unreal.EditorAssetLibrary.save_loaded_asset(dialogue, False):
            created += 1
            unreal.log("Vael: created dialogue {}".format(path))

    unreal.log("Vael: {} dialogue assets created".format(created))


create_dialogues()
