#!/usr/bin/env python3
"""Converts texture images into a binary format RSX can read.

Runs in the ps3-assets Docker image (see Dockerfile.assets).

RSX constraints:
  - Edge lengths must be powers of two
  - In linear layout rows must be 64-byte aligned; width*4 is already a
    multiple of 64 here, so no padding is needed
  - Byte order ARGB, big-endian (same as the PPU)

Output format ("BWT1"):
    magic    "BWT1"        4 bytes
    texture count          u32
    for each texture:
        name               32 bytes
        width, height      u32, u32
        mip count          u32
        pixel data         width*height*4 (for each mip, shrinking)

A mip chain is generated: it keeps the distant runway texture from
shimmering (aliasing).
"""

import os
import struct
import sys

from PIL import Image

# (output name, file, target size)
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
    """Full mip chain: 512, 256, 128 ... 1"""
    mips = [img]
    w, h = img.size
    while w > 1 or h > 1:
        w = max(1, w // 2)
        h = max(1, h // 2)
        mips.append(mips[-1].resize((w, h), Image.LANCZOS))
    return mips


def pack(img):
    """ARGB, big-endian byte array"""
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
            print('  skipped (missing): %s' % filename)
            continue

        img = Image.open(path).convert('RGBA')
        if img.size != (size, size):
            img = img.resize((size, size), Image.LANCZOS)

        if not (is_pow2(img.size[0]) and is_pow2(img.size[1])):
            raise SystemExit('%s: edges must be powers of two' % filename)

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
    print('written: %s  (%d textures, %.1f KB)' % (dst, len(entries), total / 1024.0))


main()
