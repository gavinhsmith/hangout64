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

## Places

A place is the content of one game: parts (boxes, optionally drawn with a custom model), the
models they use, and the models' textures. Places are data, loaded and validated at runtime by `core/place.c`,
so they can be delivered over the network later. The built-in place is
`assets/places/course.json`, packed into `core/default_place.c`:

```sh
python3 tools/placec.py assets/places/course.json core/default_place.c default_place
python3 tools/placec.py assets/places/course.json course.place   # the same, as a binary file
```

The format is described at the top of `tools/placec.py`. Budgets (parts, meshes, textures,
triangles) are in `core/world.h` and are sized for the weakest console. Textures are PNGs with
at most 16 colours, up to 32 px per side (32x32 fits the N64's texture memory), padded to a
power of two if needed; they are drawn nearest-filtered and tinted by the model's colour. `placec.py` needs Pillow
(`pip install pillow`).

Engine assets (like the avatar) are compiled in instead, with `tools/obj2c.py`. The avatar's
texture is the atlas `assets/texture/guy.png`; `tools/guy_uvs.py` maps it onto the model
(re-run it after re-exporting `guy.obj`, which drops the UVs):

```sh
python3 tools/guy_uvs.py assets/model/guy.obj
python3 tools/obj2c.py assets/model/guy.obj guy_mesh 255,255,255 assets/texture/guy.png guy_texture > core/guy_mesh.c
```

Models are OBJ files exported Z-up with the front facing -Y (as from OwlCAD).

## CI

Every push and PR runs tests and builds all platforms (artifacts on the run). Pushing a `v*` tag publishes a GitHub release with a zip per platform.
