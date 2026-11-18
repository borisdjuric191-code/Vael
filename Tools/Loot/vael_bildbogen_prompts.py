"""
Vael - Bildbogen fuer gemalte Itembilder (Boris 17.11., Referenz: dunkle, gemalte Itembilder mit viel Eigenheit)
=================================================================================================================

Schreibt die Auftraege fuer die Bildgenerierung (Nano Banana in Tripo, freie Bilder): je Bogen ein 2 x 2-Raster aus vier
Materialien einer Gruppe, jedes mit einer Bildbeschreibung aus vael_bildbogen_motive.py, auf flachem Chroma-Hintergrund
(gruen, bei gruenen Dingen magenta), damit vael_bildbogen_schneiden.py sie freistellen kann. Mit 3 x 3 brachte das Modell
die Reihenfolge durcheinander; vier pro Bogen trifft es und gibt 512 px je Bild.

Aufruf (Python 3):
  python vael_bildbogen_prompts.py <material_list.json> <bogen.json>
"""
import colorsys
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vael_bildbogen_motive import MOTIVE, POTIONS  # noqa: E402

GROUPS = {
    "Ore": ["Funkenerz", "Kernerz", "Rosteisen", "Drahtsilber", "Kobaltbluete", "Gruenspan", "Rippenspat", "Siechglas", "Salzspat",
            "Schwefelkies", "Blutzinnober", "Glutstein", "Schlacke", "Russquarz", "Nidkristall", "GelaeuterterKristall", "Mondtropfen"],
    "Workstuff": ["GehaertetePlatte", "Schlackeneisen", "Messingblech", "Russglas", "Schlackenglas", "Latex", "SchwarzerLatex",
                  "Siegelwachs", "Ordenstuch", "Ordenssiegel", "Harpyientand"],
}
GREEN = {"Gruenspan", "Siechglas", "Hainblatt", "Rankenfaser", "Rankenkammblatt", "RosenherzNektar", "Kelchschleim", "Verdauungssaeure",
         "Samen_Hainschoessling", "Samen_Schlotwinde", "Samen_Russrose", "Trank_Giftphiole", "Trank_Hainruf", "Trank_Ermattungsfluch",
         "Trank_Saeurewolke", "Trank_Rindenhaut", "Trank_Daemmergarten", "Eiterperle", "Glimmblatt", "Schwelblatt", "Kelchkapsel"}

STYLE = (
    "A 2x2 grid of four separate dark-fantasy game inventory item icons, each item alone and centred in its own equal square "
    "quarter of the image, with clear empty space around every item. Hand-painted digital painting, very high detail and strong "
    "individual character: worn, weathered, handmade looking materials (scratches, chipped edges, grime, rust, cracks, glowing "
    "embers or magic only where described), dramatic warm key light from the upper left, a bright rim light along the edges, deep "
    "shadows, rich saturated accent colours, three-quarter view from slightly above, the item filling about 80 percent of its "
    "quarter. No frames, no borders, no text, no labels, no numbers, no ground, no cast shadow. "
    "The whole background is one perfectly flat, uniform, bright pure {bg} chroma-key colour ({hex}), like a {bg} screen, with no "
    "gradient, no texture, no vignette and no lines between the quarters. "
    "The four items:\n"
)
PLACES = ["top left", "top right", "bottom left", "bottom right"]


def main():
    entries = json.load(open(sys.argv[1], encoding="utf-8"))
    names = [e["asset"].replace("DA_Material_", "") for e in entries]
    order = []
    for group in ("Ore", "Workstuff"):
        order += [n for n in GROUPS[group] if n in names]
    order += [n for n in names if n not in order and n in MOTIVE and not n.startswith("Samen_") and n != "Ueberraschungssaat"]
    order += [n for n in names if n not in order and n in MOTIVE]
    potions = [n for n in names if n in POTIONS]
    missing = [n for n in names if n not in MOTIVE and n not in POTIONS]
    if missing:
        print("ohne Motiv:", missing)
    sheets = []

    colours = {e["asset"].replace("DA_Material_", ""): e["color"] for e in entries}
    keys = (("green", "#00FF00", 120.0), ("magenta", "#FF00FF", 300.0), ("blue", "#0000FF", 240.0))

    def pick_key(chunk):
        """The key colour farthest in hue from every item of the sheet (green things count as green whatever their asset says)"""
        hues = []
        for n in chunk:
            r, g, b = colours.get(n, (0.5, 0.5, 0.5))
            h, l, sat = colorsys.rgb_to_hls(r, g, b)
            if sat > 0.15:
                hues.append(h * 360.0)
            if n in GREEN:
                hues.append(120.0)
            words = (MOTIVE.get(n) or POTIONS.get(n) or "").lower()
            for word, hue in (("green", 120.0), ("turquoise", 175.0), ("blue", 230.0), ("violet", 280.0), ("purple", 280.0),
                              ("pink", 320.0), ("magenta", 300.0), ("lavender", 270.0)):
                if word in words:
                    hues.append(hue)
        best = max(keys, key=lambda k: min([min(abs(k[2] - h), 360.0 - abs(k[2] - h)) for h in hues] or [180.0]))
        return best

    def add(chunk, potion):
        key = pick_key(chunk)
        sheet_id = "Bogen_{:02d}".format(len(sheets) + 1)
        text = sheet_id + ". " + STYLE.format(bg=key[0], hex=key[1])
        for place, n in zip(PLACES, chunk):
            if potion:
                text += "{}: a potion bottle: {}; thick hand-blown glass with small bubbles and refraction, the liquid glowing from inside, " \
                        "a cork, wax or wire seal, a small tag or metal fitting.\n".format(place, POTIONS[n])
            else:
                text += "{}: {}.\n".format(place, MOTIVE[n])
        sheets.append({"id": sheet_id, "background": key[0], "items": chunk, "prompt": text})

    for start in range(0, len(order), 4):
        add(order[start:start + 4], False)
    for start in range(0, len(potions), 4):
        add(potions[start:start + 4], True)
    json.dump(sheets, open(sys.argv[2], "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print(len(sheets), "Boegen")


main()
