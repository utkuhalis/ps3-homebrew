"""AirportPack'teki 737 modelini oyunun parca duzenine cevirir.

    Blender --background --python tools/blender/import_737.py

Model zaten ayri parcalardan olusuyor (aileron, flap, rudder, speed brake,
inis takimi, motor fani, kapilar). Burada yalnizca isimler oyunun bekledigi
duzene esleniyor ve eksenler cevriliyor.

TELIF: bu varlik ticari bir Unity paketinden gelir. Cikti dosyasi depoya
KONULMAZ (.gitignore); yalnizca yerel derlemede kullanilir.
"""

import bpy
import math
import os

SRC = ("/Users/dc/Desktop/Projects/Games/unity-test/Gozcu-v1/Assets/"
       "AirportPack/Meshes/SM_boeing737.fbx")
DST = os.path.join(os.path.dirname(__file__), '..', '..',
                   'assets', 'model', 'boeing737.glb')

PREFIX = 'SM_boeing737'


def game_name(raw):
    """Paket adini oyunun bekledigi parca adina cevirir."""
    n = raw
    if n.startswith(PREFIX):
        n = n[len(PREFIX):]
    n = n.lstrip('_')
    n = n.split('.')[0]              # Blender'in .001 ekleri

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


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SRC)

    used = {}
    for ob in list(bpy.data.objects):
        if ob.type != 'MESH':
            bpy.data.objects.remove(ob, do_unlink=True)
            continue
        base = game_name(ob.name)
        # Ayni ada dusen parcalar (flap_left01/02...) numaralandirilir
        k = used.get(base, 0)
        used[base] = k + 1
        ob.name = base if k == 0 else '%s_%d' % (base, k)

    # FBX Z-yukari geliyor; glTF disa aktarici Blender eksenlerini bekledigi
    # icin dondurmeye gerek yok - import zaten Blender duzenine getirdi.
    tot = 0
    print('\n=== ESLENEN PARCALAR ===')
    for ob in sorted(bpy.data.objects, key=lambda o: o.name):
        if ob.type != 'MESH':
            continue
        n = sum(len(p.vertices) - 2 for p in ob.data.polygons)
        tot += n
        print('  %-26s %6d ucgen' % (ob.name, n))
    print('  toplam %d ucgen, %d parca'
          % (tot, len([o for o in bpy.data.objects if o.type == 'MESH'])))

    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.export_scene.gltf(filepath=os.path.abspath(DST),
                              export_format='GLB',
                              export_yup=True,
                              export_apply=True)
    print('yazildi: %s' % os.path.abspath(DST))


main()
