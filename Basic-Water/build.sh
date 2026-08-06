#!/bin/sh
# Basic Water - derleme (her sey Docker icinde, host'a hicbir sey kurulmaz)
#
#   ./build.sh          -> .self + .pkg uretir
#   ./build.sh clean    -> ciktilar temizlenir
#   ./build.sh test     -> matematik ve kamera birim testlerini calistirir
#
# Iki asamali derleme:
#   1) Cg shader'lari amd64 imajinda derlenir (Cg Toolkit yalnizca x86 icin var)
#   2) Asil PS3 derlemesi native arm64 ps3dev imajinda yapilir
set -e

PS3_IMAGE=zeldin/ps3dev-docker:latest
CG_IMAGE=ps3-cgcomp:latest
cd "$(dirname "$0")"

if ! docker image inspect "$CG_IMAGE" >/dev/null 2>&1; then
    echo ">>> Shader derleyici imaji olusturuluyor (tek seferlik)."
    docker build --platform linux/amd64 -f Dockerfile.shaders -t "$CG_IMAGE" .
fi

if ! docker image inspect "$PS3_IMAGE" >/dev/null 2>&1; then
    echo ">>> PS3 toolchain imaji indiriliyor (~400 MB, tek seferlik)."
    docker pull --platform linux/arm64 "$PS3_IMAGE"
fi

PS3_RUN="docker run --rm --platform linux/arm64 -v $PWD:/project -w /project $PS3_IMAGE"
CG_RUN="docker run --rm --platform linux/amd64 -v $PWD:/project -w /project $CG_IMAGE"

if [ "$1" = "test" ]; then
    echo ">>> Matematik ve kamera testleri (host tarafi, PS3 gerekmez)"
    $PS3_RUN sh -c '
        mkdir -p build-test &&
        gcc -O1 -Wall -Wextra -o build-test/test_math tests/test_math.c \
            source/mat4.c source/camera.c -lm &&
        ./build-test/test_math &&
        gcc -O1 -Wall -Wextra -o build-test/test_menu tests/test_menu.c \
            tests/menu_stubs.c source/gamemenu.c -lm &&
        ./build-test/test_menu &&
        gcc -O1 -Wall -Wextra -o build-test/test_atm tests/test_atmosphere.c \
            source/atmosphere.c -lm &&
        ./build-test/test_atm'
    exit $?
fi

if [ "$1" = "clean" ]; then
    $PS3_RUN sh -c 'export PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev; make clean'
    rm -rf build-test
    exit 0
fi

# 1) Cg shader'lari -> RSX mikrokodu (.vpo/.fpo). Ciktilar mimariden bagimsizdir.
echo ">>> Shader'lar derleniyor (Cg)"
mkdir -p build
for f in shaders/*.vcg; do
    [ -e "$f" ] || continue
    out="build/$(basename "${f%.vcg}").vpo"
    $CG_RUN cgcomp -v "$f" "$out"
    echo "    $(basename "$f") -> $(basename "$out")"
done
for f in shaders/*.fcg; do
    [ -e "$f" ] || continue
    out="build/$(basename "${f%.fcg}").fpo"
    $CG_RUN cgcomp -f "$f" "$out"
    echo "    $(basename "$f") -> $(basename "$out")"
done

# 2) Asil derleme. Shader'lar zaten guncel oldugu icin make cgcomp'u cagirmaz
#    (arm64'te Cg kutuphanesi olmadigindan cagirmamasi sart).
echo ">>> PS3 derlemesi"
$PS3_RUN sh -c '
    export PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev
    export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin
    make'

echo
echo ">>> Ciktilar:"
ls -lh basicwater.self basicwater.fake.self basicwater.pkg 2>/dev/null || true
echo
echo "  RPCS3 (Mac)  : basicwater.fake.self  -> emulatore surukle"
echo "  Gercek PS3   : basicwater.pkg        -> USB ile kur"
