#!/bin/sh
# Basic Water - build (everything runs in Docker, nothing is installed on the host)
#
#   ./build.sh          -> builds the .self and .pkg
#   ./build.sh clean    -> removes build outputs
#   ./build.sh test     -> runs the host-side unit tests
#
#   ./build.sh send <PS3_IP>      -> upload the .pkg to the console over FTP
#   ./build.sh run <PS3_IP>       -> run the .self directly via ps3load
#   ./build.sh log <PS3_IP>       -> fetch the console's diagnostic log
#   (the old names gonder / calistir still work)
#
# Two-stage build:
#   1) Cg shaders compile in an amd64 image (the Cg Toolkit only exists for x86)
#   2) The PS3 build itself runs in the native arm64 ps3dev image
set -e

PS3_IMAGE=zeldin/ps3dev-docker:latest
CG_IMAGE=ps3-cgcomp:latest
ASSET_IMAGE=ps3-assets:latest
cd "$(dirname "$0")"

if ! docker image inspect "$CG_IMAGE" >/dev/null 2>&1; then
    echo ">>> Building the shader compiler image (one time only)."
    docker build --platform linux/amd64 -f Dockerfile.shaders -t "$CG_IMAGE" .
fi

if [ -d assets/texture ] && ! docker image inspect "$ASSET_IMAGE" >/dev/null 2>&1; then
    echo ">>> Building the asset conversion image (one time only)."
    docker build --quiet -f Dockerfile.assets -t "$ASSET_IMAGE" . >/dev/null
fi

if ! docker image inspect "$PS3_IMAGE" >/dev/null 2>&1; then
    echo ">>> Pulling the PS3 toolchain image (~400 MB, one time only)."
    docker pull --platform linux/arm64 "$PS3_IMAGE"
fi

PS3_RUN="docker run --rm --platform linux/arm64 -v $PWD:/project -w /project $PS3_IMAGE"
CG_RUN="docker run --rm --platform linux/amd64 -v $PWD:/project -w /project $CG_IMAGE"

if [ "$1" = "test" ]; then
    echo ">>> Unit tests (host side, no PS3 needed)"
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

    # Source check: flight state must not be reset INSIDE the main loop.
    # This happened once (the init call drifted into the loop): the aircraft
    # was put back on the runway every frame and no control had any effect.
    echo ">>> Source check: is flight state reset inside the main loop"
    if awk '/^    while \(!should_exit\)/{inloop=1} inloop && /flight_init/{print NR": "$0; found=1} END{exit !found}' \
           source/main.c; then
        echo "ERROR: flight_init* is called inside the main loop"
        exit 1
    fi
    echo "  ok"

    # Verify that the flight controls are actually wired up in main.c
    echo ">>> Source check: are the flight controls wired up"
    for sym in in_pitch in_roll throttle flap spoiler gear_down cam_mode; do
        grep -q "plane\.$sym\|cam_mode =" source/main.c || {
            echo "ERROR: $sym is not wired to a control in main.c"
            exit 1
        }
    done
    echo "  ok"
    exit 0
fi

# --- Sending to the PS3 ---
#   ./build.sh send <PS3_IP>  : copies the .pkg to the PS3 over FTP
#   ./build.sh run <PS3_IP>   : runs the .self directly through ps3load
if [ "$1" = "send" ] || [ "$1" = "gonder" ]; then
    IP="$2"
    [ -n "$IP" ] || { echo "Usage: ./build.sh send <PS3_IP>"; exit 1; }
    [ -f basicwater.pkg ] || { echo "Build first with ./build.sh"; exit 1; }

    echo ">>> Sending to $IP (FTP)"
    if curl -T basicwater.pkg "ftp://$IP/dev_hdd0/packages/" --connect-timeout 10; then
        echo
        echo "Sent. On the PS3:"
        echo "  Package Manager > Install Package Files > basicwater.pkg"
    else
        echo
        echo "Could not send. Check that:"
        echo "  - the PS3 and this computer are on the same network"
        echo "  - FTP is running on the PS3 (webMAN / multiMAN)"
        echo "  - the IP address is right (Settings > Network Settings > Connection Status)"
        exit 1
    fi
    exit 0
fi

if [ "$1" = "log" ]; then
    IP="$2"
    [ -n "$IP" ] || { echo "Usage: ./build.sh log <PS3_IP>"; exit 1; }

    echo ">>> Fetching the diagnostic log from the PS3"
    if curl -s --connect-timeout 10 "ftp://$IP/dev_hdd0/tmp/basicwater.log" \
            -o /tmp/basicwater-ps3.log; then
        echo "--- PS3 log ---"
        cat /tmp/basicwater-ps3.log
        echo "--- end ---"
    else
        echo "Could not fetch the log. Has the game been run at least once?"
        echo "Check that FTP is running and the IP address is right."
        exit 1
    fi
    exit 0
fi

if [ "$1" = "run" ] || [ "$1" = "calistir" ]; then
    IP="$2"
    [ -n "$IP" ] || { echo "Usage: ./build.sh run <PS3_IP>"; exit 1; }
    [ -f basicwater.self ] || { echo "Build first with ./build.sh"; exit 1; }

    echo ">>> Running directly on $IP (ps3load)"
    echo "    ps3load / a SELF loader must be running on the PS3."
    $PS3_RUN sh -c "export PS3LOAD=tcp:$IP; /usr/local/ps3dev/bin/ps3load basicwater.self"
    exit $?
fi

if [ "$1" = "clean" ]; then
    $PS3_RUN sh -c 'export PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev; make clean'
    rm -rf build-test
    exit 0
fi

# 0) Aircraft model: glTF -> embeddable binary, regenerated when the source
#    model is newer. Pure Python, no extra dependencies.
#    The 737 comes first, then our own generated jet (tools/blender/build_aircraft.py).
MODEL_SRC=assets/model/boeing737.glb
[ -f "$MODEL_SRC" ] || MODEL_SRC=assets/model/jet.glb
[ -f "$MODEL_SRC" ] || MODEL_SRC=assets/model/plane_split.glb
[ -f "$MODEL_SRC" ] || MODEL_SRC=assets/model/plane.glb
if [ -f "$MODEL_SRC" ]; then
    if [ ! -f data/plane.bin ] || [ "$MODEL_SRC" -nt data/plane.bin ]; then
        echo ">>> Converting the aircraft model"
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

# 0b) Textures: JPG -> RSX-ready ARGB with a mip chain
if [ -d assets/texture ]; then
    newest=$(ls -t assets/texture/*.jpg 2>/dev/null | head -1)
    if [ -n "$newest" ] && { [ ! -f data/textures.bin ] || [ "$newest" -nt data/textures.bin ]; }; then
        echo ">>> Converting textures"
        mkdir -p data
        docker run --rm -v "$PWD":/project -w /project "$ASSET_IMAGE" \
            python3 tools/make_textures.py assets/texture data/textures.bin
    fi
fi

# 1) Cg shaders -> RSX microcode (.vpo/.fpo). The output is architecture-independent.
echo ">>> Compiling shaders (Cg)"
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

# 2) The build itself. The shaders are already up to date, so make does not
#    call cgcomp (it must not: there is no Cg library on arm64).
echo ">>> PS3 build"
$PS3_RUN sh -c '
    export PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev
    export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin
    make'

echo
echo ">>> Outputs:"
ls -lh basicwater.self basicwater.fake.self basicwater.pkg 2>/dev/null || true
echo
echo "  RPCS3        : basicwater.fake.self  -> drag onto the emulator"
echo "  Real PS3     : basicwater.pkg        -> install from USB or webMAN"
