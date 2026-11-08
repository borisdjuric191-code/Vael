# Gerede of the settlements (Docs/Plan_NPCs_Siedlungen.md, step 3): what the people of the Aschenmark mutter.
#
# - DA_Gossip_Leute: lines all people share. Each line belongs to an attitude to the Prophet (the group with the glowing
#   hands, risen from the fever), to a role, or to nobody in particular (Strandgut der Welt: families from many shores).
# - DA_Gossip_Siedlung_<Gerede>: the lines of one settlement ("gerede" in AM_Siedlungen.json); includes DA_Gossip_Leute
#   and, where the place had gossip before (Lager, Kontor), that older asset, which stays untouched.
# Placeholders: {Name} the speaker, {Ort} their settlement, {Nachbar} someone else living there.
# These assets belong to this script: rerunning rewrites them.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateSettlementGossip.py" -EnablePlugins=PythonScriptPlugin

import unreal

FOLDER = "/Game/Vael/Story/Gerede"
OLD = "/Game/Vael/Story"
A = unreal.VaelProphetAttitude


def line(text, role="", prophet=A.NONE, flag=""):
    return unreal.VaelGossipLine(line=unreal.Text(text), role=unreal.Name(role), prophet=prophet, required_flag=unreal.Name(flag))


def roles(role, *texts):
    return [line(t, role=role) for t in texts]


# ---------------------------------------------------------------- shared by all people

PROPHET = [
    # Followers: hope, rumour, devotion
    line("Seht nur! Die mit den glühenden Händen!", prophet=A.FOLLOWER),
    line("Drei Tage im Fieber und dann aufgestanden. So steht es in keinem Buch, und doch ist es wahr.", prophet=A.FOLLOWER),
    line("Ich habe {Nachbar} gesagt: Wenn sie vorbeikommen, verbeug dich. Man weiß nie.", prophet=A.FOLLOWER),
    line("Die Asche wird weichen. Ich spüre es, seit sie da sind.", prophet=A.FOLLOWER),
    line("Ich habe eine Kerze für euch angezündet. Die letzte, die ich hatte.", prophet=A.FOLLOWER),
    line("Meine Mutter erzählte von einer Prophezeiung aus ihrer alten Heimat. Jetzt weiß ich, wen sie meinte.", prophet=A.FOLLOWER),
    line("Sie gehen in die Felder, wo sonst nur die Brut geht. Wer das wagt, kann auch mehr.", prophet=A.FOLLOWER),
    # Enemies: fear, scorn, warning
    line("Geht weiter, Falsche. Leuchtende Hände machen niemanden heilig.", prophet=A.ENEMY),
    line("Seit sie hier sind, glüht das Nid heller. Zufall? Ich glaube nicht an Zufälle.", prophet=A.ENEMY),
    line("{Nachbar} betet schon zu ihnen. Bald betet das ganze {Ort}.", prophet=A.ENEMY),
    line("Propheten. Das letzte Mal, als einer kam, brannte unser Hafen.", prophet=A.ENEMY),
    line("Haltet die Kinder fern. Was so leuchtet, verbrennt.", prophet=A.ENEMY),
    line("Ihr wollt Glauben? Bringt erst Brot.", prophet=A.ENEMY),
    # Doubters: want to believe, don't dare
    line("Wenn sie es wirklich sind … nein. Erst will ich es sehen.", prophet=A.DOUBTER),
    line("Vielleicht sind sie nur Leute, die Glück hatten. Vielleicht ist Glück dasselbe.", prophet=A.DOUBTER),
    line("{Nachbar} schwört auf sie. Ich schwöre auf gar nichts mehr.", prophet=A.DOUBTER),
    line("Ich würde gern glauben. Aber Glauben hat uns schon einmal ertränkt.", prophet=A.DOUBTER),
    line("Zeigt mir ein Feld, das wieder grün wird. Dann reden wir.", prophet=A.DOUBTER),
    # Indifferent: other worries
    line("Prophet, Gilde, Orden. Hauptsache, jemand repariert das Dach.", prophet=A.INDIFFERENT),
    line("Mir egal, wer leuchtet. Ich hab Hunger.", prophet=A.INDIFFERENT),
    line("Heute Wasser holen, morgen Wasser holen. Das ist mein Glaube.", prophet=A.INDIFFERENT),
    line("Wenn ihr eh hier steht: Habt ihr was zu tauschen?", prophet=A.INDIFFERENT),
]

STRANDGUT = [
    line("Mein Großvater kam mit einem Wrack an diese Küste. Er hat nie gesagt, woher. Nur, dass dort das Meer warm war."),
    line("Auf Vael ist jeder von irgendwo. Die Asche fragt nicht danach."),
    line("{Nachbar} kocht wie bei uns zu Hause. Dabei kommen wir von zwei verschiedenen Enden der Welt."),
    line("Ich träume noch in der Sprache meiner Mutter. Wach spreche ich nur noch diese hier."),
    line("Mein Name klingt hier fremd, sagt man. Er ist das Einzige, das ich vom Schiff gerettet habe."),
    line("Die Stürme bringen Holz, Fässer und manchmal Menschen. Wir nehmen, was kommt."),
]

