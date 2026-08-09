"""Temiz bir is jeti modeli uretir - parcali, UV'li, PS3'e uygun.

    Blender --background --python tools/blender/build_aircraft.py

Neden kendi modelimizi uretiyoruz: hazir CC0 uclaklarda kumanda yuzeyleri
govdeye kaynak geliyor, UV'ler otomatik silindirik acilim oldugu icin livery
boyanamiyor ve govde iclerinde kapatilmamis yuzeyler kaliyor (motor icinin
gorunmesi bundandi). Burada her parca kendi nesnesi olarak DOGAR, UV'ler
duzlemsel/silindirik olarak bilincli acilir, ve hicbir yuzey ic tarafa
bakmaz.

Model OYUN eksenlerinde uretilir (+X sag, +Y yukari, -Z burun); disa
aktarmadan hemen once X ekseninde +90 derece dondurulur, cunku Blender
Z-yukari calisir ve glTF disa aktarici Blender eksenlerini bekler. Bu adim
atlandiginda uzunluk ile yukseklik yer degistiriyor.
Olculer metre; toplam uzunluk ~14 m, kanat acikligi ~13 m.
"""

import bpy
import bmesh
import math
import os

from mathutils import Vector

DST = os.path.join(os.path.dirname(__file__), '..', '..',
                   'assets', 'model', 'jet.glb')

# --- ana olculer (metre) ---
FUS_LEN      = 14.0     # govde uzunlugu
FUS_R        = 0.95     # govde yaricapi
NOSE_LEN     = 2.6
TAIL_LEN     = 4.2

WING_SPAN    = 6.3      # kok merkezinden uc noktaya
WING_ROOT    = 2.9      # kok kiris
WING_TIP     = 1.15     # uc kiris
WING_SWEEP   = 1.9      # uc, koke gore ne kadar geride
WING_DIHED   = 0.30     # uc, koke gore ne kadar yukarida
WING_THICK   = 0.16     # kirise oranla kalinlik
WING_Z       = 1.2      # kanadin govde uzerindeki konumu (+ arkaya)
WING_Y       = -0.35    # alcak kanat

CTRL_CHORD   = 0.26     # arka kenarin ne kadari kumanda yuzeyi
FLAP_SPAN    = (0.18, 0.62)     # kok orani araligi
AIL_SPAN     = (0.66, 0.96)

HT_SPAN      = 2.5      # yatay stabilizator
HT_ROOT      = 1.35
HT_TIP       = 0.75
HT_SWEEP     = 0.55
VT_HEIGHT    = 2.9      # dikey stabilizator
VT_ROOT      = 2.2
VT_TIP       = 1.15
VT_SWEEP     = 1.5

ENG_LEN      = 2.4
ENG_R        = 0.62
ENG_X        = 1.55
ENG_Y        = 0.35
ENG_Z        = 2.9

GEAR_R       = 0.34     # tekerlek yaricapi
GEAR_W       = 0.20


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
    """Basit simetrik kesit: ust ve alt yuzey icin y sapmasi.
    t: 0 (on kenar) .. 1 (arka kenar) """
    # NACA benzeri: kalinlik on kenarda hizli artar, arkaya dogru incelir
    y = 5.0 * thick * chord * (0.2969 * math.sqrt(max(t, 0.0))
                               - 0.1260 * t
                               - 0.3516 * t * t
                               + 0.2843 * t ** 3
                               - 0.1015 * t ** 4)
    return y


def make_wing_panel(name, side, t0, t1, with_thickness=True):
    """Kanadin [t0,t1] acikliginda bir dilimi. side: +1 sag, -1 sol.

    Kumanda yuzeyleri de ayni fonksiyondan cikar; boylece arka kenar
    kesiti govdeyle birebir ortusur ve arada bosluk kalmaz. """
    ob, bm = new_mesh(name)

    STEPS = 10          # kiris boyunca ornek sayisi
    segs = 6            # aciklik boyunca

    def station(s):
        """Aciklik orani s icin kok/uc arasi degerler."""
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
            # ust yuzey (disa bakan normal icin sira onemli)
            bm.faces.new((verts_top[i][j], verts_top[i + 1][j],
                          verts_top[i + 1][j + 1], verts_top[i][j + 1]))
            # alt yuzey
            bm.faces.new((verts_bot[i][j], verts_bot[i][j + 1],
                          verts_bot[i + 1][j + 1], verts_bot[i + 1][j]))

    # kok ve uc kapaklari: ic yuzey birakmamak icin
    for row_t, row_b, flip in ((verts_top[0], verts_bot[0], False),
                               (verts_top[-1], verts_bot[-1], True)):
        for j in range(STEPS):
            quad = (row_t[j], row_t[j + 1], row_b[j + 1], row_b[j])
            bm.faces.new(quad if flip else tuple(reversed(quad)))

    return finish(ob, bm)


def make_fuselage(name):
    """Govde: burun konik, orta silindirik, kuyruk yukari kalkan koni."""
    ob, bm = new_mesh(name)

    RINGS = 22
    SIDES = 16

    def profile(u):
        """u: 0 burun .. 1 kuyruk. (yaricap, y kaymasi) doner."""
        nose_end = NOSE_LEN / FUS_LEN
        tail_start = 1.0 - TAIL_LEN / FUS_LEN

        if u < nose_end:
            k = u / nose_end
            r = FUS_R * math.sin(k * math.pi * 0.5) ** 0.7
            dy = 0.0
        elif u > tail_start:
            k = (u - tail_start) / (1.0 - tail_start)
            r = FUS_R * (1.0 - k) ** 0.75
            dy = 0.55 * k * k          # kuyruk yukari kalkar
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

    # burun ve kuyruk kapaklari (ic bosluk kalmasin)
    nose_c = bm.verts.new((0.0, 0.0, -FUS_LEN * 0.5 - 0.15))
    tail_c = bm.verts.new((0.0, 0.55, FUS_LEN * 0.5 + 0.05))
    for j in range(SIDES):
        k = (j + 1) % SIDES
        bm.faces.new((nose_c, rings[0][k], rings[0][j]))
        bm.faces.new((tail_c, rings[-1][j], rings[-1][k]))

    return finish(ob, bm)


