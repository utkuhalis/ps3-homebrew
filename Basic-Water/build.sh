#!/bin/sh
# Basic Water - derleme (her sey Docker icinde, host'a hicbir sey kurulmaz)
#
#   ./build.sh          -> .self + .pkg uretir
#   ./build.sh clean    -> ciktilar temizlenir
#   ./build.sh test     -> matematik ve kamera birim testlerini calistirir
#
#   ./build.sh gonder <PS3_IP>    -> .pkg'yi FTP ile PS3'e kopyalar (kalici kurulum)
#   ./build.sh calistir <PS3_IP>  -> .self'i ps3load ile dogrudan calistirir (hizli deneme)
#   ./build.sh log <PS3_IP>       -> PS3'ten teshis kaydini ceker
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
        ./build-test/test_atm &&
        gcc -O1 -Wall -Wextra -o build-test/test_flight tests/test_flight.c \
            source/flight.c -lm &&
        ./build-test/test_flight'
    exit $?
fi

# --- PS3'e gonderme ---
#   ./build.sh gonder <PS3_IP>    : .pkg'yi FTP ile PS3'e kopyalar
#   ./build.sh calistir <PS3_IP>  : .self'i ps3load ile dogrudan calistirir
if [ "$1" = "gonder" ]; then
    IP="$2"
    [ -n "$IP" ] || { echo "Kullanim: ./build.sh gonder <PS3_IP>"; exit 1; }
    [ -f basicwater.pkg ] || { echo "Once ./build.sh ile derleyin"; exit 1; }

    echo ">>> $IP adresine gonderiliyor (FTP)"
    if curl -T basicwater.pkg "ftp://$IP/dev_hdd0/packages/" --connect-timeout 10; then
        echo
        echo "Gonderildi. PS3'te:"
        echo "  Package Manager > Install Package Files > basicwater.pkg"
    else
        echo
        echo "Gonderilemedi. Kontrol edin:"
        echo "  - PS3 ve Mac ayni agda mi"
        echo "  - PS3'te FTP acik mi (webMAN / multiMAN)"
        echo "  - IP adresi dogru mu (Ayarlar > Ag Ayarlari > Baglanti Durumu)"
        exit 1
    fi
    exit 0
fi

if [ "$1" = "log" ]; then
    IP="$2"
    [ -n "$IP" ] || { echo "Kullanim: ./build.sh log <PS3_IP>"; exit 1; }

    echo ">>> PS3'ten teshis kaydi aliniyor"
    if curl -s --connect-timeout 10 "ftp://$IP/dev_hdd0/tmp/basicwater.log" \
            -o /tmp/basicwater-ps3.log; then
        echo "--- PS3 kaydi ---"
        cat /tmp/basicwater-ps3.log
        echo "--- son ---"
    else
        echo "Kayit alinamadi. Oyunu bir kez calistirdiniz mi?"
        echo "FTP acik mi ve IP dogru mu kontrol edin."
        exit 1
    fi
    exit 0
fi

if [ "$1" = "calistir" ]; then
    IP="$2"
    [ -n "$IP" ] || { echo "Kullanim: ./build.sh calistir <PS3_IP>"; exit 1; }
    [ -f basicwater.self ] || { echo "Once ./build.sh ile derleyin"; exit 1; }

    echo ">>> $IP adresinde dogrudan calistiriliyor (ps3load)"
    echo "    PS3'te ps3load / SELF yukleyici acik olmali."
    $PS3_RUN sh -c "export PS3LOAD=tcp:$IP; /usr/local/ps3dev/bin/ps3load basicwater.self"
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
