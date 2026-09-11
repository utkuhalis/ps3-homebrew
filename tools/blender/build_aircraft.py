"""Builds a clean business jet model - in parts, with UVs, suitable for the PS3.

    Blender --background --python tools/blender/build_aircraft.py

Why we build our own model: in ready-made CC0 aircraft the control surfaces
come welded to the body, the UVs are automatic cylindrical unwraps so no
livery can be painted, and open faces are left inside the body (that is
why the inside of the engines showed). Here every part is BORN as its own
object, UVs are unwrapped deliberately as planar/cylindrical, and no face
points inwards.

The model is built in GAME axes (+X right, +Y up, -Z nose); right before
export it is rotated +90 degrees about X, because Blender works Z-up and
the glTF exporter expects Blender axes. When that step was skipped,
length and height swapped.
Units are metres; overall length ~14 m, span ~13 m.
"""

import bpy
import bmesh
import math
import os

from mathutils import Vector

DST = os.path.join(os.path.dirname(__file__), '..', '..',
                   'assets', 'model', 'jet.glb')

# --- main dimensions (metres) ---
FUS_LEN      = 14.0     # fuselage length
FUS_R        = 0.95     # fuselage radius
NOSE_LEN     = 2.6
TAIL_LEN     = 4.2

WING_SPAN    = 6.3      # from the root centre to the tip
WING_ROOT    = 2.9      # root chord
WING_TIP     = 1.15     # tip chord
WING_SWEEP   = 1.9      # how far the tip sits behind the root
WING_DIHED   = 0.30     # how far the tip sits above the root
WING_THICK   = 0.16     # thickness relative to chord
WING_Z       = 1.2      # position of the wing along the fuselage (+ is aft)
WING_Y       = -0.35    # low wing

CTRL_CHORD   = 0.26     # how much of the trailing edge is control surface
FLAP_SPAN    = (0.18, 0.62)     # span fraction range
AIL_SPAN     = (0.66, 0.96)

HT_SPAN      = 2.5      # horizontal stabiliser
HT_ROOT      = 1.35
HT_TIP       = 0.75
HT_SWEEP     = 0.55
VT_HEIGHT    = 2.9      # vertical stabiliser
VT_ROOT      = 2.2
VT_TIP       = 1.15
VT_SWEEP     = 1.5

ENG_LEN      = 2.4
ENG_R        = 0.62
ENG_X        = 1.42     # closer to the fuselage
ENG_Y        = 0.35
ENG_Z        = 2.9

GEAR_R       = 0.38     # wheel radius
GEAR_W       = 0.26
STRUT_R      = 0.13     # strut radius (at 0.10 it looked like a line)


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def new_mesh(name):
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    return ob, bmesh.new()


def finish(ob, bm, smooth=True):
    bm.normal_update()
    bm.to_mesh(ob.data)
    bm.free()
    if smooth:
        for p in ob.data.polygons:
            p.use_smooth = True
    return ob


def airfoil(t, chord, thick):
    """Simple symmetric section: y offset for the upper and lower surface.
    t: 0 (leading edge) .. 1 (trailing edge) """
    # NACA-like: thickness rises fast at the leading edge and thins towards the back
    y = 5.0 * thick * chord * (0.2969 * math.sqrt(max(t, 0.0))
                               - 0.1260 * t
                               - 0.3516 * t * t
                               + 0.2843 * t ** 3
                               - 0.1015 * t ** 4)
    return y


