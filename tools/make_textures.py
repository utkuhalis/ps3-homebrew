#!/usr/bin/env python3
"""Doku goruntulerini RSX'in okuyabilecegi ikili bicime cevirir.

Docker'daki ps3-assets imajinda calisir (bkz. Dockerfile.assets).

RSX kisitlari:
  - Kenar uzunluklari 2'nin kuvveti olmali
  - Dogrusal (linear) yerlesimde satirlar 64 bayta hizali olmali; bu bicimde
    genislik*4 zaten 64'un kati oldugu icin ek dolgu gerekmiyor
  - Bayt sirasi ARGB, big-endian (PPU ile ayni)

Cikti bicimi ("BWT1"):
    magic    "BWT1"        4 bayt
    doku sayisi            u32
    her doku icin:
        ad                 32 bayt
        genislik, yukseklik  u32, u32
        mip sayisi         u32
        piksel verisi      genislik*yukseklik*4 (her mip icin, kuculerek)

Mip zinciri uretilir: uzaktaki pist dokusunun titremesini (aliasing) onler.
"""

import os
import struct
import sys

from PIL import Image

# (cikti adi, dosya, hedef boyut)
TEXTURES = [
    ('runway',   'runway_asphalt_color.jpg',  512),
    ('apron',    'apron_concrete_color.jpg',  512),
    ('grass',    'airfield_grass_color.jpg',  512),
    ('fuselage', 'fuselage_panel_color.jpg',  512),
]

MAX_NAME = 32


def is_pow2(n):
    return n > 0 and (n & (n - 1)) == 0


def build_mips(img):
    """Tam mip zinciri: 512, 256, 128 ... 1"""
    mips = [img]
    w, h = img.size
    while w > 1 or h > 1:
        w = max(1, w // 2)
        h = max(1, h // 2)
        mips.append(mips[-1].resize((w, h), Image.LANCZOS))
    return mips


def pack(img):
    """ARGB, big-endian bayt dizisi"""
    out = bytearray()
    for r, g, b, a in img.convert('RGBA').getdata():
        out += bytes((a, r, g, b))
    return bytes(out)


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else 'assets/texture'
    dst = sys.argv[2] if len(sys.argv) > 2 else 'data/textures.bin'

    entries = []
    for name, filename, size in TEXTURES:
        path = os.path.join(src_dir, filename)
        if not os.path.exists(path):
            print('  atlandi (yok): %s' % filename)
            continue

        img = Image.open(path).convert('RGBA')
        if img.size != (size, size):
            img = img.resize((size, size), Image.LANCZOS)

        if not (is_pow2(img.size[0]) and is_pow2(img.size[1])):
            raise SystemExit('%s: kenarlar 2 kuvveti olmali' % filename)

        mips = build_mips(img)
        data = b''.join(pack(m) for m in mips)
        entries.append((name, img.size[0], img.size[1], len(mips), data))
        print('  %-10s %4dx%-4d  %d mip  %6.1f KB'
              % (name, img.size[0], img.size[1], len(mips), len(data) / 1024.0))

    os.makedirs(os.path.dirname(dst) or '.', exist_ok=True)
    with open(dst, 'wb') as f:
        f.write(b'BWT1')
        f.write(struct.pack('>I', len(entries)))
        for name, w, h, nmip, data in entries:
            f.write(name.encode('ascii').ljust(MAX_NAME, b'\0'))
            f.write(struct.pack('>III', w, h, nmip))
            f.write(data)
            pad = (-f.tell()) % 4
            if pad:
                f.write(b'\0' * pad)

    total = sum(len(e[4]) for e in entries)
    print('yazildi: %s  (%d doku, %.1f KB)' % (dst, len(entries), total / 1024.0))


main()
