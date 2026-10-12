# hangout64
Multi-platform demo for a simple 3D homebrew title for Nintendo 64 and beyond.

Currently a spinning cube, rendered at 320x240 everywhere (DS: native 256x192) and scaled up.

| Folder | Platform | Library | Output |
|---|---|---|---|
| `core/` | shared game code (no platform headers) | — | — |
| `n64/` | Nintendo 64 | libdragon (preview, OpenGL) | `.z64` |
| `ogc/` | GameCube + Wii | libogc (GX) | `.dol` ×2 |
| `nds/` | DS | libnds | `.nds` |
| `3ds/` | 3DS | libctru + citro3d | `.3dsx` |
| `pc/` | Windows / Linux / macOS | SDL3 + OpenGL | executable |

## Controls

The cube spins; the camera orbits it.

| Platform | Orbit | Zoom in / out | Exit |
|---|---|---|---|
| PC | Arrows / WASD | Q / E | Esc |
| N64 | Stick / D-pad | C-up / C-down | — |
| GameCube | Stick / D-pad | L / R | START |
| Wii | Wii Remote D-pad (or GC pad) | − / + | HOME |
| DS | D-pad | L / R | START |
| 3DS | Circle pad / D-pad | L / R | START |

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

## CI

Every push and PR runs tests and builds all platforms (artifacts on the run). Pushing a `v*` tag publishes a GitHub release with a zip per platform.
