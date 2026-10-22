"""
Feuer-Baukasten: sets the coverage (alpha) of finished fire flipbooks again from their brightness, the same way
feuer_simulation.py does, without simulating them anew. Faint glow below the threshold gets no coverage.
Run: blender -b -P feuer_deckkraft.py -- <flipbook png> [<flipbook png> ...]
"""
import bpy, sys
import numpy as np

for path in sys.argv[sys.argv.index("--") + 1:]:
    image = bpy.data.images.load(path)
    w, h = image.size
    pixels = np.array(image.pixels[:], dtype=np.float32).reshape(h, w, 4)
    pixels[:, :, 3] = np.clip((pixels[:, :, :3].max(axis=2) - 0.07) * 2.2, 0.0, 1.0)
    image.pixels = pixels.ravel()
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    print("DECKKRAFT", path)
