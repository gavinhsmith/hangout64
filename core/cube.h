#ifndef CUBE_H
#define CUBE_H

#include <stdint.h>

/* Every platform renders at this size (DS excepted) and scales up. */
#define SCREEN_W 320
#define SCREEN_H 240

/* Quads with one colour each. Coordinates are whole units. */
typedef struct {
    const int8_t (*verts)[3]; /* quads * 4 corners */
    const uint8_t (*colors)[3]; /* one per quad */
    int quads;
} mesh;

#define CUBE_FACES 6
#define FLOOR_TILES 10 /* per side, 2 units each, centred on the origin */
#define FLOOR_Y -2     /* below the spinning cube's reach (sqrt(3)) */

extern const mesh cube_mesh;
extern const mesh floor_mesh; /* valid after world_init() */

/* Held input for one frame. Movement is relative to the camera's view. */
enum {
    MOVE_LEFT = 1 << 0, MOVE_RIGHT = 1 << 1, MOVE_FWD = 1 << 2, MOVE_BACK = 1 << 3,
    CAM_LEFT = 1 << 4, CAM_RIGHT = 1 << 5, CAM_UP = 1 << 6, CAM_DOWN = 1 << 7,
    CAM_IN = 1 << 8, CAM_OUT = 1 << 9,
};

/* Lengths are in hundredths of a unit, angles in tenths of a degree. */
typedef struct {
    int x, z;  /* point the camera looks at (on the cube's plane) */
    int yaw;   /* increasing moves the camera right; not wrapped */
    int pitch; /* positive looks down from above */
    int dist;
} view;

typedef struct {
    int spin;    /* cube spin, degrees 0..359 */
    int x, z;    /* cube position */
    view target; /* where the camera is heading (input + cube position) */
    view cam;    /* where it is: eases toward target every frame */
} world;

#define CAM_PITCH_MAX 800
#define CAM_DIST_MIN 300
#define CAM_DIST_MAX 1200
#define CUBE_LIMIT ((FLOOR_TILES - 1) * 100) /* keep the cube on the floor */

/* Build the floor mesh and return the starting state. */
world world_init(void);

/* Advance one frame. Platforms render with:
 *   view:  translate(0, 0, -cam.dist) rotateX(cam.pitch) rotateY(-cam.yaw) translate(-cam.x, 0, -cam.z)
 *   cube:  translate(x, 0, z) rotateX(spin) rotateY(spin)                                            */
world world_update(world w, unsigned input);

/* sin/cos of whole degrees, scaled by 4096 (integer-only for the DS). */
int isin(int deg);
int icos(int deg);

#endif
