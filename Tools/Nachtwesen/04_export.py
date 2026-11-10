"""Step 4: exports a night creature for Unreal (export/SK_<Name>.fbx with every action, textures next to it).

    blender -b --factory-startup -P Tools/Nachtwesen/04_export.py -- <Name>
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402

bau.exportieren(sys.argv[sys.argv.index("--") + 1])
