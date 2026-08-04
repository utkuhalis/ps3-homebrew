#!/bin/sh
# assets/*.png -> data/*.bin  (ham piksel verisi)
#
# PS3'te PNG cozucu kullanmiyoruz: gorseller derleme sirasinda ham piksele
# cevrilip programa gomuluyor. Boylece konsolda cozme maliyeti ve ek kutuphane
# bagimliligi olmuyor.
#
# Arkaplan : 1280x720, XRGB8888 (alfa yok) - dogrudan framebuffer'a kopyalanir
# Digerleri: ARGB8888 (alfa var) - alfa harmanlamasiyla cizilir
set -e
cd "$(dirname "$0")/.."
mkdir -p data

run() { docker run --rm --platform linux/arm64 -v "$PWD":/p -w /p alpine-assets "$@"; }

if ! docker image inspect alpine-assets >/dev/null 2>&1; then
    printf 'FROM alpine:3.20\nRUN apk add --no-cache imagemagick python3\n' \
        | docker build --platform linux/arm64 -t alpine-assets -f - . >/dev/null
fi

convert_one() {
    src="$1"; dst="$2"; w="$3"; h="$4"; mode="$5"
    [ -f "$src" ] || { echo "  atlandi (yok): $src"; return 0; }

    # 16:9'a ortadan kirp + hedef boyuta olcekle, ham RGBA uret
    run magick "$src" -resize "${w}x${h}^" -gravity center -extent "${w}x${h}" \
        -depth 8 RGBA:/p/data/.tmp.rgba

    run python3 -c "
import sys
raw = open('data/.tmp.rgba','rb').read()
n = len(raw) // 4
out = bytearray(n * 4)
# PS3 big-endian: bellekte bayt sirasi A,R,G,B olacak sekilde yaziyoruz
out[1::4] = raw[0::4]   # R
out[2::4] = raw[1::4]   # G
out[3::4] = raw[2::4]   # B
if '$mode' == 'alpha':
    out[0::4] = raw[3::4]
else:
    out[0::4] = b'\x00' * n
open('$dst','wb').write(bytes(out))
print('  %s -> %s (%dx%d, %.1f MB)' % ('$src', '$dst', $w, $h, len(out)/1048576.0))
"
    rm -f data/.tmp.rgba
}

echo ">>> Gorseller donusturuluyor"
convert_one assets/arkaplan.png data/arkaplan.bin 1280 720 opaque
convert_one assets/top.png      data/top.bin        64  64 alpha
convert_one assets/raket.png    data/raket.bin      64 384 alpha
