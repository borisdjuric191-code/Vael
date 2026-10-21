"""Preview of both material forms in Blender (EEVEE), standing and in the camouflage pose: the Unreal materials rebuilt
roughly as Blender nodes (light: Tripo texture, green crest, amber eyes; shadow: ash grey fur, black violet green parts,
violet glowing veins, crest black violet with red tips, violet eyes). No Megascans texture involved.
Writes vorschau/08_licht_*.png, vorschau/08_schatten_*.png and the sheet vorschau/08_formen.png.

    blender -b --factory-startup -P skripte/08_vorschau_formen.py
"""
import math
import os
import sys

import bpy

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

ORDNER = vorschau.ORDNER


def knoten(mat):
    mat.use_nodes = True
    return mat.node_tree.nodes, mat.node_tree.links


def koerper_schatten(mat):
    nodes, links = knoten(mat)
    bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
    tex = next(n for n in nodes if n.type == "TEX_IMAGE" and "basecolor" in n.image.name.lower())
    sep = nodes.new("ShaderNodeSeparateColor")
    links.new(tex.outputs["Color"], sep.inputs["Color"])
    mx = nodes.new("ShaderNodeMath")
    mx.operation = "MAXIMUM"
    links.new(sep.outputs["Red"], mx.inputs[0])
    links.new(sep.outputs["Blue"], mx.inputs[1])
    diff = nodes.new("ShaderNodeMath")
    diff.operation = "SUBTRACT"
    links.new(sep.outputs["Green"], diff.inputs[0])
    links.new(mx.outputs[0], diff.inputs[1])
    gruen = nodes.new("ShaderNodeMath")
    gruen.operation = "MULTIPLY"
    gruen.use_clamp = True
    links.new(diff.outputs[0], gruen.inputs[0])
    gruen.inputs[1].default_value = 6.0
    grau = nodes.new("ShaderNodeHueSaturation")
    grau.inputs["Saturation"].default_value = 0.0
    grau.inputs["Value"].default_value = 0.42
    links.new(tex.outputs["Color"], grau.inputs["Color"])
    violett = nodes.new("ShaderNodeMix")
    violett.data_type = "RGBA"
    links.new(gruen.outputs[0], violett.inputs["Factor"])
    links.new(grau.outputs["Color"], violett.inputs["A"])
    violett.inputs["B"].default_value = (0.035, 0.005, 0.05, 1.0)
    links.new(violett.outputs["Result"], bsdf.inputs["Base Color"])
    # Veins: zero crossings of noise on the object's own coordinates
    coord = nodes.new("ShaderNodeTexCoord")
    rausch = nodes.new("ShaderNodeTexNoise")
    rausch.inputs["Scale"].default_value = 9.0
    rausch.inputs["Detail"].default_value = 3.0
    links.new(coord.outputs["Object"], rausch.inputs["Vector"])
    ab = nodes.new("ShaderNodeMath")
    ab.operation = "SUBTRACT"
    links.new(rausch.outputs["Fac"], ab.inputs[0])
    ab.inputs[1].default_value = 0.5
    betrag = nodes.new("ShaderNodeMath")
    betrag.operation = "ABSOLUTE"
    links.new(ab.outputs[0], betrag.inputs[0])
    linie = nodes.new("ShaderNodeMapRange")
    links.new(betrag.outputs[0], linie.inputs["Value"])
    linie.inputs["From Min"].default_value = 0.0
    linie.inputs["From Max"].default_value = 0.012
    linie.inputs["To Min"].default_value = 1.0
    linie.inputs["To Max"].default_value = 0.0
    bsdf.inputs["Emission Color"].default_value = (0.55, 0.08, 1.0, 1.0)
    links.new(linie.outputs["Result"], bsdf.inputs["Emission Strength"])


