# Creates the quests of Act I. A quest that exists already is left untouched, so texts edited in the editor stay.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateQuestAssets.py" -EnablePlugins=PythonScriptPlugin
#
# The chain follows the Lore-Bibel, tab Storyleitfaden, "Akt I im Detail" (Ablauf). Talk targets are dialogue asset names,
# kill targets the KindId of the creature data, reach targets the MarkerId of quest markers (Scripts/PlaceQuestMarkers.py).
# The twist (Gildenspeicher, choice at the end of the act) and the Gildenkontor come in later packages.

import unreal

FOLDER = "/Game/Vael/Story/Quests"
EDDA = "DA_Dialogue_Edda"

Objective = unreal.VaelQuestObjective


def step(objective, kind, target="", count=1, lines=()):
    result = unreal.VaelQuestStep()
    result.set_editor_property("objective", unreal.Text(objective))
    result.set_editor_property("type", kind)
    result.set_editor_property("target", unreal.Name(target))
    result.set_editor_property("count", count)
    result.set_editor_property("lines", [unreal.Text(line) for line in lines])
    return result


QUESTS = [
    {
        "asset": "DA_Quest_Q1_NachDemFieber", "quest_id": "Q1_NachDemFieber", "sort_order": 10, "main_quest": True, "prerequisite": "",
        "title": "Nach dem Fieber", "giver_name": "Edda",
        "summary": "Drei Tage Fieber, und Kesselgrund ist fort. In deinen Händen glüht etwas, das vorher nicht da war.",
        "steps": [
            step("Sprich mit Edda am Lagerfeuer", Objective.TALK, EDDA),
            step("Wirke Formeln", Objective.CAST, count=3),
            step("Kehr zu Edda zurück", Objective.TALK, EDDA, lines=[
                "Es gehorcht dir also. Gut. Dann bist du das Einzige, was zwischen uns und der Brut steht.",
                "Fang auf den Feldern an. Die Kriecher graben sich bis an unsere Zelte heran, und die Harpyien holen sich, was wir liegen lassen.",
            ]),
        ],
    },
    {
        "asset": "DA_Quest_Q2_DieBrut", "quest_id": "Q2_DieBrut", "sort_order": 20, "main_quest": True, "prerequisite": "Q1_NachDemFieber",
        "title": "Die Brut auf den Feldern", "giver_name": "Edda",
        "summary": "Glutkriecher wühlen in der Asche, Aschharpyien kreisen darüber. Solange sie dort sind, wächst auf den Feldern nichts.",
        "steps": [
            step("Besiege Glutkriecher auf den Feldern", Objective.KILL, "Glutkriecher", 6),
            step("Hol Aschharpyien vom Himmel", Objective.KILL, "Aschharpyie", 3),
            step("Erzähl Edda von den Feldern", Objective.TALK, EDDA, lines=[
                "Die Felder sind still. Still ist nicht gut, aber besser als brennend.",
                "Geh ins Dorf. Vielleicht liegt in den Ruinen noch etwas, das uns hilft. Und vielleicht etwas, das einmal dir gehört hat.",
            ]),
        ],
    },
    {
        "asset": "DA_Quest_Q3_Kesselgrund", "quest_id": "Q3_Kesselgrund", "sort_order": 30, "main_quest": True, "prerequisite": "Q2_DieBrut",
        "title": "Die Ruinen von Kesselgrund", "giver_name": "Edda",
        "summary": "Euer Dorf lag in einem warmen Kessel, daher der Name. Was die Flut übrig gelassen hat, steht noch.",
        "steps": [
            step("Geh in die Ruinen von Kesselgrund", Objective.REACH, "Dorf"),
            step("Sieh am alten Brunnen nach", Objective.REACH, "Brunnen"),
            step("Kehr zu Edda zurück", Objective.TALK, EDDA, lines=[
                "Der Brunnen war immer kalt, selbst im Sommer. Die alte Hüterin hat dafür gesorgt, mit einer Formel, von der keiner wusste, woher sie sie hatte.",
                "Im Osten haben Fremde ein Lager aufgeschlagen. Dunkle Kutten, eine Narbe als Zeichen. Sie verteilen Brot und reden viel. Sieh sie dir an.",
            ]),
        ],
    },
    {
        "asset": "DA_Quest_Q4_DieNarbe", "quest_id": "Q4_DieNarbe", "sort_order": 40, "main_quest": True, "prerequisite": "Q3_Kesselgrund",
        "title": "Die Narbe predigt", "giver_name": "Edda",
        "summary": "Der Orden der Narbe wirbt um die Überlebenden: Brot gegen Treue. Wer nicht hören will, bekommt ihre Dornen zu spüren.",
        "steps": [
            step("Sieh dir das Lager der Fremden im Osten an", Objective.REACH, "Orden"),
            step("Wehr dich gegen die Prediger", Objective.KILL, "Prediger", 3),
            step("Berichte Edda vom Orden", Objective.TALK, EDDA, lines=[
                "Sie predigen, die Flut sei eine Strafe gewesen. Die Schlafende habe gerufen, und wir hätten nicht hören wollen.",
                "Und sie flüstern über die Gilde. Gerede vielleicht. Erst die Mutter der Brut, dann alles andere.",
            ]),
        ],
    },
    {
        "asset": "DA_Quest_Q5_DieMutter", "quest_id": "Q5_DieMutter", "sort_order": 50, "main_quest": True, "prerequisite": "Q4_DieNarbe",
        "title": "Die Mutter der Brut", "giver_name": "Edda",
        "summary": "Im Krater, wo die Flut ausbrach, brütet die Glutkönigin. Solange sie lebt, kommt die Brut immer wieder.",
        "steps": [
            step("Geh zum Krater im Nordosten", Objective.REACH, "Krater"),
            step("Besiege die Glutkönigin", Objective.KILL, "Glutkoenigin"),
            step("Kehr zu Edda zurück", Objective.TALK, EDDA, lines=[
                "Du hast sie wirklich getötet. Ohne sie verhungert ihre Brut.",
                "Ich habe ihren Schrei bis hierher gehört. Er klang nicht nach Wut. Er klang, als hätte etwas sehr Großes Schmerzen.",
            ]),
        ],
    },
    {
        "asset": "DA_Quest_S1_DieQuelle", "quest_id": "S1_DieQuelle", "sort_order": 110, "main_quest": False, "prerequisite": "Q2_DieBrut",
        "title": "Die blutende Quelle", "giver_name": "Edda",
        "summary": "Im Osten bricht violettes Licht aus dem Boden. Solange die Quelle offen ist, frisst sie sich weiter ins Land.",
        "steps": [
            step("Frag Edda nach dem Leuchten im Osten", Objective.TALK, EDDA, lines=[
                "Die Quelle im Osten blutet weiter, und mit jedem Tag frisst sie mehr Land.",
                "Gezeichnete Wesen tragen Markkristalle in sich. Bring genug davon zur Quelle, dann zeigt sich, was sie bewacht.",
            ]),
            step("Versiegle die Mark-Quelle im Osten", Objective.EVENT, "QuelleVersiegelt"),
            step("Kehr zu Edda zurück", Objective.TALK, EDDA, lines=[
                "Ich spüre es bis ans Feuer. Das Land atmet wieder, ein bisschen.",
            ]),
        ],
    },
    {
        "asset": "DA_Quest_S2_WasDieAscheHergibt", "quest_id": "S2_WasDieAscheHergibt", "sort_order": 120, "main_quest": False, "prerequisite": "Q1_NachDemFieber",
        "title": "Was die Asche hergibt", "giver_name": "Edda",
        "summary": "Selbst in der Asche wächst noch etwas. Wer weiß, was man daraus machen kann.",
        "steps": [
            step("Sammle Pflanzen, Pilze oder Gestein", Objective.GATHER, count=5),
            step("Zeig Edda, was du gefunden hast", Objective.TALK, EDDA, lines=[
                "Glutsamen, Kelchtau, Stäublinge … Meine Mutter hätte gewusst, was man daraus macht. Ich weiß nur, dass man so etwas nicht wegwirft.",
            ]),
        ],
    },
]