def make_tube(name, cx, cy, cz, radius, length, sides=14, closed=True):
    """Kapali silindir (motor govdesi, takim bacagi, tekerlek)."""
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
    """Tekerlek: X ekseni etrafinda donecek sekilde yatik silindir."""
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
    """Yatay veya dikey stabilizator (kumanda yuzeyi ayri uretilir)."""
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
    """X ekseninde aynala (sol kanat, sol motor...)."""
    new = ob.copy()
    new.data = ob.data.copy()
    new.name = name
    bpy.context.collection.objects.link(new)
    new.scale.x = -1.0
    bpy.context.view_layer.objects.active = new
    new.select_set(True)
    bpy.ops.object.transform_apply(scale=True)
    # aynalama yuz sirasini ters cevirir; normalleri geri duzelt
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.flip_normals()
    bpy.ops.object.mode_set(mode='OBJECT')
    new.select_set(False)
    return new


def unwrap_all():
    """Her nesneyi kendi UV alanina acar.

    Smart project, ada baslarina bosluk birakerek acar; livery boyamak icin
    yeterli, doku tekrari icin de kullanilabilir. """
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

    # --- kanatlar: sabit kisim + flap + aileron ayri ---
    for side, tag in ((1.0, 'right'), (-1.0, 'left')):
        main_panel = make_wing_panel('wing_%s' % tag, side, 0.06, 1.0)
        add_material(main_panel, 'skin', (0.86, 0.87, 0.89))

        flap = make_wing_panel('flap_%s' % tag, side,
                               FLAP_SPAN[0], FLAP_SPAN[1])
        add_material(flap, 'control', (0.78, 0.79, 0.82))

        ail = make_wing_panel('aileron_%s' % tag, side,
                              AIL_SPAN[0], AIL_SPAN[1])
        add_material(ail, 'control', (0.78, 0.79, 0.82))

        # kumanda yuzeylerini arka kenara tasi ve kirisini kisalt
        for ob, span in ((flap, FLAP_SPAN), (ail, AIL_SPAN)):
            for v in ob.data.vertices:
                s = abs(v.co.x) / WING_SPAN
                chord = WING_ROOT + (WING_TIP - WING_ROOT) * s
                z_le = WING_Z + WING_SWEEP * s - chord * 0.5
                t = (v.co.z - z_le) / chord
                # yalnizca arka %CTRL_CHORD'luk dilim kalsin
                v.co.z = z_le + chord * (1.0 - CTRL_CHORD
                                         + CTRL_CHORD * max(t, 0.0))

        # spoiler: kanat ustunde, flap'in onunde ince panel
        sp = make_wing_panel('spoiler_%s' % tag, side, 0.24, 0.56,
                             with_thickness=False)
        for v in sp.data.vertices:
            s = abs(v.co.x) / WING_SPAN
            chord = WING_ROOT + (WING_TIP - WING_ROOT) * s
            z_le = WING_Z + WING_SWEEP * s - chord * 0.5
            v.co.z = z_le + chord * 0.52
            v.co.y += 0.10
        add_material(sp, 'control', (0.72, 0.73, 0.76))

        # motor: kapali silindir + fan diski (ic yuzey yok)
        eng = make_tube('engine_%s' % tag, side * ENG_X, ENG_Y, ENG_Z,
                        ENG_R, ENG_LEN)
        add_material(eng, 'engine', (0.42, 0.44, 0.47), metallic=0.8)

        fan = make_tube('fan_%s' % tag, side * ENG_X, ENG_Y,
                        ENG_Z - ENG_LEN * 0.48, ENG_R * 0.86, 0.06)
        add_material(fan, 'dark', (0.05, 0.05, 0.06), metallic=0.3)

        # ana inis takimi
        strut = make_tube('gear_%s' % tag, side * 1.15, -1.15, WING_Z + 0.2,
                          0.10, 1.5, sides=8)
        add_material(strut, 'metal', (0.35, 0.36, 0.38), metallic=0.9)

        wheel = make_wheel('wheel_%s' % tag, side * 1.15, -1.85, WING_Z + 0.2)
        add_material(wheel, 'tyre', (0.06, 0.06, 0.07), metallic=0.0, rough=0.9)

    # --- kuyruk ---
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

    # burun takimi
    ns = make_tube('gear_front', 0.0, -1.05, -FUS_LEN * 0.5 + 2.2,
                   0.08, 1.3, sides=8)
    add_material(ns, 'metal', (0.35, 0.36, 0.38), metallic=0.9)
    nw = make_wheel('wheel_front', 0.0, -1.62, -FUS_LEN * 0.5 + 2.2)
    add_material(nw, 'tyre', (0.06, 0.06, 0.07), metallic=0.0, rough=0.9)

    unwrap_all()

    tris = 0
    print('\n=== PARCALAR ===')
    for ob in sorted(bpy.data.objects, key=lambda o: o.name):
        if ob.type != 'MESH':
            continue
        n = sum(len(p.vertices) - 2 for p in ob.data.polygons)
        tris += n
        print('  %-20s %6d ucgen' % (ob.name, n))
    print('  toplam %d ucgen' % tris)

    # Oyun eksenlerinden Blender eksenlerine: (x, y, z) -> (x, -z, y)
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