ROLES = (
    roles("Wache", "Augen auf. Die Brut kommt, wenn man blinzelt.", "Keine Waffen gezogen innerhalb der Pfähle. Auch nicht leuchtende.")
    + roles("Bauer", "Asche ist fruchtbar, sagen sie. In hundert Jahren.", "Drei Reihen gesät, zwei aufgegangen. Ein guter Tag.")
    + roles("Haendler", "Alles hat seinen Preis. Hoffnung ist gerade teuer.", "Gildenmünze oder Tausch. Lächeln nehme ich nicht.")
    + roles("Kind", "Darf ich mal deine Hand anfassen? Leuchtet die auch nachts?", "{Nachbar} sagt, unter der Asche schläft ein Drache. Stimmt das?")
    + roles("Alte", "Ich hab drei Fluten gesehen. Diese war die gierigste.", "Früher roch es hier nach Brot. Jetzt nach Schwefel und Sorgen.")
    + roles("Koch", "Suppe aus Wurzeln und Geduld. Mehr gibt's nicht.", "Wer jagt, isst zuerst. So ist die Regel.")
    + roles("Heiler", "Brandwunden, Brandwunden, Brandwunden. Bringt mir Taublatt, wenn ihr welches findet.", "Fieber geht um. Ihr wisst, wie das endet. Oder anfängt.")
    + roles("Handwerker", "Nägel sind knapp. Ich ziehe sie aus den Ruinen.", "Gebt mir Holz, das nicht verkohlt ist, und ich baue euch ein Haus.")
    + roles("Schmied", "Das Eisen hier ist voller Schlacke. Man muss es dreimal falten.", "Bringt mir Obsidian, und ich mache euch eine Klinge, die singt.")
    + roles("Pilger", "Ich bin über das Meer gekommen, um die Narbe zu sehen. Jetzt bleibe ich.", "Meine Füße sind wund, aber mein Herz ist leicht.")
    + roles("Prediger", "Die Narbe ist eine Wunde der Welt. Wir halten sie geschlossen.", "Wer das Nid berührt, wird von ihm berührt. Merkt euch das.")
    + roles("Jaeger", "Glutkriecher schmecken nach Holzkohle. Man gewöhnt sich dran.", "Die Panzer verkaufe ich der Gilde. Das Fleisch esse ich selbst.")
    + roles("Fallensteller", "Tritt nicht dahin. Und nicht dahin. Am besten bleibst du stehen.", "Die Brut ist dumm, aber zahlreich. Fallen sind geduldiger als ich.")
    + roles("Gerber", "Glutkriecherhaut hält Feuer ab. Und riecht dafür ewig.", "Gebt mir Felle, ich gebe euch Rüstung. Einfacher Handel.")
    + roles("Arbeiter", "Schlacke schaufeln. Jeden Tag Schlacke schaufeln.", "Die Gilde zahlt pünktlich. Das ist das Beste, was man über sie sagen kann.")
    + roles("Vorarbeiter", "Wer trödelt, schaufelt doppelt.", "Hinter der Halde liegt ein Tor, das niemand öffnen soll. Fragt nicht.")
    + roles("Traeger", "Kisten rauf, Kisten runter. Was drin ist? Nicht mein Geschäft.", "Mein Rücken gehört der Gilde. Mein Kopf noch mir.")
    + roles("Schreiber", "Jede Münze steht in diesem Buch. Jede.", "Wenn ihr Kontrakte wollt: Name, Zeichen, Daumenabdruck.")
    + roles("Fuhrmann", "Der Pass ist ruhig heute. Das gefällt mir nicht.", "Zwei Tage vom Kontor zur Passwache, wenn die Achse hält.")
    + roles("Bote", "Ich renne schneller als jede Nachricht. Meistens.", "Die Hauptfrau will wissen, wer ihr seid. Ich sag's ihr später.")
    + roles("Hirte", "Die Ziegen riechen das Nid, bevor ich es sehe.", "Der Wind dreht. Das heißt nie etwas Gutes.")
    + roles("Windleser", "Der Wind kommt aus dem Wald. Er riecht nach Moos und nach etwas Altem.", "Heute flüstert der Pass. Morgen wird er schreien.")
    + roles("Einsiedler", "Die Königin singt nachts. Nur ich höre noch hin.", "Ich lebe allein, weil die Felsen nicht lügen.")
    + roles("Brunnenhueterin", "Ich habe den Brunnen kühl gehalten, als alles kochte. Das hat mich die Akademie gelehrt.", "Frostlanze in den Keller, jede Nacht. Sonst verdirbt das Letzte, was wir haben.")
    + roles("Pluenderer", "Was in den Ruinen liegt, gehört den Lebenden.", "Schau nicht so. Du hättest es auch genommen.")
    + roles("Anfuehrer", "Haltet die Linie. Ein Pass fällt nicht von außen, sondern von innen.", "Wer hier Wache steht, schläft mit offenen Augen.")
    + roles("Gelehrter", "Interessant. Wirklich sehr interessant.", "Ich notiere das. Ich notiere alles.")
    + roles("Heiler", "Zeig her. Nein, das muss genäht werden.")
)

