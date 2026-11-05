# Tusche-Stil für Blender-Bilder (Boris 2026-11-05, Vorbilder: botanische Traktat-Seite, Glas im Astrahmen, Alchemie-Skizze):
# gezeichnet statt gerendert. Dicke dunkle Tuschelinien (Freestyle) um alle Formen, Flächen in wenigen Aquarell-Stufen
# (Toon), Schatten mit Schraffur, dazu Glanzlichter bleiben leuchtend. Wird nach dem Aufbau einer Szene angewandt:
#
#   import vael_tusche as T
#   T.ink(scene, line=2.4)          # Freestyle-Linien
#   T.toon_all()                     # alle Principled-Materialien in Toon + Schraffur umbauen
#   T.no_lines(obj)                  # ein Objekt ohne Linie (Text, Lichthöfe)

import bpy

INK = (0.045, 0.028, 0.016)
_NO_LINES = "OhneLinie"


def no_lines(obj):
    """Ein Objekt bekommt keine Tuschelinie (Schrift, Lichthöfe, Lasuren)"""
    coll = bpy.data.collections.get(_NO_LINES)
    if coll is None:
        coll = bpy.data.collections.new(_NO_LINES)
        bpy.context.scene.collection.children.link(coll)
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    coll.objects.link(obj)
    return obj


def ink(scene, line=2.4, color=INK):
    scene.render.use_freestyle = True
    scene.render.line_thickness_mode = "ABSOLUTE"
    scene.render.line_thickness = line
    layer = scene.view_layers[0]
    layer.use_freestyle = True
    settings = layer.freestyle_settings
    settings.crease_angle = 2.2
    for ls in list(settings.linesets):
        settings.linesets.remove(ls)
    ls = settings.linesets.new("Tusche")
    ls.select_by_visibility = True
    ls.select_by_edge_types = True
    ls.select_silhouette = True
    ls.select_border = True
    ls.select_crease = True
    ls.select_external_contour = True
    coll = bpy.data.collections.get(_NO_LINES)
    if coll is not None:
        ls.select_by_collection = True
        ls.collection = coll
        ls.collection_negation = "EXCLUSIVE"
    style = ls.linestyle
    style.color = color
    style.thickness = line
    # Strich wie mit der Feder: an- und abschwellend, leicht zitternd
    mod = style.thickness_modifiers.new("Feder", "ALONG_STROKE")
    mod.mapping = "CURVE"
    curve = mod.curve
    pts = curve.curves[0].points
    pts[0].location = (0.0, 0.35)
    pts[-1].location = (1.0, 0.35)
    pts.new(0.5, 1.0)
    curve.update()
    style.geometry_modifiers.new("Zittern", "PERLIN_NOISE_1D").amplitude = 0.8
    style.caps = "ROUND"


def _base_color_socket(nt, bsdf):
    inp = bsdf.inputs["Base Color"]
    return inp.links[0].from_socket if inp.links else None, tuple(inp.default_value)


