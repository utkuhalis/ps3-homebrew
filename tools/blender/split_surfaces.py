"""Splits the control surfaces in plane.glb into separate objects.

Runs in headless Blender; no BlenderMCP add-on needed:

    Blender --background --python tools/blender/split_surfaces.py

The model arrives as a single fuselage mesh, so flaps, ailerons, spoilers,
elevator and rudder are selected geometrically: a face is selected and
separated if its centre falls in the region where that surface belongs.

Blender axes (after import, Z up):
    -Y nose,  +Y tail,  X span,  Z up
Measurements come from a region analysis done in Blender:
    wing plane        Z ~ -1.5,  Y 0.0 .. 4.15  (trailing edge ~4.1)
    wing tip          |X| ~ 9.8
    vertical tail     Y 7.7 .. 10.9,  Z 0.8 .. 3.2
"""

import bpy
import bmesh
import os
import sys

SRC = os.path.join(os.path.dirname(__file__), '..', '..',
                   'assets', 'model', 'plane.glb')
DST = os.path.join(os.path.dirname(__file__), '..', '..',
                   'assets', 'model', 'plane_split.glb')

BODY = 'Air Plane'

# The wing is swept: the trailing edge Y shifts with span. Measured:
#   |X|=4 -> Y=2.41 ,  |X|=9 -> Y=4.12   =>  Y_te = 0.342*|X| + 1.04
def wing_trailing_y(x):
    return 0.342 * abs(x) + 1.04


# Wing trailing edge: faces in the last slice of the wing chord
WING_Z = (-2.35, -0.85)
TRAIL_Y = (3.55, 4.40)          # trailing edge strip
SPOIL_Y = (2.70, 3.55)          # just ahead of the trailing edge (upper surface)

def bisect_wing(obj, sign, offset):
    """Cuts the wing with a slanted plane that follows the sweep.

    The wing panels are low-poly, so there are no ready-made faces at the
    trailing edge to separate; a new edge has to be cut with a plane first:
        Y - 0.342*sign*X = 1.04 - offset
    """
    import mathutils

    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bm = bmesh.from_edit_mesh(obj.data)
    bm.faces.ensure_lookup_table()

    mw = obj.matrix_world
    inv = mw.inverted()

    no_world = mathutils.Vector((-0.342 * sign, 1.0, 0.0)).normalized()
    co_world = mathutils.Vector((0.0, 1.04 - offset, 0.0))

    # Only the geometry of the relevant wing is cut
    geom = []
    for f in bm.faces:
        c = mw @ f.calc_center_median()
        if (sign * c.x > 2.20 and -2.35 < c.z < -0.85 and c.y < 6.0):
            geom.append(f)
            geom.extend(f.verts)
            geom.extend(f.edges)

    if not geom:
        bpy.ops.object.mode_set(mode='OBJECT')
        return

    no = (inv.to_3x3().transposed() @ no_world).normalized()
    co = inv @ co_world

    bmesh.ops.bisect_plane(bm, geom=list(set(geom)), plane_co=co, plane_no=no,
                           clear_inner=False, clear_outer=False)
    bmesh.update_edit_mesh(obj.data)
    bpy.ops.object.mode_set(mode='OBJECT')


REGIONS = [
    # (name, test function)
    ('flap_left',     lambda c, n: (-5.80 < c.x < -2.20 and
                                    c.y > wing_trailing_y(c.x) - 0.55 and
                                    c.y < 6.0 and
                                    WING_Z[0] < c.z < WING_Z[1])),
    ('flap_right',    lambda c, n: (2.20 < c.x < 5.80 and
                                    c.y > wing_trailing_y(c.x) - 0.55 and
                                    c.y < 6.0 and
                                    WING_Z[0] < c.z < WING_Z[1])),
    ('aileron_left',  lambda c, n: (c.x < -5.80 and
                                    c.y > wing_trailing_y(c.x) - 0.55 and
                                    WING_Z[0] < c.z < WING_Z[1])),
    ('aileron_right', lambda c, n: (c.x > 5.80 and
                                    c.y > wing_trailing_y(c.x) - 0.55 and
                                    WING_Z[0] < c.z < WING_Z[1])),
    # Trailing edge of the vertical tail
    ('rudder',        lambda c, n: (abs(c.x) < 0.65 and
                                    c.y > 10.10 and c.z > 0.85)),
]


def load():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.abspath(SRC))
    return bpy.data.objects[BODY]


