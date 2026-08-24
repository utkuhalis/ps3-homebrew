# Basic Water

A flight simulator on the PS3: a real aircraft over an open sea, with flight
physics, runways, weather, instruments and autopilot.

The whole toolchain runs inside Docker — no SDK or compiler is installed on the
host machine.

## Download

Pre-built packages are attached to the [latest release](../../releases/latest).

| File | Use |
|---|---|
| `basicwater.pkg` | Real PS3 (CFW/HEN) — install via Package Manager |
| `basicwater.fake.self` | RPCS3 — File → Boot SELF/ELF |

## Building

```sh
./build.sh          # produces .self + .pkg
./build.sh test     # host-side unit tests
./build.sh clean    # removes build outputs
```

Deployment helpers:

```sh
./build.sh gonder <PS3_IP>     # upload the .pkg over FTP
./build.sh calistir <PS3_IP>   # run the .self directly via ps3load
./build.sh log <PS3_IP>        # fetch /dev_hdd0/tmp/basicwater.log
```

### Why two Docker images

| Stage | Image | Reason |
|---|---|---|
| Cg shader compilation | `ps3-cgcomp` (amd64) | `cgcomp` needs the NVIDIA Cg Toolkit, which exists for x86 only |
| PS3 build | `zeldin/ps3dev-docker` (arm64) | Runs natively on Apple Silicon |

Shader output (`.vpo`/`.fpo`) is RSX microcode and architecture-independent, so
it is produced on amd64 and linked into the arm64 build. `build.sh` chains both
steps; you run one command.

## Controls

| Action | Pad | Keyboard (RPCS3 default) |
|---|---|---|
| **Fly** (pitch / roll) | **Right stick** (or D-pad) | arrow keys |
| **Orbit camera** | **Left stick** | W A S D |
| Throttle up | R2 or R1 | T or E |
| Throttle down | L2 or L1 | R or Q |
| Flap notch | Square | Z |
| Spoiler + brakes | Triangle (hold) | V |
| Landing gear | Cross | X |
| Camera mode | Circle | C |
| Autopilot | R3 | G |
| Settings menu | Select | Space |
| Quit | Start | Enter |

Camera modes cycle through Chase, Cockpit, Left Wing, Right Wing, Tail and
Free.

### Taking off

The aircraft starts stopped at the head of the runway, engine at idle. Hold
the throttle key until the on-screen strip shows you have reached rotation
speed, then pull back. The strip disappears once you are airborne.

## Flight model

Not a full aerodynamic simulation, but built on the same quantities a real one
uses, and tuned to the numbers of a Boeing 737-800:

- 41 t empty, 18 t fuel, 235 kN thrust, 125 m² wing, rotation at 75 m/s
- Lift and drag from dynamic pressure and angle of attack, with stall in
  **both** directions and lift decaying to zero past 60°
- Angle of attack is computed in body axes, so sideslip does not contaminate
  it (measuring the total angle between velocity and nose produced false stall
  warnings in every turn)
- Angular inertia: the stick commands a target rate, and the actual rate
  approaches it with a time constant — the aircraft does not snap to input and
  does not stop turning the instant you let go
- Control authority scales with dynamic pressure; a parked aircraft ignores
  the stick
- Trim stability: the nose settles at the angle of attack that carries the
  aircraft's weight, so it flies straight hands-off
- **Body rates are converted to Euler rates properly**: in a bank, part of a
  pitch input becomes yaw, which is what actually turns an aircraft. Adding
  body rates straight to the Euler angles only works in level flight, and at
  airliner mass it drove the nose down into a stall on every turn
- Coordinated turn rate is g·tan(bank)/V, so it depends on speed
- Trim accounts for bank: lift's vertical component falls by cos(bank), so the
  aircraft trims to a higher angle of attack to hold altitude
- G-loading with structural limits (+2.5 / −1.0)
- Ground contact with rolling and braking friction, rotation-speed gate, and
  much heavier drag on water

## Aircraft model

Models are converted to a flat binary at build time by `tools/glb_to_mesh.py` —
pure Python, no dependencies. Every moving surface is its own part, and the
engine recognises them by name prefix, because real models split one surface
across several panels (`flap_left01..04`, `spoiler_right01..06`) that all move
together.

The in-repo default is `assets/model/jet.glb`, generated parametrically by
`tools/blender/build_aircraft.py` in headless Blender: 25 parts, 4280
triangles, ends capped so no surface faces inward, UVs unwrapped per part.
It exists so the project builds for anyone, with no third-party asset.

`build.sh` prefers `assets/model/boeing737.glb` when present. That one is
imported from a commercial asset pack by `tools/blender/import_737.py` and is
**gitignored** — usable in a local build and embedded in the compiled package,
but not redistributed as source.

## Testing without a PS3

Hardware code (RSX, pad, audio) cannot be unit tested, so everything that can
be pure is pure and tested on the host:

| Suite | Covers |
|---|---|
| `test_math` | matrices, projection, camera collision |
| `test_flight` | lift, drag, stall, takeoff, gear, fuel |
| `test_camview` | camera aiming, orientation matrix, artificial horizon |
| `test_autopilot` | altitude/heading hold, bank limits, stall protection |
| `test_atmosphere` | weather and time-of-day combinations |
| `test_menu` | menu state machine |
| `test_mesh` | model file parsing against the real data file |

The mesh test has caught the same class of bug twice: the part limit was too
low (16, then 40) and `mesh_load` silently rejected the whole model. Both times
it failed on the host instead of shipping a game that would not start.

`build.sh test` also greps the source for two regressions that once shipped:
flight state being reset inside the main loop, and controls not being bound.

Rendering is checked before it reaches hardware. `tools/ui_preview.c`
rasterises the HUD the way the GPU does (pixel-centre coverage, MSAA off) and
writes a PNG, so half-pixel geometry that would silently vanish on the console
is visible on the host.

## Diagnostics

The game writes a boot log to `/dev_hdd0/tmp/basicwater.log` on the console.
`./build.sh log <PS3_IP>` fetches it. When the model loader once rejected its
own data file, that log named the failing call directly.