def toon_all(bands=(0.42, 0.72, 1.0), hatch=True):
    """Jedes Principled-Material wird zu: Licht → Stufen → mal Grundfarbe, Schatten schraffiert, Leuchten bleibt"""
    for m in bpy.data.materials:
        if not m.use_nodes or m.name.startswith("Lasur"):
            continue
        nt = m.node_tree
        bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
        out = next((n for n in nt.nodes if n.type == "OUTPUT_MATERIAL" and n.is_active_output), None)
        if bsdf is None or out is None or not out.inputs["Surface"].links:
            continue
        if out.inputs["Surface"].links[0].from_node != bsdf:
            continue
        if bsdf.inputs["Transmission Weight"].default_value > 0.5:
            # Glas: fast durchsichtig, leicht getönt
            glass = nt.nodes.new("ShaderNodeBsdfTransparent")
            glass.inputs["Color"].default_value = (0.85, 0.95, 0.9, 1)
            nt.links.new(glass.outputs[0], out.inputs["Surface"])
            continue
        color_link, color_value = _base_color_socket(nt, bsdf)
        diffuse = nt.nodes.new("ShaderNodeBsdfDiffuse")
        to_rgb = nt.nodes.new("ShaderNodeShaderToRGB")
        nt.links.new(diffuse.outputs[0], to_rgb.inputs[0])
        bw = nt.nodes.new("ShaderNodeRGBToBW")
        nt.links.new(to_rgb.outputs["Color"], bw.inputs[0])
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.interpolation = "CONSTANT"
        els = ramp.color_ramp.elements
        els[0].position, els[0].color = 0.0, (bands[0],) * 3 + (1,)
        els[1].position, els[1].color = 0.12, (bands[1],) * 3 + (1,)
        third = els.new(0.45)
        third.color = (bands[2],) * 3 + (1,)
        nt.links.new(bw.outputs[0], ramp.inputs["Fac"])
        shade = ramp.outputs["Color"]
        if hatch:
            # Schraffur in den Schatten: schräge Linien im Bildraum, nur wo es dunkel ist
            coord = nt.nodes.new("ShaderNodeTexCoord")
            wave = nt.nodes.new("ShaderNodeTexWave")
            wave.wave_type = "BANDS"
            wave.bands_direction = "DIAGONAL"
            wave.inputs["Scale"].default_value = 260.0
            wave.inputs["Distortion"].default_value = 1.5
            nt.links.new(coord.outputs["Window"], wave.inputs["Vector"])
            dark = nt.nodes.new("ShaderNodeMath")
            dark.operation = "LESS_THAN"
            dark.inputs[1].default_value = 0.1
            nt.links.new(bw.outputs[0], dark.inputs[0])
            lines = nt.nodes.new("ShaderNodeMath")
            lines.operation = "GREATER_THAN"
            lines.inputs[1].default_value = 0.72
            nt.links.new(wave.outputs["Fac"], lines.inputs[0])
            both = nt.nodes.new("ShaderNodeMath")
            both.operation = "MULTIPLY"
            nt.links.new(dark.outputs[0], both.inputs[0])
            nt.links.new(lines.outputs[0], both.inputs[1])
            hatch_mix = nt.nodes.new("ShaderNodeMix")
            hatch_mix.data_type = "RGBA"
            hatch_mix.blend_type = "MULTIPLY"
            nt.links.new(both.outputs[0], hatch_mix.inputs["Factor"])
            nt.links.new(shade, hatch_mix.inputs[6])
            hatch_mix.inputs[7].default_value = (0.45, 0.4, 0.38, 1)
            shade = hatch_mix.outputs[2]
        mul = nt.nodes.new("ShaderNodeMix")
        mul.data_type = "RGBA"
        mul.blend_type = "MULTIPLY"
        mul.inputs["Factor"].default_value = 1.0
        nt.links.new(shade, mul.inputs[6])
        if color_link is not None:
            nt.links.new(color_link, mul.inputs[7])
        else:
            mul.inputs[7].default_value = color_value
        emission = nt.nodes.new("ShaderNodeEmission")
        nt.links.new(mul.outputs[2], emission.inputs["Color"])
        # was ohnehin leuchtet, leuchtet weiter
        glow = nt.nodes.new("ShaderNodeEmission")
        e_in = bsdf.inputs["Emission Color"]
        if e_in.links:
            nt.links.new(e_in.links[0].from_socket, glow.inputs["Color"])
        else:
            glow.inputs["Color"].default_value = e_in.default_value
        glow.inputs["Strength"].default_value = bsdf.inputs["Emission Strength"].default_value
        add = nt.nodes.new("ShaderNodeAddShader")
        nt.links.new(emission.outputs[0], add.inputs[0])
        nt.links.new(glow.outputs[0], add.inputs[1])
        alpha = bsdf.inputs["Alpha"]
        if alpha.links or alpha.default_value < 0.999:
            clear = nt.nodes.new("ShaderNodeBsdfTransparent")
            mix = nt.nodes.new("ShaderNodeMixShader")
            if alpha.links:
                nt.links.new(alpha.links[0].from_socket, mix.inputs["Fac"])
            else:
                mix.inputs["Fac"].default_value = alpha.default_value
            nt.links.new(clear.outputs[0], mix.inputs[1])
            nt.links.new(add.outputs[0], mix.inputs[2])
            nt.links.new(mix.outputs[0], out.inputs["Surface"])
        else:
            nt.links.new(add.outputs[0], out.inputs["Surface"])


def eevee(scene, samples=32):
    scene.render.engine = "BLENDER_EEVEE"
    scene.eevee.taa_render_samples = samples


def sun(scene, strength=3.2, color=(1.0, 0.92, 0.8)):
    """Ein Sonnenlicht von links oben vorn, damit die Toon-Stufen unabhängig von den Raumlichtern sitzen"""
    import math
    data = bpy.data.lights.new("TuscheSonne", "SUN")
    data.energy = strength
    data.color = color
    data.angle = 0.2
    obj = bpy.data.objects.new("TuscheSonne", data)
    obj.rotation_euler = (math.radians(60), math.radians(-35), math.radians(-20))
    scene.collection.objects.link(obj)


def finish(scene, line=3.0, samples=32, hatch=True):
    """Szene fertig zeichnen: EEVEE, Sonne, Toon-Flächen mit Schraffur, Tuschelinien"""
    eevee(scene, samples)
    sun(scene)
    unline_transparent()
    toon_all(hatch=hatch)
    curves_to_meshes()
    ink(scene, line)


