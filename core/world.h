#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

/* Every platform renders at this size (DS excepted) and scales up. */
#define SCREEN_W 320
#define SCREEN_H 240

/* Triangles with one colour each. Coordinates are 8.8 fixed point: MESH_ONE = 1 unit. */
#define MESH_ONE 256
typedef struct {
    const int16_t (*verts)[3];  /* tris * 3 corners */
    const uint8_t (*colors)[3]; /* one per triangle */
    int tris;
} mesh;

#define FLOOR_TILES 10 /* per side, 2 units each, centred on the origin */
#define FLOOR_Y -1     /* the guy stands on it */

extern const mesh guy_mesh;   /* feet at y = 0, facing -Z; see tools/obj2c.py */
extern const mesh floor_mesh; /* valid after world_init() */

/* Held input for one frame. Movement is relative to the camera's view. */
enum {
    MOVE_LEFT = 1 << 0, MOVE_RIGHT = 1 << 1, MOVE_FWD = 1 << 2, MOVE_BACK = 1 << 3,
    CAM_LEFT = 1 << 4, CAM_RIGHT = 1 << 5, CAM_UP = 1 << 6, CAM_DOWN = 1 << 7,
    CAM_IN = 1 << 8, CAM_OUT = 1 << 9, JUMP = 1 << 10,
};

/* Lengths are in thousandths of a unit, angles in tenths of a degree. */
typedef struct {
    int x, y, z; /* point the camera looks at; y follows half of a jump */
    int yaw;   /* increasing moves the camera right; not wrapped */
    int pitch; /* positive looks down from above */
    int dist;
} view;

typedef struct {
    int x, z;           /* guy position */
    int y, vy;          /* height above the floor and vertical speed, per frame */
    int heading;        /* guy facing, 0..3599; 0 faces -Z, turns toward where it moves */
    int target_heading;
    view target;        /* where the camera is heading (input + guy position) */
    view cam;           /* where it is: eases toward target every frame */
} world;

#define CAM_PITCH_MAX 800
#define CAM_DIST_MIN 3000
#define CAM_DIST_MAX 12000
#define GUY_LIMIT ((FLOOR_TILES - 1) * 1000) /* keep the guy on the floor */

/* Build the floor mesh and return the starting state. */
world world_init(void);

/* Advance one frame. Platforms render with:
 *   view:  translate(0, 0, -cam.dist) rotateX(cam.pitch) rotateY(-cam.yaw) translate(-cam.x, -cam.y, -cam.z)
 *   guy:   translate(x, FLOOR_Y + y, z) rotateY(heading)                                               */
world world_update(world w, unsigned input);

/* sin/cos of whole degrees, scaled by 4096 (integer-only for the DS). */
int isin(int deg);
int icos(int deg);

#endif
