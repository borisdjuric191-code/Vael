# Doc-Nachträge

Änderungen für die Claude-Docs (Aufgaben für Boris, Lore-Bibel, Leitfaden), die im zweiten Claude-Konto entstanden sind.
Das zweite Konto kann die Docs nicht bearbeiten. Die nächste Sitzung im Hauptkonto überträgt jeden Eintrag ins Doc und löscht ihn danach hier.

Format: `- [Datum] Doc → Abschnitt: Eintrag`

## Offen


- [2026-10-28] Lore-Bibel → Bestiarium/Welt (Boris 2026-10-28, gebaut 27e96fa): Gegner wie in Diablo. Im Wurzelforst gibt es keine festen Gruppen der häufigen Gegner mehr: Während die Gruppe läuft, kommt alle 35–55 m (nachts öfter) ein Rudel 30–40 m voraus, außer Sicht, auf trockenem, nicht zu steilem Boden, nie in einem Schutzgebiet. Rudel: Rankenluchse zu dritt (nachts häufiger), Spannhornkäfer zu zweit oder dritt, Schleieraffen allein oder zu zweit (vor allem nachts). Jeder weitere Spieler +35 % Rudelgröße und Obergrenze (18 gleichzeitig allein). Weit Zurückgelassene (90 m) verschwinden. Stehenbleiben bringt nichts, nur Laufen. Feste Orte nur noch für Besonderes (Raupen, Draculaupen an der Nid-Quelle, später Bosse und Nester). Neue Arten (Distelspinne, Pfuhlnatter, Peitschenunke) kommen in die Tabelle DA_Begegnungen_Wurzelforst. Rankenluchs jetzt 1,1 m Schulterhöhe (vorher 0,70 m).
- [2026-10-28] Aufgaben → Testrunde C Spielsysteme: Im Wurzelforst einfach drauflos laufen: kommen regelmäßig Rudel? Zu oft/zu selten (DistancePerPack), zu viele (MaxAlive), zu nah sichtbar aufgetaucht (SpawnDistance)? Alles in DA_Begegnungen_Wurzelforst einstellbar. Cheat `VaelBegegnung` holt sofort ein Rudel. Mit zwei Spielern: größere Rudel? Im Test gesehen: Käfer, Luchse, Käfer erschienen beim Laufen 31–35 m voraus (Log); ein echter Kampf danach nicht im Bild geprüft (die Testfigur blieb an einem Felsen hängen).
- [2026-10-28] Aufgaben → Testrunde A Optik: Rankenluchs auf 1,1 m: sieht man Kamm und Ranken jetzt gut? Laufen und Sprung noch stimmig (Schrittweite mitskaliert)? Im Testbild nur liegend im Hinterhalt gesehen. Stellschraube: BodyScale in den Luchs-Werten (UVaelRankenluchsData).
