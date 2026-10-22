"""
Feuer-Baukasten: makes the noise picture the fire materials pan and distort (Scripts/Data/Feuer/T_Feuer_Rauschen.png).
Filtered random noise made in frequency space, so it tiles without a seam. Three channels of different grain:
R coarse billows, G middle tongues, B fine flicker. A holds a stretched variant (tall streaks) for the rising sparks and smoke.
Run: blender -b -P feuer_texturen.py -- <output folder>
"""
import bpy, sys, os
import numpy as np

OUT = sys.argv[sys.argv.index("--") + 1]
SIZE = 256
rng = np.random.default_rng(23)


def tiling_noise(falloff, stretch_y=1.0):
    white = rng.standard_normal((SIZE, SIZE))
    spectrum = np.fft.fft2(white)
    fy = np.fft.fftfreq(SIZE)[:, None] * stretch_y
    fx = np.fft.fftfreq(SIZE)[None, :]
    radius = np.sqrt(fx * fx + fy * fy)
    radius[0, 0] = 1.0
    field = np.real(np.fft.ifft2(spectrum / radius ** falloff))
    field -= field.min()
    field /= field.max()
    return field


channels = [tiling_noise(1.9), tiling_noise(1.45), tiling_noise(1.0), tiling_noise(1.4, stretch_y=6.0)]
pixels = np.stack(channels, axis=-1).astype(np.float32)

image = bpy.data.images.new("T_Feuer_Rauschen", SIZE, SIZE, alpha=True, float_buffer=False)
image.colorspace_settings.name = "Non-Color"
image.pixels = pixels.ravel()
os.makedirs(OUT, exist_ok=True)
image.filepath_raw = os.path.join(OUT, "T_Feuer_Rauschen.png")
image.file_format = "PNG"
image.save()
print("FEUER saved", image.filepath_raw)