def make_wing_panel(name, side, t0, t1, with_thickness=True):
    """A slice of the wing over the span [t0,t1]. side: +1 right, -1 left.

    The control surfaces come out of the same function, so the trailing
    edge section matches the wing exactly and no gap is left. """
    ob, bm = new_mesh(name)

    STEPS = 10          # samples along the chord
    segs = 6            # along the span

    def station(s):
        """Root/tip interpolated values for span fraction s."""
        chord = WING_ROOT + (WING_TIP - WING_ROOT) * s
        x = side * WING_SPAN * s
        z = WING_Z + WING_SWEEP * s
        y = WING_Y + WING_DIHED * s
        return x, y, z, chord

    verts_top = []
    verts_bot = []
    for i in range(segs + 1):
        s = t0 + (t1 - t0) * (i / float(segs))
        x, y, z, chord = station(s)

        row_t = []
        row_b = []
        for j in range(STEPS + 1):
            t = j / float(STEPS)
            zz = z - chord * 0.5 + chord * t
            yy = airfoil(t, chord, WING_THICK) if with_thickness else 0.0
            row_t.append(bm.verts.new((x, y + yy, zz)))
            row_b.append(bm.verts.new((x, y - yy, zz)))
        verts_top.append(row_t)
        verts_bot.append(row_b)

    bm.verts.ensure_lookup_table()

    for i in range(segs):
        for j in range(STEPS):
            # upper surface (the order matters for outward-facing normals)
            bm.faces.new((verts_top[i][j], verts_top[i + 1][j],
                          verts_top[i + 1][j + 1], verts_top[i][j + 1]))
            # lower surface
            bm.faces.new((verts_bot[i][j], verts_bot[i][j + 1],
                          verts_bot[i + 1][j + 1], verts_bot[i + 1][j]))

    # root and tip caps: so no inner faces are left
    for row_t, row_b, flip in ((verts_top[0], verts_bot[0], False),
                               (verts_top[-1], verts_bot[-1], True)):
        for j in range(STEPS):
            quad = (row_t[j], row_t[j + 1], row_b[j + 1], row_b[j])
            bm.faces.new(quad if flip else tuple(reversed(quad)))

    return finish(ob, bm)


def make_fuselage(name):
    """Fuselage: conical nose, cylindrical middle, upswept conical tail."""
    ob, bm = new_mesh(name)

    RINGS = 22
    SIDES = 16

    def profile(u):
        """u: 0 nose .. 1 tail. Returns (radius, y offset)."""
        nose_end = NOSE_LEN / FUS_LEN
        tail_start = 1.0 - TAIL_LEN / FUS_LEN

        if u < nose_end:
            k = u / nose_end
            r = FUS_R * math.sin(k * math.pi * 0.5) ** 0.7
            dy = 0.0
        elif u > tail_start:
            k = (u - tail_start) / (1.0 - tail_start)
            r = FUS_R * (1.0 - k) ** 0.75
            dy = 0.55 * k * k          # the tail sweeps up
        else:
            r = FUS_R
            dy = 0.0
        return max(r, 0.02), dy

    rings = []
    for i in range(RINGS + 1):
        u = i / float(RINGS)
        z = -FUS_LEN * 0.5 + FUS_LEN * u
        r, dy = profile(u)
        ring = []
        for j in range(SIDES):
            a = 2.0 * math.pi * j / SIDES
            ring.append(bm.verts.new((math.sin(a) * r,
                                      math.cos(a) * r + dy,
                                      z)))
        rings.append(ring)

    bm.verts.ensure_lookup_table()

    for i in range(RINGS):
        for j in range(SIDES):
            k = (j + 1) % SIDES
            bm.faces.new((rings[i][j], rings[i][k],
                          rings[i + 1][k], rings[i + 1][j]))

    # nose and tail caps (no hollow inside)
    nose_c = bm.verts.new((0.0, 0.0, -FUS_LEN * 0.5 - 0.15))
    tail_c = bm.verts.new((0.0, 0.55, FUS_LEN * 0.5 + 0.05))
    for j in range(SIDES):
        k = (j + 1) % SIDES
        bm.faces.new((nose_c, rings[0][k], rings[0][j]))
        bm.faces.new((tail_c, rings[-1][j], rings[-1][k]))

    return finish(ob, bm)


