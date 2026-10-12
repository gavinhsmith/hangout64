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
#define FLOOR_Y -1     /* where the floor is drawn; game heights are measured from it */

extern const mesh guy_mesh;   /* feet at y = 0, facing -Z; see tools/obj2c.py */
extern const mesh level_mesh; /* floor + parts, valid after world_init() */

/* Lengths are in thousandths of a unit, angles in tenths of a degree. Heights (y) are
 * measured from the floor. Everything here is plain integers so it can be saved or sent
 * over a network as-is and runs identically on every platform. */

/* An axis-aligned box. */
typedef struct {
    int x, y, z;    /* centre */
    int sx, sy, sz; /* size */
    uint8_t color[3];
} part;

extern const part level_parts[];
extern const int level_part_count;

/* Held buttons for one frame. Movement is relative to the camera's view. */
enum {
    MOVE_LEFT = 1 << 0, MOVE_RIGHT = 1 << 1, MOVE_FWD = 1 << 2, MOVE_BACK = 1 << 3,
    CAM_LEFT = 1 << 4, CAM_RIGHT = 1 << 5, CAM_UP = 1 << 6, CAM_DOWN = 1 << 7,
    CAM_IN = 1 << 8, CAM_OUT = 1 << 9, JUMP = 1 << 10,
};

/* One player's input for a frame: buttons plus the yaw of the camera they're looking
 * through, so "forward" means into their screen. */
typedef struct {
    unsigned buttons;
    int yaw;
} input;

#define PLAYER_RADIUS 350  /* collision box half-width */
#define PLAYER_HEIGHT 2400

typedef struct {
    int x, y, z;   /* feet */
    int vy;        /* vertical speed per frame */
    int on_ground;
    int heading;   /* 0..3599; 0 faces -Z, turns toward where it moves */
    int target_heading;
} player;

#define MAX_PLAYERS 4
typedef struct {
    player players[MAX_PLAYERS];
    int player_count;
} world;

#define PLAYER_LIMIT ((FLOOR_TILES - 1) * 1000) /* keep players on the floor */

/* Build the level mesh and return the starting state with `players` players. */
world world_init(int players);

/* Advance one frame; inputs[i] drives players[i]. */
world world_update(world w, const input inputs[MAX_PLAYERS]);

/* A camera is per viewer, not game state. It follows one player and eases toward
 * the orbit/zoom target set by that viewer's buttons. Platforms render with:
 *   view:   translate(0, 0, -dist) rotateX(pitch) rotateY(-yaw) translate(-x, -y, -z)
 *   level:  as is
 *   player: translate(x, FLOOR_Y + y, z) rotateY(heading)                             */
typedef struct {
    int x, y, z; /* point it looks at: the ground they stand on, plus half of a jump */
    int yaw;     /* increasing moves the camera right; not wrapped */
    int pitch;   /* positive looks down from above */
    int dist;
} view;

typedef struct {
    view now, target;
    int ground; /* height of the last thing the followed player stood on */
} camera;

#define CAM_PITCH_MAX 800
#define CAM_DIST_MIN 3000
#define CAM_DIST_MAX 12000

camera camera_init(void);
camera camera_update(camera c, unsigned buttons, const player *follow);

/* sin/cos of whole degrees, scaled by 4096 (integer-only for the DS). */
int isin(int deg);
int icos(int deg);

#endif