def curves_to_meshes(min_radius=1.4):
    """Freestyle zeichnet nur Netze: Kurven (Ranken, Wurzeln, Stiele) werden vor dem Rendern zu Netzen. Ganz feine (Haare)
    bleiben Kurven und damit ohne Linie, sonst würden sie zu Klecksen."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    no_line = bpy.data.collections.get(_NO_LINES)
    for obj in list(bpy.data.objects):
        if obj.type != "CURVE":
            continue
        if obj.data.bevel_depth < min_radius:
            no_lines(obj)  # feine Striche sind selbst schon Tusche
            continue

        if no_line is not None and obj.name in no_line.objects:
            continue
        mesh = bpy.data.meshes.new_from_object(obj.evaluated_get(depsgraph))
        new = bpy.data.objects.new(obj.name + "_Netz", mesh)
        new.matrix_world = obj.matrix_world
        for coll in obj.users_collection:
            coll.objects.link(new)
        bpy.data.objects.remove(obj)


def vignette_png(path, strength=0.85, burn=(0.10, 0.06, 0.03)):
    """Dunkelt die Ränder eines fertigen Bildes ab, wie angesengtes Papier (statt einer Fläche vor der Kamera, die Freestyle
    alle Linien verdecken würde)"""
    import numpy as np
    image = bpy.data.images.load(path)
    w, h = image.size
    pixels = np.array(image.pixels[:], np.float32).reshape(h, w, 4)
    yy, xx = np.mgrid[0:h, 0:w]
    dx = (xx + 0.5) / w - 0.5
    dy = (yy + 0.5) / h - 0.5
    r = np.sqrt((dx * 1.0) ** 2 + (dy * 1.25) ** 2) / 0.62
    fall = np.clip((r - 0.55) / 0.55, 0.0, 1.0) ** 1.6 * strength
    pixels[..., :3] = pixels[..., :3] * (1.0 - fall[..., None]) + np.array(burn, np.float32) * fall[..., None] * 0.3
    image.pixels = pixels.reshape(-1)
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()


def unline_transparent():
    """Flächen mit durchsichtigem Material (Lichthöfe, Lasuren, Flügelbilder mit Alpha) bekommen keine Linie um ihr Viereck"""
    for obj in list(bpy.data.objects):
        mats = getattr(obj.data, "materials", None) if obj.data is not None else None
        if not mats:
            continue
        for m in mats:
            if m is None or not m.use_nodes:
                continue
            nodes = m.node_tree.nodes
            cut = any(n.type == "BSDF_PRINCIPLED" and n.inputs["Alpha"].links for n in nodes)
            if cut or any(n.type == "BSDF_TRANSPARENT" for n in nodes):
                no_lines(obj)
                break


def spatter_png(path, seed=7, drops=900, stains=8):
    """Tuschespritzer und blasse Flecken über das fertige Bild, wie auf einer alten, benutzten Seite"""
    import numpy as np
    rng = np.random.default_rng(seed)
    image = bpy.data.images.load(path)
    w, h = image.size
    pixels = np.array(image.pixels[:], np.float32).reshape(h, w, 4)
    dark = np.zeros((h, w), np.float32)
    # Spritzer kommen in Wolken, wie vom Federkiel abgeschüttelt
    centres = rng.uniform((0, 0), (w, h), size=(30, 2))
    for _ in range(drops):
        cx, cy = centres[rng.integers(len(centres))] + rng.normal(0, 60, 2)
        r = rng.uniform(0.6, 2.6) if rng.random() < 0.95 else rng.uniform(3, 6)
        x0, x1 = int(max(cx - r - 1, 0)), int(min(cx + r + 2, w))
        y0, y1 = int(max(cy - r - 1, 0)), int(min(cy + r + 2, h))
        if x0 >= x1 or y0 >= y1:
            continue
        yy, xx = np.mgrid[y0:y1, x0:x1]
        d = np.hypot(xx + 0.5 - cx, yy + 0.5 - cy)
        dark[y0:y1, x0:x1] = np.maximum(dark[y0:y1, x0:x1], np.clip(r + 0.5 - d, 0, 1) * rng.uniform(0.35, 0.75))
    for _ in range(stains):
        cx, cy = rng.uniform(0, w), rng.uniform(0, h)
        r = rng.uniform(25, 90)
        x0, x1 = int(max(cx - r, 0)), int(min(cx + r, w))
        y0, y1 = int(max(cy - r, 0)), int(min(cy + r, h))
        if x0 >= x1 or y0 >= y1:
            continue
        yy, xx = np.mgrid[y0:y1, x0:x1]
        d = np.hypot(xx + 0.5 - cx, yy + 0.5 - cy) / r
        ring = np.exp(-((d - 0.92) / 0.06) ** 2) * 0.07 + np.clip(1 - d, 0, 1) * 0.04
        dark[y0:y1, x0:x1] = np.maximum(dark[y0:y1, x0:x1], ring)
    ink = np.array((0.08, 0.05, 0.03), np.float32)
    pixels[..., :3] = pixels[..., :3] * (1 - dark[..., None]) + ink * dark[..., None]
    image.pixels = pixels.reshape(-1)
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
