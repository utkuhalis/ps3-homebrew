#!/usr/bin/env python3
"""GLB (glTF 2.0 ikili) -> Basic Water mesh formati.

Harici bagimlilik yok: GLB zaten JSON + ham tampon, saf Python ile okunur.
Cikti, PS3 tarafinda dogrudan vertex tamponuna kopyalanabilecek duz bir
ikili dosyadir (big-endian, PowerPC ile ayni siralama).

Bicim:
    magic   "BWM2"                       4 bayt
    parca sayisi                         u32
    her parca icin:
        ad                               32 bayt (null ile doldurulmus)
        vertex sayisi                    u32
        index sayisi                     u32
        vertexler: x,y,z, nx,ny,nz, r,g,b, metallic  10 x float32
        indexler:  u16
        (parca sonu 4 bayta hizalanir)

Eksenler: glTF'in kendi konvansiyonu (+Y yukari, -Z ileri) oyunla birebir
aynidir, bu yuzden ek bir eksen cevirmesi YAPILMAZ. Blender'in Z-yukari
duzeni zaten kok node'un donusumunde (X ekseninde -90 derece) kodludur.

Node hiyerarsisi rekursif gezilir ve ebeveyn donusumleri biriktirilir;
bu model tek kok ("Air Plane") altinda toplandigi icin duz gezmek yanlis
sonuc veriyordu. --flip burun yonunu ters cevirir.
"""

import json
import struct
import sys

COMP_SIZE = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
COMP_FMT = {5120: 'b', 5121: 'B', 5122: 'h', 5123: 'H', 5125: 'I', 5126: 'f'}
NUM_COMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def read_glb(path):
    data = open(path, 'rb').read()
    magic, ver, _ = struct.unpack('<III', data[:12])
    if magic != 0x46546C67:
        raise SystemExit('GLB degil: %s' % path)
    off = 12
    js = None
    bin_ = b''
    while off < len(data):
        clen, ctype = struct.unpack('<II', data[off:off + 8])
        chunk = data[off + 8:off + 8 + clen]
        if ctype == 0x4E4F534A:
            js = json.loads(chunk)
        elif ctype == 0x004E4942:
            bin_ = chunk
        off += 8 + clen + (-clen % 4)
    return js, bin_


def accessor(js, bin_, index):
    a = js['accessors'][index]
    n = NUM_COMP[a['type']]
    ctype = a['componentType']
    fmt = COMP_FMT[ctype]
    size = COMP_SIZE[ctype] * n

    if 'bufferView' not in a:
        return [(0,) * n] * a['count']

    bv = js['bufferViews'][a['bufferView']]
    base = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = bv.get('byteStride', size)

    out = []
    for i in range(a['count']):
        o = base + i * stride
        out.append(struct.unpack_from('<' + fmt * n, bin_, o))
    return out


def node_matrix(node):
    """Node'un yerel donusum matrisi (4x4, satir oncelikli)."""
    if 'matrix' in node:
        m = node['matrix']          # glTF sutun oncelikli
        return [[m[0], m[4], m[8], m[12]],
                [m[1], m[5], m[9], m[13]],
                [m[2], m[6], m[10], m[14]],
                [m[3], m[7], m[11], m[15]]]

    t = node.get('translation', [0, 0, 0])
    r = node.get('rotation', [0, 0, 0, 1])      # x,y,z,w
    s = node.get('scale', [1, 1, 1])

    x, y, z, w = r
    rot = [
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ]
    m = [[rot[i][j] * s[j] for j in range(3)] + [t[i]] for i in range(3)]
    m.append([0, 0, 0, 1])
    return m


def apply(m, v, is_point=True):
    w = 1.0 if is_point else 0.0
    return tuple(
        m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2] + m[i][3] * w
        for i in range(3))


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)]
            for i in range(4)]


def to_game_axes(v, flip):
    """glTF ve oyun ayni konvansiyonu kullanir; --flip burnu ters cevirir."""
    x, y, z = v
    return (-x, y, -z) if flip else (x, y, z)


def material_metallic(js, index):
    """Materyalin metaliklik degeri; shader yansimayi buna gore olceklendirir.

    Govde ve motor kaportasi metalik, lastik ve kaucuk degil. Bu ayrim
    olmadan tum ucak ayni parlakliga sahip oluyor ve plastik gibi duruyor."""
    if index is None:
        return 0.0

    mat = js['materials'][index]
    if mat.get('name') == 'glass':
        return 0.85

    pbr = mat.get('pbrMetallicRoughness', {})
    metal = pbr.get('metallicFactor', 0.0)
    rough = pbr.get('roughnessFactor', 0.5)

    # Puruzluluk yansimayi bir miktar kirar ama tamamen yok etmez.
    return max(0.0, min(1.0, metal * (1.0 - rough * 0.35)))