def create_quests():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = 0

    for definition in QUESTS:
        values = dict(definition)
        name = values.pop("asset")
        path = "{}/{}".format(FOLDER, name)

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log("Vael: quest {} exists, left untouched".format(path))
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelQuest)

        quest = asset_tools.create_asset(name, FOLDER, unreal.VaelQuest, factory)
        if quest is None:
            unreal.log_error("Vael: quest {} could not be created".format(path))
            continue

        quest.set_editor_property("quest_id", unreal.Name(values["quest_id"]))
        quest.set_editor_property("sort_order", values["sort_order"])
        quest.set_editor_property("main_quest", values["main_quest"])
        quest.set_editor_property("prerequisite", unreal.Name(values["prerequisite"]))
        quest.set_editor_property("title", unreal.Text(values["title"]))
        quest.set_editor_property("giver_name", unreal.Text(values["giver_name"]))
        quest.set_editor_property("summary", unreal.Text(values["summary"]))
        quest.set_editor_property("steps", values["steps"])

        if unreal.EditorAssetLibrary.save_loaded_asset(quest, False):
            created += 1
            unreal.log("Vael: created quest {}".format(path))
        else:
            unreal.log_error("Vael: quest {} could not be saved".format(path))

    unreal.log("Vael: {} quest assets created".format(created))


create_quests()
