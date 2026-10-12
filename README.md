# hangout64
Multi-platform demo for a simple 3D homebrew title for Nintendo 64 and beyond.

Currently a little guy walking around a checkered floor with a few parts to climb, rendered at 320x240 everywhere (DS: native 256x192) and scaled up.

| Folder | Platform | Library | Output |
|---|---|---|---|
| `core/` | shared game code (no platform headers) | — | — |
| `n64/` | Nintendo 64 | libdragon (preview, OpenGL) | `.z64` |
| `ogc/` | GameCube + Wii | libogc (GX) | `.dol` ×2 |
| `nds/` | DS | libnds | `.nds` |
| `3ds/` | 3DS | libctru + citro3d | `.3dsx` |
| `pc/` | Windows / Linux / macOS | SDL3 + OpenGL | executable |

## Controls

The guy walks around a checkered floor and turns to face where he's going; the camera
eases after him. Movement is relative to the camera (forward = into the screen). Orbit and
zoom ease too.

| Platform | Move | Jump | Orbit camera | Zoom in / out | Exit |
|---|---|---|---|---|---|
| PC | WASD | Space | Arrows | Q / E | Esc |
| N64 | Stick | A | D-pad | C-up / C-down | — |
| GameCube | Stick | A | C-stick / D-pad | L / R | START |
| Wii Remote | D-pad | A | B + D-pad | − / + | HOME |
| DS | D-pad | A | R + D-pad | X / Y | START |
| 3DS | Circle pad | A | D-pad | L / R | START |

On the Wii a GameCube pad works too, with the GameCube layout.

## Building

```sh
make test   # core unit tests (host compiler)
make pc     # needs CMake + SDL3
make n64    # consoles build in Docker, same images as CI
make ogc
make nds
make 3ds
make        # everything
make n64-reset  # drop the cached N64 toolchain + libdragon volume (e.g. after an image update)
```

## Running

- N64: ares, or a flashcart (SummerCart64, EverDrive)
- GameCube/Wii: Dolphin; on hardware via Swiss (GC) or the Homebrew Channel / `wiiload` (Wii)
- DS: melonDS 1.0+, or a flashcart. DeSmuME 0.9.13 shows white screens for anything built with libnds 2 / calico.
- 3DS: Azahar, or the Homebrew Launcher / `3dslink`

## Models

`assets/model/*.obj` are converted to C with `tools/obj2c.py` (Z-up, front facing -Y, as
exported from OwlCAD). The output is committed, so builds don't need Python:

```sh
python3 tools/obj2c.py assets/model/guy.obj guy_mesh 160,235,245 > core/guy_mesh.c
```

## CI

Every push and PR runs tests and builds all platforms (artifacts on the run). Pushing a `v*` tag publishes a GitHub release with a zip per platform.
