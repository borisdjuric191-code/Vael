# Vael – Projektgedächtnis für Claude Code

Isometrisches Action-RPG (Diablo / Path of Exile 2 als Vorbild) in **Unreal Engine 5, C++**.
Plattform: PC und Konsole, **lokaler Koop für 1–4 Spieler** an einem Bildschirm, Controller zuerst.
Der Entwickler (Boris) arbeitet oft nur über das Handy mit dir (Remote Control). Antworte ihm auf **Deutsch**, kurz und klar. Code, Klassennamen und Kommentare auf **Englisch**.

## Welt in drei Sätzen
Die Welt Vael ist der Körper eines uralten Wesens, der **Schlafenden**. Menschen haben auf der Jagd nach Kernerz eine Wunde in ihr Herz gerissen; ihr Blut, das **Mark**, sickert an die Oberfläche und verdirbt Land, Tiere und Menschen. Grundton: düster und brutal, die Umwelt ist das zentrale System für alle Klassen.

Die vollständige Lore und alle Designentscheidungen stehen in der **Lore-Bibel** (Tabs: Lore-Bibel, Bestiarium, Klangbibel). Wenn eine Designfrage offen ist: nachfragen statt erfinden.

## Kernsysteme (Kurzfassung)
- **Vier Klassen / Urkräfte:** Magier (Geist, blau), Tamer (Seele, grün), Ingenieur (Geschick, rot), Prophet (Glaube, gelb).
- **Mark-Versuchung:** Jede Klasse kann über Mark stärker werden, wird dadurch sichtbar verdorben und erhöht die Verderbnis der Region. Der reine Weg erreicht dieselbe Spitzenstärke über Können.
- **Verderbnis-Grad pro Region:** verändert Gegner, Beute, Wetter und Musik.
- **Magier:** Elemente Feuer, Wasser, Erde, Luft + Mark (ab Akt III). Start mit 3 Slots (55 Kombinationen), Slot 4–5 über den Skillbaum. Nur ~15 Formeln durch Experimentieren lernbar; versiegelte Formeln erzeugen ein **Echo** mit Hinweis auf den Fundort. Umwelt-Elemente (Wasser, Feuer, Fels, Sturm in der Nähe) sind billiger und stärker. Schnellformeln: 4 Plätze, mehr Mana + Abklingzeit.
- **Reaktionen:** nass + Blitz = doppelt, gefroren + Erde = zerschmettern, Wind verbreitet Feuer, Wasser löscht Feuer.
- **Tamer:** bindet besiegte Kreaturen (keine Menschen) über Fangbedingungen; Bosse über Rituale. Eigener Kampf über Befehle und Seelenentladung.
- **Ingenieur:** Vorrichtung platzieren + mit Munition zünden; Geräte aus Monster-Materialien; Waffenlinie Schleuder → Bogen → Armbrust → Gewehr; knappe Spezialmunition, Feldcrafting.
- **Prophet:** Licht und Schatten als **zwei getrennte Speicher** (nie eine Waage), Zwielicht als dritter Weg, Gelübde am Ende von Akt II. Mönch → Aura → manifestierte Waffen; Flüche, Segen, Weissagungen.
- **Skillbaum (Adernbaum):** ein gemeinsamer passiver Baum für alle Klassen, vier Viertel, Mark im Zentrum; ~300–400 Knoten zum Start.
- **Inventar:** feste Bereiche (Ausrüstung, Rucksack 1 Feld pro Teil, Materialbeutel ohne Limit, Klassenfach). Persönliche Beute pro Spieler im Koop (~80 % für die eigene Klasse).

## Architekturregeln
- **Logik in C++, Blueprints nur für Feintuning, Daten und Optik.** Blueprints (.uasset) sind binär und für dich nicht bearbeitbar; neue Spiellogik gehört in C++-Klassen, die Boris im Editor per Blueprint ableiten kann.
- **Gameplay Ability System (GAS)** für Zauber, Fähigkeiten, Status-Effekte (nass, brennend, gefroren …) und Attribute (Leben, Mana, Verderbnis).
- **Enhanced Input** für alle Eingaben; jede Aktion muss mit Controller funktionieren.
- **Lokaler Multiplayer** über mehrere lokale Spieler (`UGameInstance::CreateLocalPlayer`), eine gemeinsame isometrische Kamera, die alle Spieler im Bild hält.
- **Datengetrieben:** Formeln, Kreaturen, Beute und Werte in DataAssets oder DataTables, nicht hart im Code.
- Klassen-Präfix `Vael` (z. B. `AVaelCharacter`, `UVaelElementComponent`). Ordner unter `Source/Vael/` nach System (`Magic/`, `Creatures/`, `Combat/`, `Player/`, `UI/` …).
- Unreal-Coding-Standard von Epic einhalten (UPROPERTY/UFUNCTION, `TObjectPtr`, keine rohen `new`).

## Arbeitsweise
1. **Nach jeder C++-Änderung kompilieren** (Editor-Ziel `VaelEditor`, Win64, Development) und alle Fehler beheben, bevor du fertig meldest.
2. **Kleine, saubere Git-Commits** mit kurzer deutscher Nachricht; nach jeder abgeschlossenen Aufgabe committen und pushen.
3. **Niemals** Dateien in `Content/`, `.uasset` oder `.umap` direkt verändern oder löschen. Dinge, die im Editor passieren müssen, als kurze Schritt-für-Schritt-Anleitung an Boris schreiben.
4. Nichts löschen, keine Engine-Dateien ändern, keine Pakete installieren, ohne vorher zu fragen.
5. Sparsam mit dem Kontingent umgehen: keine riesigen Logs komplett einlesen, nur die relevanten Fehlerzeilen. Bei Kompilierfehlern zuerst die erste Fehlermeldung beheben.
6. Am Ende jeder Aufgabe in 2–4 Sätzen melden: was gebaut wurde, ob es kompiliert, was Boris im Editor testen soll.
7. Der PC hat derzeit 16 GB RAM (32 GB sind bestellt): Kompilieren nicht parallel zum offenen Editor erzwingen, wenn es eng wird.

## Erster Meilenstein: „Aschenmark spielbar“
Ziel: Der Browser-Prototyp (Magier, Aschenmark, Glutkönigin, Koop) in Unreal nachgebaut, mit Platzhalter-Grafik.
1. Projektgrundlage: Git, LFS, Kompilierung, GAS-Plugin aktiv, Ordnerstruktur.
2. Isometrische Kamera + Bewegung mit Controller und Tastatur, lokaler Koop (Beitreten/Verlassen per Controller).
3. Element-Warteschlange (3 Slots) und Formelsystem als DataAsset; die 4 Grundzauber.
4. Reaktionen und Status-Effekte über GAS; Umwelt-Elemente.
5. Gegner: Glutkriecher, Aschharpyie, Prediger; Boss Glutkönigin.
6. HUD: Leben, Mana, Elementlegende mit Controller-Symbolen, Grimoire.
7. Wetter und Verderbnis-Grad der Region.
