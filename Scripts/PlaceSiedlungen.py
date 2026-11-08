# The people of the Aschenmark in L_Vael (Docs/Plan_NPCs_Siedlungen.md), from Scripts/Data/Welt/AM_Siedlungen.json.
#
# Today (step 3): the people already standing in L_Vael ("bestehend" and the story people Edda, Albrun, Maren) get their
# identity (name, origin, role, settlement, age, build, attitude to the Prophet) and the gossip of their settlement.
# Step 5 adds spawning the other people on their places.
# Only the changed actors are saved. Run CreateSettlementGossip.py first.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceSiedlungen.py" -EnablePlugins=PythonScriptPlugin -unattended

import json
import os

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "Scripts", "Data", "Welt", "AM_Siedlungen.json")
LEVEL = "/Game/Vael/Maps/L_Vael"
GOSSIP = "/Game/Vael/Story/Gerede/DA_Gossip_Siedlung_"

PROPHET = {"Anhaenger": unreal.VaelProphetAttitude.FOLLOWER, "Gegner": unreal.VaelProphetAttitude.ENEMY,
           "Zweifler": unreal.VaelProphetAttitude.DOUBTER, "Gleichgueltig": unreal.VaelProphetAttitude.INDIFFERENT,
           "Story": unreal.VaelProphetAttitude.STORY}
AGE = {"erwachsen": unreal.VaelNpcAge.ADULT, "kind": unreal.VaelNpcAge.CHILD, "jung": unreal.VaelNpcAge.YOUNG, "alt": unreal.VaelNpcAge.OLD}
BUILD = {"mittel": unreal.VaelNpcBuild.MEDIUM, "schmal": unreal.VaelNpcBuild.SLIM, "kraeftig": unreal.VaelNpcBuild.STRONG,
         "klein": unreal.VaelNpcBuild.SLIM}


def log(message):
    unreal.log_warning("[PlaceSiedlungen] " + message)


def identity(settlement, person):
    return unreal.VaelNpcIdentity(
        name=unreal.Text(person["name"]), culture=unreal.Name(person.get("kultur", "")), role=unreal.Name(person["rolle"]),
        faction=unreal.Name(settlement["fraktion"]), settlement=unreal.Name(settlement["id"]),
        settlement_name=unreal.Text(settlement["name"]), age=AGE[person["alter"]], build=BUILD[person["koerper"]],
        prophet=PROPHET[person["prophet"]])


def main():
    data = json.load(open(DATA, encoding="utf-8"))
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
        log("could not load " + LEVEL)
        return
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])

    npcs = {}
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(actor, unreal.VaelNpc):
            npcs.setdefault(actor.get_actor_label(), []).append(actor)
    log("people in the level: " + ", ".join(sorted(npcs)))

    changed = []
    for settlement in data["siedlungen"]:
        gossip = unreal.load_asset(GOSSIP + settlement["gerede"])
        for person in settlement["leute"]:
            label = person.get("bestehend") or (person["name"] if person["prophet"] == "Story" else None)
            if label is None:
                continue  # placed in step 5
            for actor in npcs.get(label, []):
                actor.modify()
                actor.set_editor_property("identity", identity(settlement, person))
                if person.get("bestehend") and gossip is not None:
                    actor.set_editor_property("gossip", gossip)
                changed.append(actor)
                log("{} -> {} ({}, {}, {})".format(label, person["name"], person["rolle"], settlement["id"], person["prophet"]))
            if label not in npcs:
                log("not found in the level: " + label)

    packages = []
    for actor in changed:
        package = actor.get_package() if hasattr(actor, "get_package") else actor.get_outermost()
        if package not in packages:
            packages.append(package)
    saved = unreal.EditorLoadingAndSavingUtils.save_packages(packages, True)
    log("{} people updated, {} packages saved ({})".format(len(changed), len(packages), saved))


main()
