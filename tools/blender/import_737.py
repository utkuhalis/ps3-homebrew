"""Converts the 737 model from AirportPack into the game's part layout.

    Blender --background --python tools/blender/import_737.py

The model already consists of separate parts (ailerons, flaps, rudder,
speed brakes, landing gear, engine fans, doors). This script only maps the
names to the layout the game expects and converts the axes.

COPYRIGHT: this asset comes from a commercial Unity package. The output
file is NOT committed (.gitignore); it is only used in local builds.
"""

import bpy
import math
import os

SRC = ("/Users/dc/Desktop/Projects/Games/unity-test/Gozcu-v1/Assets/"
       "AirportPack/Meshes/SM_boeing737.fbx")
DST = os.path.join(os.path.dirname(__file__), '..', '..',
                   'assets', 'model', 'boeing737.glb')

TEX_DIR = ("/Users/dc/Desktop/Projects/Games/unity-test/Gozcu-v1/Assets/"
           "AirportPack/Textures/Boeing_737")

PREFIX = 'SM_boeing737'


def game_name(raw):
    """Maps a package name to the part name the game expects."""
    n = raw
    if n.startswith(PREFIX):
        n = n[len(PREFIX):]
    n = n.lstrip('_')
    n = n.split('.')[0]              # Blender's .001 suffixes

    if n == '':
        return 'body'
    if n.startswith('speed_brakes_left'):
        return 'spoiler_left' + n[len('speed_brakes_left'):]
    if n.startswith('speed_brakes_right'):
        return 'spoiler_right' + n[len('speed_brakes_right'):]
    if n.startswith('flaps_'):
        return 'flap_' + n[len('flaps_'):]
    if n.startswith('engine_fan_'):
        return 'fan_' + n[len('engine_fan_'):]
    if n.startswith('gear_door'):
        return 'geardoor_' + n[len('gear_door_'):]
    return n


# Material by part name. The model is entirely texture based, so all
# its materials arrive white and carry no information; we tried baking
# the textures into vertex colours, but Blender's colour attribute comes
# out white in the glTF export. A 737's materials are well known, so we assign them.
#
# (name prefix, r, g, b, metallic)
MATERIALS = [
    ('wheel',           0.06, 0.06, 0.07, 0.00),   # tyre
    ('gear',            0.52, 0.54, 0.58, 0.85),   # chrome strut
    ('geardoor',        0.86, 0.87, 0.89, 0.45),
    ('fan',             0.18, 0.19, 0.21, 0.70),   # fan disc
    ('thrust_reverser', 0.44, 0.45, 0.48, 0.80),
    ('engine',          0.44, 0.45, 0.48, 0.80),
    ('door',            0.82, 0.83, 0.85, 0.40),
    ('spoiler',         0.80, 0.81, 0.84, 0.55),
    ('flap',            0.84, 0.85, 0.88, 0.50),
    ('aileron',         0.84, 0.85, 0.88, 0.50),
    ('rudder',          0.22, 0.36, 0.62, 0.35),   # tail colour
    ('body',            0.88, 0.89, 0.91, 0.45),   # body: light, metallic
]


def assign_materials():
    """Gives each part the material that matches its name."""
    for ob in bpy.data.objects:
        if ob.type != 'MESH':
            continue

        rgb = (0.85, 0.86, 0.88)
        metal = 0.45
        for prefix, r, g, b, m in MATERIALS:
            if ob.name.startswith(prefix):
                rgb = (r, g, b)
                metal = m
                break

        mat = bpy.data.materials.new('M_' + ob.name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get('Principled BSDF')
        if bsdf:
            bsdf.inputs['Base Color'].default_value = (rgb[0], rgb[1],
                                                       rgb[2], 1.0)
            bsdf.inputs['Metallic'].default_value = metal
            bsdf.inputs['Roughness'].default_value = 0.30

        ob.data.materials.clear()
        ob.data.materials.append(mat)

    print('  materials assigned to %d parts'
          % len([o for o in bpy.data.objects if o.type == 'MESH']))


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SRC)

    # Part positions come from a root object called RIG_boeing737.
    # Deleting the non-mesh objects first dropped the children's world
    # transforms: every part piled up at the origin, engines on top of each other.
    # So parents are cleared keeping the transform, and transforms are applied first.
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    bpy.ops.object.select_all(action='DESELECT')
    for ob in bpy.data.objects:
        if ob.type == 'MESH':
            ob.select_set(True)
    bpy.context.view_layer.objects.active = next(
        (o for o in bpy.data.objects if o.type == 'MESH'), None)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.select_all(action='DESELECT')

    used = {}
    for ob in list(bpy.data.objects):
        if ob.type != 'MESH':
            bpy.data.objects.remove(ob, do_unlink=True)
            continue
        base = game_name(ob.name)
        # Parts that map to the same name (flap_left01/02...) are numbered
        k = used.get(base, 0)
        used[base] = k + 1
        ob.name = base if k == 0 else '%s_%d' % (base, k)

    tot = 0
    print('\n=== MAPPED PARTS ===')
    for ob in sorted(bpy.data.objects, key=lambda o: o.name):
        if ob.type != 'MESH':
            continue
        n = sum(len(p.vertices) - 2 for p in ob.data.polygons)
        tot += n
        print('  %-26s %6d ucgen' % (ob.name, n))
    print('  %d triangles total, %d parts'
          % (tot, len([o for o in bpy.data.objects if o.type == 'MESH'])))

    print('\n=== ASSIGNING MATERIALS ===')
    assign_materials()

    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.export_scene.gltf(filepath=os.path.abspath(DST),
                              export_format='GLB',
                              export_yup=True,
                              export_apply=True)
    print('yazildi: %s' % os.path.abspath(DST))


main()