def material_color(js, index):
    """glTF materyalinden vertex rengi.

    Yalnizca baseColorFactor'a bakmak yetmiyordu: bu modelde isik yayan
    parcalarin (pencere/farlar) taban rengi [0,0,0] ve renk yalnizca
    emissiveFactor'da duruyor - o parcalar bu yuzden simsiyah ciziliyordu.
    Cam materyalinin ise hic degeri yok, varsayilana dusuyor.

    Metalik yuzeyler biraz aydinlatilir: gercek bir yansima hesabimiz
    olmadigi icin tamamen mat cikiyorlar. """
    if index is None:
        return (0.8, 0.8, 0.8)

    mat = js['materials'][index]
    pbr = mat.get('pbrMetallicRoughness', {})
    c = list(pbr.get('baseColorFactor', [1.0, 1.0, 1.0, 1.0])[:3])
    emis = mat.get('emissiveFactor', [0.0, 0.0, 0.0])
    metal = pbr.get('metallicFactor', 0.0)

    if mat.get('name') == 'glass':
        return (0.42, 0.55, 0.68)

    # isik yayan parcalar taban rengin yerine emissive ile cizilir
    for k in range(3):
        if emis[k] > c[k]:
            c[k] = emis[k]

    # metalik yuzeylere hafif bir taban parlaklik
    for k in range(3):
        c[k] = c[k] * (1.0 - 0.35 * metal) + 0.35 * metal * 0.72
        # tamamen siyah kalan yuzeyler govdeden ayirt edilemiyor
        if c[k] < 0.05:
            c[k] = 0.05

    return tuple(c)


def walk(js, node_idx, parent, out):
    """Node agacini gezip her mesh icin birikmis donusumu toplar."""
    node = js['nodes'][node_idx]
    m = mat_mul(parent, node_matrix(node))

    if 'mesh' in node:
        out.append((node.get('name', 'part'), node['mesh'], m))
    for c in node.get('children', []):
        walk(js, c, m, out)


def convert(src, dst, flip=False, scale=1.0):
    js, bin_ = read_glb(src)
    parts = []

    ident = [[1 if i == j else 0 for j in range(4)] for i in range(4)]
    found = []
    scene = js['scenes'][js.get('scene', 0)]
    for root in scene['nodes']:
        walk(js, root, ident, found)

    for name, mesh_idx, mat in found:
        mesh = js['meshes'][mesh_idx]
        verts = []
        idx = []

        for prim in mesh['primitives']:
            attrs = prim['attributes']
            pos = accessor(js, bin_, attrs['POSITION'])
            nrm = (accessor(js, bin_, attrs['NORMAL'])
                   if 'NORMAL' in attrs else [(0, 1, 0)] * len(pos))

            color = material_color(js, prim.get('material'))
            metallic = material_metallic(js, prim.get('material'))

            # Vertex renkleri varsa onlar oncelikli: doku bilgisi bunlara
            # islenmis oluyor (bkz. tools/blender/import_737.py). Alfa
            # kanali metakligi tasir.
            vcol = None
            if 'COLOR_0' in attrs:
                vcol = accessor(js, bin_, attrs['COLOR_0'])

            base = len(verts)
            for vi, (p, n) in enumerate(zip(pos, nrm)):
                wp = to_game_axes(apply(mat, p), flip)
                wn = to_game_axes(apply(mat, n, False), flip)
                ln = (wn[0] ** 2 + wn[1] ** 2 + wn[2] ** 2) ** 0.5 or 1.0
                cr, cg, cb, cm = color[0], color[1], color[2], metallic
                if vcol is not None and vi < len(vcol):
                    c = vcol[vi]
                    # unsigned short/byte gelirse 0..1'e olceklenir
                    if isinstance(c[0], int):
                        div = 65535.0 if max(c) > 255 else 255.0
                        c = tuple(x / div for x in c)
                    cr, cg, cb = c[0], c[1], c[2]
                    if len(c) > 3:
                        cm = c[3]

                verts.append((wp[0] * scale, wp[1] * scale, wp[2] * scale,
                              wn[0] / ln, wn[1] / ln, wn[2] / ln,
                              cr, cg, cb, cm))

            if 'indices' in prim:
                for i in accessor(js, bin_, prim['indices']):
                    idx.append(base + i[0])
            else:
                idx.extend(range(base, base + len(pos)))

        if len(verts) == 0 or len(idx) == 0:
            continue        # Blender ayirmasindan kalan bos kabuk

        if len(verts) > 65535:
            print('  UYARI: %s %d vertex - u16 index sinirini asiyor, atlandi'
                  % (name, len(verts)))
            continue

        parts.append((name[:31], verts, idx))
        print('  %-16s vertex=%6d ucgen=%6d'
              % (name, len(verts), len(idx) // 3))

    with open(dst, 'wb') as f:
        f.write(b'BWM2')
        f.write(struct.pack('>I', len(parts)))
        for name, verts, idx in parts:
            f.write(name.encode('ascii', 'replace').ljust(32, b'\0'))
            f.write(struct.pack('>II', len(verts), len(idx)))
            for v in verts:
                f.write(struct.pack('>10f', *v))
            for i in idx:
                f.write(struct.pack('>H', i))
            # Sonraki parcanin vertex dizisi 4 bayta hizali baslamali:
            # PPU hizasiz float okumasinda tuzaga duser.
            pad = (-f.tell()) % 4
            if pad:
                f.write(b'\0' * pad)

    total = sum(len(i) // 3 for _, _, i in parts)
    print('yazildi: %s  (%d parca, %d ucgen)' % (dst, len(parts), total))
    return parts


if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit(
            'kullanim: glb_to_mesh.py <girdi.glb> <cikti.msh> [--flip] '
            '[--scale N]')
    fl = '--flip' in sys.argv
    sc = 1.0
    if '--scale' in sys.argv:
        sc = float(sys.argv[sys.argv.index('--scale') + 1])
    convert(sys.argv[1], sys.argv[2], fl, sc)
