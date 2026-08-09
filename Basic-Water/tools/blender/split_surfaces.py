"""plane.glb icindeki kumanda yuzeylerini ayri nesnelere boler.

Headless Blender ile calisir; BlenderMCP eklentisine gerek yoktur:

    Blender --background --python tools/blender/split_surfaces.py

Model tek bir govde mesh'i olarak geldigi icin flap, aileron, spoiler,
elevator ve rudder geometrik olarak secilir: her yuzun merkezi, o yuzeyin
bulunmasi gereken bolgeye dusuyorsa secilir ve mesh'ten ayrilir.

Blender eksenleri (import sonrasi, Z-yukari):
    -Y burun,  +Y kuyruk,  X kanat acikligi,  Z yukari
Olculer tools/blender ile yapilan bolge analizinden alindi:
    kanat duzlemi     Z ~ -1.5,  Y 0.0 .. 4.15  (arka kenar ~4.1)
    kanat ucu         |X| ~ 9.8
    dikey kuyruk      Y 7.7 .. 10.9,  Z 0.8 .. 3.2
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

# Kanat supurulmus: arka kenarin Y'si acikliga gore kayiyor. Olcum:
#   |X|=4 -> Y=2.41 ,  |X|=9 -> Y=4.12   =>  Y_te = 0.342*|X| + 1.04
def wing_trailing_y(x):
    return 0.342 * abs(x) + 1.04


# Kanat arka kenari: kanat kirisinin son diliminde kalan yuzler
WING_Z = (-2.35, -0.85)
TRAIL_Y = (3.55, 4.40)          # arka kenar seridi
SPOIL_Y = (2.70, 3.55)          # arka kenarin hemen onu (ust yuzey)

def bisect_wing(obj, sign, offset):
    """Kanadi supurmeye uyan egik bir duzlemle keser.

    Kanat panelleri az poligonlu oldugu icin arka kenarda ayrilacak hazir yuz
    yok; once duzlemle kesip yeni kenar olusturmak gerekiyor. Duzlem:
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

    # Yalnizca ilgili kanattaki geometri kesilir
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
    # (ad, test fonksiyonu)
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
    # Dikey kuyrugun arka kenari
    ('rudder',        lambda c, n: (abs(c.x) < 0.65 and
                                    c.y > 10.10 and c.z > 0.85)),
]


def load():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.abspath(SRC))
    return bpy.data.objects[BODY]


def split(obj, name, test):
    """test'e uyan yuzleri ayri bir nesneye ayirir; sayiyi doner."""
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


WHEEL_MATERIALS = ('Material.005', 'Material.006')   # lastik ve jant


def split_wheels():
    """Tekerlek gobegini suspansiyon bacagindan ayirir.

    Lastik ve jant kendi materyallerini kullaniyor, bacak baskasini; ayirmadan
    tekerlegi dondurmek bacagi da dondururdu.

    Kaynak nesneler ISIMLE degil, ICERIKLE bulunur: nesne adlari ice aktarma
    ve onceki ayirmalar sirasinda kayiyor, sabit isme guvenmek kirilgandi.
    Olcut: tekerlek materyali iceren ve govde disinda kalan nesneler; sag/sol/
    burun ayrimi bounding box merkezinden gelir. """
    from mathutils import Vector

    # Hedefler ONCE belirlenir: ayirma sirasinda bpy.data.objects degisiyor
    # ve isimler kayiyor, uzerinde gezerken islem yapmak guvenli degil.
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

        # Secimi temizlemeden EDIT moduna girmek tehlikeli: mesh.separate
        # SECILI TUM nesnelerde calisiyor ve govdeyi de parcaliyordu.
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

        # Yeni nesne ISIMLE degil REFERANSLA bulunur: isim kaymasi
        # yanlis nesneyi yeniden adlandiriyordu (govde 'wheel_front'
        # olarak isaretlenmisti).
        before = set(bpy.data.objects.values())
        bpy.ops.mesh.separate(type='SELECTED')
        bpy.ops.object.mode_set(mode='OBJECT')

        fresh = [x for x in bpy.data.objects.values() if x not in before]
        src_name = o.name
        if fresh:
            fresh[0].name = name
            print('  %-14s -> %-13s %5d yuz (merkez X %.2f)'
                  % (src_name, name, n, cx))


def main():
    body = load()

    # Once kanatlari arka kenar cizgisinden kes, sonra parcalari ayir
    print('\n=== TEKERLEKLER AYRILIYOR ===')
    split_wheels()

    print('\n=== KANATLAR KESILIYOR ===')
    bisect_wing(body, -1.0, 0.55)
    bisect_wing(body,  1.0, 0.55)
    print('  kesim tamam, yuz sayisi %d' % len(body.data.polygons))

    print('\n=== KUMANDA YUZEYLERI AYRILIYOR ===')

    for name, test in REGIONS:
        n = split(body, name, test)
        print('  %-16s %5d yuz' % (name, n))

    print('\n=== SONUC ===')
    total = 0
    for o in bpy.data.objects:
        if o.type == 'MESH':
            print('  %-16s %6d yuz' % (o.name, len(o.data.polygons)))
            total += len(o.data.polygons)
    print('  toplam %d yuz' % total)

    bpy.ops.export_scene.gltf(filepath=os.path.abspath(DST),
                              export_format='GLB',
                              export_yup=True,
                              export_apply=False)
    print('yazildi: %s' % os.path.abspath(DST))


main()
