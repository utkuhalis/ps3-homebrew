# PS3 Ping Pong

A pong game for the PlayStation 3, written with PSL1GHT.
The whole build chain runs inside Docker — no SDK or compiler is installed on
the host machine.

## Download

Pre-built packages are attached to the [latest release](../../releases/latest).

| File | Use |
|---|---|
| `pingpong.pkg` | Real PS3 (CFW/HEN) — install via Package Manager, launch from XMB |
| `pingpong.fake.self` | RPCS3 — File → Boot SELF/ELF |

## Building

```sh
./build.sh          # produces .self + .pkg
./build.sh clean    # removes build outputs
./build.sh test     # game-logic unit tests
```

The first run downloads a prebuilt toolchain image (`zeldin/ps3dev-docker`,
~400 MB — ppu-gcc 7.2.0 + PSL1GHT). On Apple Silicon the arm64 variant is used,
so it runs natively without Rosetta. Later builds take seconds.

## Controls

**Menu** — D-pad: select, **X**: confirm, **O**: back

**Game** — Player 1 on pad 1, Player 2 on pad 2 (up/down).
The left analog stick gives proportional paddle speed; the D-pad moves at full
speed. **START** pauses, and **O** while paused returns to the menu.

## Gameplay

First to 11 points wins. In single-player mode the right paddle is driven by a
medium-difficulty AI: it tracks the ball, but its speed is capped and it reacts
with a small delay, so it is competitive yet beatable.

## Screen preview without a PS3

The drawing layer can be run on the host and dumped to PNG:

```sh
docker run --rm -v "$PWD":/project -w /project alpine:3.20 sh -c '
  apk add --no-cache build-base imagemagick >/dev/null &&
  gcc -O1 -o build-test/preview tests/preview.c source/font.c source/menu.c \
      source/draw.c source/game.c -lm && ./build-test/preview &&
  cd build-test && for f in *.ppm; do magick "$f" "${f%.ppm}.png"; done'
```

Images land in `build-test/`.

## Code layout

| File | Responsibility |
|---|---|
| `source/game.c` | Physics, scoring, bot AI — hardware-independent, unit tested |
| `source/video.c` | libgcm double-buffered framebuffer, rectangle fill |
| `source/font.c` | 8x8 bitmap font, UTF-8 |
| `source/input.c` | Pad reading, edge detection |
| `source/menu.c` | Menu state and drawing |
| `source/draw.c` | Play field and result screen |
| `source/main.c` | State machine and main loop |

Font tables live in `source/font_data.h` (dhepper/font8x8, public domain).

## A note on pad input

The PS3 pad API does not deliver data every frame. When `padData.len == 0`
there is **no new data** and the struct contents are invalid — the axes read as
0, which looks exactly like "stick pushed to the corner". Reading it blindly
made the paddle stick to the top of the screen. The fix is in
`source/input.c`: skip the frame and keep the previous state.