def split(obj, name, test):
    """Separates the faces that pass test into a new object; returns the count."""
    mw = obj.matrix_world
    nw = mw.to_3x3()

    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bm = bmesh.from_edit_mesh(obj.data)
    bm.faces.ensure_lookup_table()

    for f in bm.faces:
        f.select = False

    n = 0
    for f in bm.faces:
        c = mw @ f.calc_center_median()
        nrm = (nw @ f.normal).normalized()
        if test(c, nrm):
            f.select = True
            n += 1

    bmesh.update_edit_mesh(obj.data)

    if n == 0:
        bpy.ops.object.mode_set(mode='OBJECT')
        return 0

    before = set(bpy.data.objects.keys())
    bpy.ops.mesh.separate(type='SELECTED')
    bpy.ops.object.mode_set(mode='OBJECT')

    new = [k for k in bpy.data.objects.keys() if k not in before]
    if new:
        bpy.data.objects[new[0]].name = name
    return n


WHEEL_MATERIALS = ('Material.005', 'Material.006')   # tyre and rim


def split_wheels():
    """Separates the wheel hub from the suspension strut.

    Tyre and rim use their own materials, the strut another; without
    separating them, spinning the wheel would spin the strut too.

    Source objects are found by CONTENT, not by NAME: object names shift
    during import and earlier separations, and relying on fixed names was
    fragile. Criterion: objects that contain the wheel material and lie
    outside the fuselage; left/right/nose comes from the bounding box centre. """
    from mathutils import Vector

    # Targets are decided FIRST: bpy.data.objects changes during separation
    # and names shift, so operating while iterating over it is not safe.
    targets = []
    for o in list(bpy.data.objects):
        if o.type != 'MESH' or o.name == BODY:
            continue
        if len(o.data.polygons) < 500:
            continue
        has = any(m is not None and m.name in WHEEL_MATERIALS
                  for m in o.data.materials)
        if not has:
            continue

        bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
        cx = sum(v[0] for v in bb) / 8.0
        targets.append((o, cx))

    for o, cx in targets:
        name = 'wheel_left' if cx < -0.5 else (
               'wheel_right' if cx > 0.5 else 'wheel_front')

        wheel_idx = set(i for i, m in enumerate(o.data.materials)
                        if m is not None and m.name in WHEEL_MATERIALS)

        # Entering EDIT mode without clearing the selection is dangerous: mesh.separate
        # works on ALL selected objects and was shredding the fuselage too.
        bpy.ops.object.select_all(action='DESELECT')
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode='EDIT')
        bm = bmesh.from_edit_mesh(o.data)
        bm.faces.ensure_lookup_table()

        n = 0
        for f in bm.faces:
            f.select = f.material_index in wheel_idx
            if f.select:
                n += 1
        bmesh.update_edit_mesh(o.data)

        if n == 0:
            bpy.ops.object.mode_set(mode='OBJECT')
            continue

        # The new object is found by REFERENCE, not by NAME: a shifted name
        # renamed the wrong object (the fuselage had been marked 'wheel_front'
        # at one point).
        before = set(bpy.data.objects.values())
        bpy.ops.mesh.separate(type='SELECTED')
        bpy.ops.object.mode_set(mode='OBJECT')

        fresh = [x for x in bpy.data.objects.values() if x not in before]
        src_name = o.name
        if fresh:
            fresh[0].name = name
            print('  %-14s -> %-13s %5d faces (centre X %.2f)'
                  % (src_name, name, n, cx))


def main():
    body = load()

    # Cut the wings along the trailing edge line first, then separate the parts
    print('\n=== SEPARATING WHEELS ===')
    split_wheels()

    print('\n=== CUTTING WINGS ===')
    bisect_wing(body, -1.0, 0.55)
    bisect_wing(body,  1.0, 0.55)
    print('  cut done, %d faces' % len(body.data.polygons))

    print('\n=== SEPARATING CONTROL SURFACES ===')

    for name, test in REGIONS:
        n = split(body, name, test)
        print('  %-16s %5d yuz' % (name, n))

    print('\n=== SONUC ===')
    total = 0
    for o in bpy.data.objects:
        if o.type == 'MESH':
            print('  %-16s %6d yuz' % (o.name, len(o.data.polygons)))
            total += len(o.data.polygons)
    print('  %d faces total' % total)

    bpy.ops.export_scene.gltf(filepath=os.path.abspath(DST),
                              export_format='GLB',
                              export_yup=True,
                              export_apply=False)
    print('yazildi: %s' % os.path.abspath(DST))


main()
