#!/bin/sh
# Basic Water - derleme (her sey Docker icinde, host'a hicbir sey kurulmaz)
#
#   ./build.sh          -> .self + .pkg uretir
#   ./build.sh clean    -> ciktilar temizlenir
#   ./build.sh test     -> matematik ve kamera birim testlerini calistirir
#
#   ./build.sh send <PS3_IP>      -> upload the .pkg to the console over FTP
#   ./build.sh run <PS3_IP>       -> run the .self directly via ps3load
#   ./build.sh log <PS3_IP>       -> fetch the console's diagnostic log
#   (gonder / calistir also work, for compatibility)
#
# Iki asamali derleme:
#   1) Cg shader'lari amd64 imajinda derlenir (Cg Toolkit yalnizca x86 icin var)
#   2) Asil PS3 derlemesi native arm64 ps3dev imajinda yapilir
set -e

PS3_IMAGE=zeldin/ps3dev-docker:latest
CG_IMAGE=ps3-cgcomp:latest
ASSET_IMAGE=ps3-assets:latest
cd "$(dirname "$0")"

if ! docker image inspect "$CG_IMAGE" >/dev/null 2>&1; then
    echo ">>> Shader derleyici imaji olusturuluyor (tek seferlik)."
    docker build --platform linux/amd64 -f Dockerfile.shaders -t "$CG_IMAGE" .
fi

if [ -d assets/texture ] && ! docker image inspect "$ASSET_IMAGE" >/dev/null 2>&1; then
    echo ">>> Varlik donusturme imaji olusturuluyor (tek seferlik)."
    docker build --quiet -f Dockerfile.assets -t "$ASSET_IMAGE" . >/dev/null
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
        ./build-test/test_flight &&
        gcc -O1 -Wall -Wextra -o build-test/test_camview tests/test_camview.c \
            source/flightcam.c source/camera.c source/mat4.c source/flight.c \
            source/gauges.c -lm &&
        ./build-test/test_camview &&
        gcc -O1 -Wall -Wextra -o build-test/test_mesh tests/test_mesh.c \
            source/mesh.c &&
        ./build-test/test_mesh &&
        gcc -O1 -Wall -Wextra -o build-test/test_autopilot \
            tests/test_autopilot.c source/autopilot.c source/flight.c -lm &&
        ./build-test/test_autopilot'
    rc=$?
    [ $rc -ne 0 ] && exit $rc

    # Kaynak kontrolu: ucus durumu ana dongunun ICINDE sifirlanmamali.
    # Bir kez bu hata olustu (init cagrisi dongunun icine kaydi) ve ucak her
    # karede pistte sifirlandigi icin hicbir kumanda ise yaramadi.
    echo ">>> Kaynak kontrolu: ana dongude durum sifirlama var mi"
    if awk '/^    while \(!should_exit\)/{inloop=1} inloop && /flight_init/{print NR": "$0; found=1} END{exit !found}' \
           source/main.c; then
        echo "HATA: flight_init* ana dongunun icinde cagriliyor"
        exit 1
    fi
    echo "  temiz"

    # Kumandalarin main.c'de gercekten bagli oldugunu dogrula
    echo ">>> Kaynak kontrolu: ucus kumandalari bagli mi"
    for sym in in_pitch in_roll throttle flap spoiler gear_down cam_mode; do
        grep -q "plane\.$sym\|cam_mode =" source/main.c || {
            echo "HATA: $sym main.c'de kumandaya bagli degil"
            exit 1
        }
    done
    echo "  temiz"
    exit 0
fi

# --- PS3'e gonderme ---
#   ./build.sh gonder <PS3_IP>    : .pkg'yi FTP ile PS3'e kopyalar
#   ./build.sh calistir <PS3_IP>  : .self'i ps3load ile dogrudan calistirir
if [ "$1" = "send" ] || [ "$1" = "gonder" ]; then
    IP="$2"
    [ -n "$IP" ] || { echo "Usage: ./build.sh send <PS3_IP>"; exit 1; }
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
    [ -n "$IP" ] || { echo "Usage: ./build.sh log <PS3_IP>"; exit 1; }

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

if [ "$1" = "run" ] || [ "$1" = "calistir" ]; then
    IP="$2"
    [ -n "$IP" ] || { echo "Usage: ./build.sh run <PS3_IP>"; exit 1; }
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

# 0) Ucak modeli: glTF -> gomulebilir ikili. Kaynak model daha yeniyse
#    yeniden uretilir. Saf Python, ek bagimlilik yok.
# Kendi urettigimiz model oncelikli (tools/blender/build_aircraft.py).
MODEL_SRC=assets/model/boeing737.glb
[ -f "$MODEL_SRC" ] || MODEL_SRC=assets/model/jet.glb
[ -f "$MODEL_SRC" ] || MODEL_SRC=assets/model/plane_split.glb
[ -f "$MODEL_SRC" ] || MODEL_SRC=assets/model/plane.glb
if [ -f "$MODEL_SRC" ]; then
    if [ ! -f data/plane.bin ] || [ "$MODEL_SRC" -nt data/plane.bin ]; then
        echo ">>> Ucak modeli donusturuluyor"
        mkdir -p data
        if [ "$MODEL_SRC" = "assets/model/boeing737.glb" ] \
           || [ "$MODEL_SRC" = "assets/model/jet.glb" ]; then
            $PS3_RUN python3 tools/glb_to_mesh.py "$MODEL_SRC" data/plane.bin
        else
            $PS3_RUN python3 tools/glb_to_mesh.py "$MODEL_SRC" data/plane.bin \
                --scale 0.62 --flip
        fi
    fi
fi

# 0b) Dokular: JPG -> RSX'e hazir ARGB + mip zinciri
if [ -d assets/texture ]; then
    newest=$(ls -t assets/texture/*.jpg 2>/dev/null | head -1)
    if [ -n "$newest" ] && { [ ! -f data/textures.bin ] || [ "$newest" -nt data/textures.bin ]; }; then
        echo ">>> Dokular donusturuluyor"
        mkdir -p data
        docker run --rm -v "$PWD":/project -w /project "$ASSET_IMAGE" \
            python3 tools/make_textures.py assets/texture data/textures.bin
    fi
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