def make_tube(name, cx, cy, cz, radius, length, sides=14, closed=True):
    """Closed cylinder (engine nacelle, gear strut, wheel)."""
    ob, bm = new_mesh(name)

    front = []
    back = []
    for j in range(sides):
        a = 2.0 * math.pi * j / sides
        front.append(bm.verts.new((cx + math.sin(a) * radius,
                                   cy + math.cos(a) * radius,
                                   cz - length * 0.5)))
        back.append(bm.verts.new((cx + math.sin(a) * radius,
                                  cy + math.cos(a) * radius,
                                  cz + length * 0.5)))
    bm.verts.ensure_lookup_table()

    for j in range(sides):
        k = (j + 1) % sides
        bm.faces.new((front[j], front[k], back[k], back[j]))

    if closed:
        fc = bm.verts.new((cx, cy, cz - length * 0.5))
        bc = bm.verts.new((cx, cy, cz + length * 0.5))
        for j in range(sides):
            k = (j + 1) % sides
            bm.faces.new((fc, front[k], front[j]))
            bm.faces.new((bc, back[j], back[k]))

    return finish(ob, bm)


def make_wheel(name, cx, cy, cz):
    """Wheel: a cylinder lying on its side so it spins about X."""
    ob, bm = new_mesh(name)
    sides = 16

    left = []
    right = []
    for j in range(sides):
        a = 2.0 * math.pi * j / sides
        y = cy + math.cos(a) * GEAR_R
        z = cz + math.sin(a) * GEAR_R
        left.append(bm.verts.new((cx - GEAR_W * 0.5, y, z)))
        right.append(bm.verts.new((cx + GEAR_W * 0.5, y, z)))
    bm.verts.ensure_lookup_table()

    for j in range(sides):
        k = (j + 1) % sides
        bm.faces.new((left[j], right[j], right[k], left[k]))

    lc = bm.verts.new((cx - GEAR_W * 0.5, cy, cz))
    rc = bm.verts.new((cx + GEAR_W * 0.5, cy, cz))
    for j in range(sides):
        k = (j + 1) % sides
        bm.faces.new((lc, left[k], left[j]))
        bm.faces.new((rc, right[j], right[k]))

    return finish(ob, bm)


def make_stab(name, span, root, tip, sweep, z0, vertical=False):
    """Horizontal or vertical stabiliser (the control surface is built separately)."""
    ob, bm = new_mesh(name)
    segs = 4
    STEPS = 6

    rows_a = []
    rows_b = []
    for i in range(segs + 1):
        s = i / float(segs)
        chord = root + (tip - root) * s
        off = sweep * s
        row_a = []
        row_b = []
        for j in range(STEPS + 1):
            t = j / float(STEPS)
            zz = z0 + off - chord * 0.5 + chord * t
            th = airfoil(t, chord, 0.13)
            if vertical:
                row_a.append(bm.verts.new((th, 0.55 + VT_HEIGHT * s, zz)))
                row_b.append(bm.verts.new((-th, 0.55 + VT_HEIGHT * s, zz)))
            else:
                row_a.append(bm.verts.new((span * s, 0.55 + th, zz)))
                row_b.append(bm.verts.new((span * s, 0.55 - th, zz)))
        rows_a.append(row_a)
        rows_b.append(row_b)

    bm.verts.ensure_lookup_table()
    for i in range(segs):
        for j in range(STEPS):
            bm.faces.new((rows_a[i][j], rows_a[i + 1][j],
                          rows_a[i + 1][j + 1], rows_a[i][j + 1]))
            bm.faces.new((rows_b[i][j], rows_b[i][j + 1],
                          rows_b[i + 1][j + 1], rows_b[i + 1][j]))

    for ra, rb, flip in ((rows_a[0], rows_b[0], False),
                         (rows_a[-1], rows_b[-1], True)):
        for j in range(STEPS):
            quad = (ra[j], ra[j + 1], rb[j + 1], rb[j])
            bm.faces.new(quad if flip else tuple(reversed(quad)))

    return finish(ob, bm)