# ---------------------------------------------------------------- the settlements

SETTLEMENTS = {
    "Lager": ("Überlebende", [
        line("Im {Ort} schläft niemand ruhig. Aber wir schlafen."),
        line("{Nachbar} hat heute Holz gebracht. Das Feuer brennt die Nacht durch."),
    ], "DA_Gossip_Lager"),
    "Dorf": ("Kesselgrunder", [
        line("Das war mein Haus. Jetzt ist es ein Ofen ohne Dach."),
        line("Ich gehe nicht ins Lager. Hier sind meine Toten, hier bleibe ich."),
        line("Der Brunnen ist das Einzige, was noch kalt ist in Kesselgrund."),
        line("Nachts hört man es unter der Asche knacken. Als würde der Grund atmen."),
    ], None),
    "Hof": ("Hofleute", [
        line("Jemand muss säen, auch wenn keiner erntet."),
        line("Die Asche hält die Wärme. Vielleicht wächst hier bald etwas, das sonst nirgends wächst."),
        line("Wir sind geblieben, als alle gingen. Das Feld war unser."),
    ], None),
    "Kontor": ("Gildenleute", [
        line("Die Gilde vergisst nichts. Das ist Versprechen und Drohung zugleich."),
        line("Kontrakte hängen am Brett. Lest das Kleingedruckte."),
    ], "DA_Gossip_Kontor"),
    "Strasse": ("Fuhrleute", [
        line("Halt hier, Pause. Wer weiß, was hinter der nächsten Kurve brütet."),
        line("Die Räder knirschen in der Asche wie Zähne."),
        line("Vom {Ort} bis zum Pass und zurück. Seit drei Monden."),
    ], None),
    "Schlacke": ("Schlackengräber", [
        line("Die Halden wachsen. Manchmal rollt etwas herunter, das nicht von selbst rollen sollte."),
        line("Im Obsidian sieht man sein Gesicht. Ich schaue nicht mehr hin."),
        line("Die Gilde will den Bruch leer haben, bevor der Winter kommt. Fragt nicht, warum."),
    ], None),
    "Jagd": ("Glutjäger", [
        line("Die Nester rücken näher. Letzten Mond waren sie noch hinter dem Hügel."),
        line("Wir jagen die Brut, die Gilde kauft die Panzer. Jeder lebt von der Plage."),
        line("Nachts glühen die Felder. Schön, wenn man nicht weiß, was es ist."),
    ], None),
    "Einsiedlerin": ("Einsiedlerin", [
        line("Die Harpyien lassen mich in Ruhe. Wir verstehen uns."),
        line("Die Königin war einmal ein Käfer wie alle anderen. Dann trank sie aus der Flut."),
        line("Geht nicht in den Krater, bevor ihr ihr Lied kennt."),
    ], None),
    "Orden": ("Orden der Narbe", [
        line("Die Quelle bleibt versiegelt. Für immer, wenn es nach uns geht."),
        line("Der Orden war vor der Flut hier. Der Orden wird nach ihr hier sein."),
        line("Kniet, wenn ihr die Kathedrale betretet. Oder bleibt draußen."),
    ], None),
    "Passwache": ("Passwache", [
        line("Niemand geht nach Norden. Befehl der Hauptfrau."),
        line("Hinter dem Tor wächst Moos. Moos! Hier, wo nichts wächst."),
        line("Wir bewachen einen Pass, den niemand benutzt. Noch nicht."),
    ], None),
    "Predigerlager": ("Pilger", [
        line("Die Prediger sagen, das Nid ist eine Prüfung. Ich sage, es ist ein Gift."),
        line("Wir sind aus allen Ecken der Welt hier. Die Narbe ruft jeden."),
        line("{Nachbar} betet seit drei Tagen ohne Pause. Ich bringe Wasser."),
    ], None),
    "Windkante": ("Windkante", [
        line("Von hier oben sieht man das ganze Elend. Und manchmal das Meer."),
        line("Der Wind kennt alle Namen. Er sagt sie nur nicht laut."),
    ], None),
}


def write_gossip(name, speaker, lines, includes=(), range_cm=450.0):
    path = "{}/{}".format(FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelGossip)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.VaelGossip, factory)
    asset.set_editor_property("speaker_name", unreal.Text(speaker))
    asset.set_editor_property("lines", lines)
    asset.set_editor_property("includes", [i for i in includes if i is not None])
    asset.set_editor_property("range", range_cm)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


def main():
    common = write_gossip("DA_Gossip_Leute", "Leute", PROPHET + STRANDGUT + ROLES)
    for key, (speaker, lines, old) in SETTLEMENTS.items():
        older = unreal.load_asset("{}/{}".format(OLD, old)) if old else None
        write_gossip("DA_Gossip_Siedlung_" + key, speaker, lines, includes=[older, common])
    unreal.log_warning("[CreateSettlementGossip] DA_Gossip_Leute ({} lines) and {} settlements written".format(
        len(PROPHET) + len(STRANDGUT) + len(ROLES), len(SETTLEMENTS)))


main()
