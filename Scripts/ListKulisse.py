# Lists the static meshes of the packs the Aschenmark scenery may use (read only): size, material slots and the
# textures of their materials, written to Saved/Kulisse_Inventar.json for Scripts/BuildKulisse.py.

import json
import os

import unreal

FOLDERS = ["/Game/Megascans/3D_Assets", "/Game/Megaplant_Library", "/Game/Rock_Collection_04/Meshes", "/Game/Fab", "/Game/BlackAlder",
           "/Game/Megascans/3D_Plants", "/Game/Vael/Maps/Kulisse", "/Game/Vael/Maps/Aschenmark/Bauten"]
out = {}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
for folder in FOLDERS:
    for data in registry.get_assets_by_path(folder, recursive=True):
        if str(data.asset_class_path.asset_name) != "StaticMesh":
            continue
        path = str(data.package_name)
        mesh = unreal.load_asset(path)
        box = mesh.get_bounding_box()
        mats = []
        for slot in mesh.get_editor_property("static_materials"):
            mi = slot.get_editor_property("material_interface")
            textures = {}
            parent = None
            if isinstance(mi, unreal.MaterialInstance):
                parent = mi.get_base_material().get_path_name()
                for p in mi.get_editor_property("texture_parameter_values"):
                    t = p.get_editor_property("parameter_value")
                    textures[str(p.get_editor_property("parameter_info").get_editor_property("name"))] = t.get_path_name() if t else None
            mats.append({"slot": str(slot.get_editor_property("material_slot_name")), "material": mi.get_path_name() if mi else None,
                         "base": parent, "textures": textures})
        out[path] = {"size": [round(box.max.x - box.min.x), round(box.max.y - box.min.y), round(box.max.z - box.min.z)],
                     "nanite": mesh.get_editor_property("nanite_settings").get_editor_property("enabled"), "materials": mats}
target = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Kulisse_Inventar.json")
json.dump(out, open(target, "w", encoding="utf-8"), indent=1)
unreal.log_warning("[ListKulisse] {} meshes".format(len(out)))
