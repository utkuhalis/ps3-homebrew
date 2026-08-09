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


def main():
    body = load()

    # Once kanatlari arka kenar cizgisinden kes, sonra parcalari ayir
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