def mirror(ob, name):
    """Mirror across X (left wing, left engine...)."""
    new = ob.copy()
    new.data = ob.data.copy()
    new.name = name
    bpy.context.collection.objects.link(new)
    new.scale.x = -1.0
    bpy.context.view_layer.objects.active = new
    new.select_set(True)
    bpy.ops.object.transform_apply(scale=True)
    # mirroring reverses the face order; fix the normals back
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.flip_normals()
    bpy.ops.object.mode_set(mode='OBJECT')
    new.select_set(False)
    return new


def unwrap_all():
    """Unwraps each object into its own UV space.

    Smart project leaves margins around the islands; good enough for
    painting a livery and usable for repeating textures too. """
    for ob in bpy.data.objects:
        if ob.type != 'MESH':
            continue
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.02)
        bpy.ops.object.mode_set(mode='OBJECT')
        ob.select_set(False)


def add_material(ob, name, rgb, metallic=0.2, rough=0.45):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get('Principled BSDF')
        if bsdf:
            bsdf.inputs['Base Color'].default_value = (rgb[0], rgb[1], rgb[2], 1)
            bsdf.inputs['Metallic'].default_value = metallic
            bsdf.inputs['Roughness'].default_value = rough
    ob.data.materials.append(mat)


def main():
    clear()

    body = make_fuselage('body')
    add_material(body, 'skin', (0.86, 0.87, 0.89))

    # --- wings: fixed part + flap + aileron as separate parts ---
    for side, tag in ((1.0, 'right'), (-1.0, 'left')):
        main_panel = make_wing_panel('wing_%s' % tag, side, 0.06, 1.0)
        add_material(main_panel, 'skin', (0.86, 0.87, 0.89))

        flap = make_wing_panel('flap_%s' % tag, side,
                               FLAP_SPAN[0], FLAP_SPAN[1])
        add_material(flap, 'control', (0.78, 0.79, 0.82))

        ail = make_wing_panel('aileron_%s' % tag, side,
                              AIL_SPAN[0], AIL_SPAN[1])
        add_material(ail, 'control', (0.78, 0.79, 0.82))

        # move the control surfaces to the trailing edge and shorten their chord
        for ob, span in ((flap, FLAP_SPAN), (ail, AIL_SPAN)):
            for v in ob.data.vertices:
                s = abs(v.co.x) / WING_SPAN
                chord = WING_ROOT + (WING_TIP - WING_ROOT) * s
                z_le = WING_Z + WING_SWEEP * s - chord * 0.5
                t = (v.co.z - z_le) / chord
                # keep only the rear CTRL_CHORD slice
                v.co.z = z_le + chord * (1.0 - CTRL_CHORD
                                         + CTRL_CHORD * max(t, 0.0))

        # spoiler: thin panel on top of the wing, ahead of the flap
        sp = make_wing_panel('spoiler_%s' % tag, side, 0.24, 0.56,
                             with_thickness=False)
        for v in sp.data.vertices:
            s = abs(v.co.x) / WING_SPAN
            chord = WING_ROOT + (WING_TIP - WING_ROOT) * s
            z_le = WING_Z + WING_SWEEP * s - chord * 0.5
            v.co.z = z_le + chord * 0.52
            v.co.y += 0.10
        add_material(sp, 'control', (0.72, 0.73, 0.76))

        # engine: closed cylinder + fan disc (no inner faces)
        eng = make_tube('engine_%s' % tag, side * ENG_X, ENG_Y, ENG_Z,
                        ENG_R, ENG_LEN)
        add_material(eng, 'engine', (0.42, 0.44, 0.47), metallic=0.8)

        # pylon: ties the engine to the body so it does not float
        pylon = make_tube('pylon_%s' % tag, side * (ENG_X * 0.55), ENG_Y,
                          ENG_Z, 0.16, ENG_LEN * 0.7, sides=6)
        add_material(pylon, 'skin', (0.86, 0.87, 0.89))

        fan = make_tube('fan_%s' % tag, side * ENG_X, ENG_Y,
                        ENG_Z - ENG_LEN * 0.48, ENG_R * 0.86, 0.06)
        add_material(fan, 'dark', (0.05, 0.05, 0.06), metallic=0.3)

        # main landing gear
        strut = make_tube('gear_%s' % tag, side * 1.15, -1.05, WING_Z + 0.2,
                          STRUT_R, 1.6, sides=10)
        add_material(strut, 'metal', (0.35, 0.36, 0.38), metallic=0.9)

        wheel = make_wheel('wheel_%s' % tag, side * 1.15, -1.85, WING_Z + 0.2)
        add_material(wheel, 'tyre', (0.06, 0.06, 0.07), metallic=0.0, rough=0.9)

    # --- tail ---
    ht = make_stab('stabilizer_right', HT_SPAN, HT_ROOT, HT_TIP, HT_SWEEP,
                   FUS_LEN * 0.5 - 1.0)
    add_material(ht, 'skin', (0.86, 0.87, 0.89))
    ht_l = mirror(ht, 'stabilizer_left')
    add_material(ht_l, 'skin', (0.86, 0.87, 0.89)) if not ht_l.data.materials else None

    elev = make_stab('elevator_right', HT_SPAN, HT_ROOT * 0.30, HT_TIP * 0.30,
                     HT_SWEEP, FUS_LEN * 0.5 - 1.0 + HT_ROOT * 0.36)
    add_material(elev, 'control', (0.78, 0.79, 0.82))
    mirror(elev, 'elevator_left')

    vt = make_stab('fin', VT_HEIGHT, VT_ROOT, VT_TIP, VT_SWEEP,
                   FUS_LEN * 0.5 - 1.6, vertical=True)
    add_material(vt, 'skin', (0.20, 0.34, 0.62))

    rud = make_stab('rudder', VT_HEIGHT * 0.96, VT_ROOT * 0.26,
                    VT_TIP * 0.26, VT_SWEEP,
                    FUS_LEN * 0.5 - 1.6 + VT_ROOT * 0.34, vertical=True)
    add_material(rud, 'control', (0.24, 0.38, 0.66))

    # nose gear
    ns = make_tube('gear_front', 0.0, -0.95, -FUS_LEN * 0.5 + 2.2,
                   STRUT_R * 0.8, 1.5, sides=10)
    add_material(ns, 'metal', (0.35, 0.36, 0.38), metallic=0.9)
    nw = make_wheel('wheel_front', 0.0, -1.62, -FUS_LEN * 0.5 + 2.2)
    add_material(nw, 'tyre', (0.06, 0.06, 0.07), metallic=0.0, rough=0.9)

    # Recompute the normals so they face outwards.
    # Trying to keep face orders right by hand was fragile: the fuselage
    # looked dark from above because some faces had inward normals.
    for ob in bpy.data.objects:
        if ob.type != 'MESH':
            continue
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.normals_make_consistent(inside=False)
        bpy.ops.object.mode_set(mode='OBJECT')
        ob.select_set(False)

    unwrap_all()

    tris = 0
    print('\n=== PARCALAR ===')
    for ob in sorted(bpy.data.objects, key=lambda o: o.name):
        if ob.type != 'MESH':
            continue
        n = sum(len(p.vertices) - 2 for p in ob.data.polygons)
        tris += n
        print('  %-20s %6d ucgen' % (ob.name, n))
    print('  %d triangles total' % tris)

    # From game axes to Blender axes: (x, y, z) -> (x, -z, y)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.transform.rotate(value=math.radians(90.0), orient_axis='X',
                             orient_type='GLOBAL')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.object.select_all(action='DESELECT')

    bpy.ops.export_scene.gltf(filepath=os.path.abspath(DST),
                              export_format='GLB',
                              export_yup=True,
                              export_apply=True)
    print('yazildi: %s' % os.path.abspath(DST))


main()