def kamm(mat, schatten):
    nodes, links = knoten(mat)
    bsdf = nodes.get("Principled BSDF") or nodes.new("ShaderNodeBsdfPrincipled")
    out = nodes.get("Material Output") or nodes.new("ShaderNodeOutputMaterial")
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    if not schatten:
        bsdf.inputs["Base Color"].default_value = (0.05, 0.38, 0.04, 1.0)
        return
    uv = nodes.new("ShaderNodeUVMap")
    sep = nodes.new("ShaderNodeSeparateXYZ")
    links.new(uv.outputs["UV"], sep.inputs["Vector"])
    rampe = nodes.new("ShaderNodeValToRGB")
    rampe.color_ramp.elements[0].position = 0.55
    rampe.color_ramp.elements[0].color = (0.035, 0.005, 0.05, 1.0)
    rampe.color_ramp.elements[1].color = (0.32, 0.0, 0.02, 1.0)
    links.new(sep.outputs["Y"], rampe.inputs["Fac"])
    links.new(rampe.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Emission Color"].default_value = (0.55, 0.08, 1.0, 1.0)
    bsdf.inputs["Emission Strength"].default_value = 0.03


def augen(mat, schatten):
    nodes, links = knoten(mat)
    bsdf = nodes.get("Principled BSDF") or nodes.new("ShaderNodeBsdfPrincipled")
    out = nodes.get("Material Output") or nodes.new("ShaderNodeOutputMaterial")
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    bsdf.inputs["Base Color"].default_value = (0, 0, 0, 1)
    bsdf.inputs["Emission Color"].default_value = (0.55, 0.08, 1.0, 1.0) if schatten else (1.0, 0.55, 0.08, 1.0)
    bsdf.inputs["Emission Strength"].default_value = 12.0


def rendern(form):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE" if "BLENDER_EEVEE" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = scene.render.resolution_y = 900
    world = bpy.data.worlds.get("Vorschau") or bpy.data.worlds.new("Vorschau")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.3, 0.3, 0.32, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.2
    scene.world = world
    if not bpy.data.objects.get("Sonne"):
        sonne = bpy.data.objects.new("Sonne", bpy.data.lights.new("Sonne", "SUN"))
        sonne.data.energy = 3.0
        sonne.rotation_euler = (math.radians(45), 0, math.radians(30))
        scene.collection.objects.link(sonne)
    cam = vorschau._kamera()
    from mathutils import Vector
    for name, richtung, mitte_ in (("seite", (0.0, -1.0, 0.05), (-0.15, 0, 0.5)), ("spielkamera", (0.45, -0.8, math.tan(math.radians(50))), (-0.15, 0, 0.5))):
        r = Vector(richtung).normalized()
        cam.location = Vector(mitte_) + r * 10
        vorwaerts = -r
        rechts = vorwaerts.cross(Vector((0, 0, 1))).normalized()
        hoch = rechts.cross(vorwaerts).normalized()
        from mathutils import Matrix
        cam.rotation_euler = Matrix((rechts, hoch, -vorwaerts)).transposed().to_euler()
        cam.data.ortho_scale = 1.8
        scene.render.filepath = os.path.join(vorschau.VORSCHAU, "08_%s_%s.png" % (form, name))
        bpy.ops.render.render(write_still=True)


def main():
    for form in ("licht", "schatten"):
        bpy.ops.wm.open_mainfile(filepath=os.path.join(ORDNER, "arbeit", "04_animationen.blend"))
        luchs = bpy.data.objects["Rankenluchs"]
        arm = bpy.data.objects["Rankenluchs_Skelett"]
        for pb in arm.pose.bones:
            pb.location = (0, 0, 0)
            pb.rotation_quaternion = (1, 0, 0, 0)
            pb.scale = (1, 1, 1)
        mats = {m.name: m for m in luchs.data.materials}
        if form == "schatten":
            koerper_schatten(luchs.data.materials[0])
        kamm(mats["Kamm"], form == "schatten")
        augen(mats["Augen"], form == "schatten")
        rendern(form)
        print("[Rankenluchs] Form gerendert:", form)


main()
