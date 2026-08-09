# PS3 Homebrew Games

Homebrew games for the PlayStation 3, written with PSL1GHT.
The entire toolchain lives inside Docker — no SDK, compiler or library is
installed on the host machine. Runs natively (arm64) on Apple Silicon.

## Download

Pre-built `.pkg` files are attached to the
[latest release](../../releases/latest). Install them on a CFW/HEN console via
Package Manager, or drop the included `.self` files straight into RPCS3.

Both packages are verified: the exact `EBOOT.BIN` shipped inside each `.pkg`
is booted in RPCS3 as part of the release process.

## Projects

### [Ping Pong](Ping-Pong/) — playable

A complete pong game with menus.

- Menu (Start / About / Quit), single player vs. AI and two-player modes
- Matches to 11 points, pause, winner screen
- Medium-difficulty bot (tracks the ball, but beatable)
- Proportional paddle speed on the analog stick, full speed on the D-pad
- 8x8 bitmap font built into the code
- Background image support (converted to raw pixels and embedded at build time)
- 29 unit tests

### [Basic Water](Basic-Water/) — playable

A flight simulator over an open sea: real aircraft model, flight physics,
runways, weather and a full instrument panel.

![Basic Water](docs/basic-water.png)

- **Flight model:** thrust, lift, drag, stall in both directions, angular
  inertia, G-loading, trim stability. Control authority scales with dynamic
  pressure — a parked aircraft does not respond to the stick.
- **Aircraft:** a real 60k-triangle glTF model with separated control
  surfaces; flaps, ailerons and rudder deflect with your input, and the
  landing gear retracts.
- **Sky:** horizon-to-zenith gradient, sun disc and halo, wind-drifting
  clouds — procedural, no textures.
- **Sea:** two large waves in geometry, two fine waves as per-pixel normals;
  Fresnel-weighted sky reflection, sun glitter, distance fog.
- **Weather and time of day:** clear, cloudy, rainy, foggy, stormy; day,
  sunset, night. Selectable from the in-game menu.
- **Instruments:** airspeed, altitude, artificial horizon, throttle lever,
  minimap, objectives, STALL / OVER G / LOW FUEL warnings with audio.
- **Autopilot:** holds altitude, heading and speed; releases when you touch
  the stick.
- **Audio:** engine, wind, sea, stall horn, gear/flap servo, wheel roll and
  touchdown — all synthesised, no sound files.
- 7 test suites covering flight physics, camera, autopilot, atmosphere,
  menus, mesh loading and math.

## Building

Both projects build with a single command. Docker is the only requirement.

```sh
cd Basic-Water        # or Ping-Pong
./build.sh            # produces .self and .pkg
./build.sh test       # runs the host-side test suites
```

Outputs:

| File | Use |
|---|---|
| `*.fake.self` | Drag into RPCS3 |
| `*.pkg` | Install on a real PS3 (CFW/HEN) |

Extra commands for Basic Water:

```sh
./build.sh gonder <PS3_IP>     # upload the .pkg over FTP
./build.sh calistir <PS3_IP>   # run the .self directly via ps3load
./build.sh log <PS3_IP>        # fetch the on-console diagnostic log
```

## How it is tested

PS3 hardware code (RSX, pad, audio) cannot be unit tested, so everything that
can be kept pure is kept pure and tested on the host inside Docker: flight
physics, camera math, autopilot, atmosphere, menu state, mesh parsing.

Rendering is verified before it ever reaches the console — `tools/ui_preview.c`
rasterises the HUD exactly as the GPU would (pixel-centre coverage, no MSAA)
and writes a PNG, and the shader formulas have C counterparts that are
rendered on the host and compared.

## Toolchain

- Base image: `zeldin/ps3dev-docker` (ps3toolchain + PSL1GHT), arm64 native
- Cg shaders are compiled in a separate amd64 image, because the NVIDIA Cg
  Toolkit is x86-only. The resulting `.vpo`/`.fpo` RSX microcode is
  architecture-independent, so it is built once and linked on arm64.

## License

MIT
