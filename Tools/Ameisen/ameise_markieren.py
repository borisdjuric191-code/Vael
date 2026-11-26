"""
Zeichnet die Mulden aus ansichten.json als nummerierte Kreise in die Prüfbilder (Python mit Pillow, nicht Blender).

  python ameise_markieren.py <Ordner von ameise_ansichten.py>
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ORDNER = sys.argv[1]
with open(os.path.join(ORDNER, "ansichten.json"), encoding="utf-8") as f:
    info = json.load(f)
try:
    font = ImageFont.truetype("arial.ttf", 22)
except OSError:
    font = ImageFont.load_default()
for name, punkte in info.items():
    img = Image.open(os.path.join(ORDNER, name + ".png")).convert("RGB")
    dr = ImageDraw.Draw(img)
    for p in punkte:
        if not p["frei"]:
            continue
        x, y, r = p["x"], p["y"], p["r"]
        dr.ellipse((x - r, y - r, x + r, y + r), outline=(255, 40, 40), width=3)
        dr.text((x + r * 0.7, y - r * 0.9), str(p["i"]), fill=(255, 255, 80), font=font, stroke_width=2,
                stroke_fill=(0, 0, 0))
    img.save(os.path.join(ORDNER, name + "_markiert.png"))
print("markiert:", ", ".join(info))
