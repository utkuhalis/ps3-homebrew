#!/bin/sh
# PS3 Ping Pong - derleme (her sey Docker icinde, host'a hicbir sey kurulmaz)
#
#   ./build.sh          -> .self + .pkg uretir
#   ./build.sh clean    -> ciktilar temizlenir
#   ./build.sh test     -> oyun mantigi birim testlerini calistirir
set -e

IMAGE=ps3dev-pingpong:latest
cd "$(dirname "$0")"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo ">>> '$IMAGE' bulunamadi. Toolchain build ediliyor."
    echo ">>> Hazir toolchain imaji indiriliyor (~400 MB, tek seferlik)."
    docker build --platform linux/arm64 -t "$IMAGE" .
fi

DOCKER_RUN="docker run --rm --platform linux/arm64 -v $PWD:/project -w /project $IMAGE"

if [ "$1" = "test" ]; then
    echo ">>> Oyun mantigi testleri (host tarafi, PS3 gerekmez)"
    $DOCKER_RUN sh -c '
        mkdir -p build-test &&
        gcc -O1 -Wall -Wextra -o build-test/test_game tests/test_game.c source/game.c -lm &&
        ./build-test/test_game'
    exit $?
fi

# Gorseller (assets/*.png) ham piksele cevrilip data/ altina yazilir.
./tools/convert_assets.sh

$DOCKER_RUN make "$@"

if [ "$1" != "clean" ]; then
    echo
    echo ">>> Ciktilar:"
    ls -lh pingpong.self pingpong.fake.self pingpong.pkg 2>/dev/null || true
    echo
    echo "  RPCS3 (Mac)  : pingpong.fake.self  -> emulatore surukle"
    echo "  Gercek PS3   : pingpong.pkg        -> USB ile kur"
fi
