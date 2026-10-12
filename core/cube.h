#ifndef CUBE_H
#define CUBE_H

#include <stdint.h>

/* Every platform renders at this size (DS excepted) and scales up. */
#define SCREEN_W 320
#define SCREEN_H 240

#define CUBE_FACES 6

/* Unit cube centred on the origin: 6 faces x 4 corners, one colour per face. */
extern const int8_t cube_verts[CUBE_FACES * 4][3];
extern const uint8_t cube_colors[CUBE_FACES][3];

/* Advance the spin by one frame. Angle is in whole degrees, 0..359. */
int cube_step(int angle);

/* Orbit camera looking at the origin. Platforms map their buttons to CAM_* bits
 * and render with: translate(0, 0, -dist / 10), rotateX(pitch), rotateY(-yaw). */
enum { CAM_LEFT = 1, CAM_RIGHT = 2, CAM_UP = 4, CAM_DOWN = 8, CAM_IN = 16, CAM_OUT = 32 };

typedef struct {
    int yaw;   /* degrees, 0..359; increasing moves the camera right */
    int pitch; /* degrees, clamped to +/-CAM_PITCH_MAX; positive looks down from above */
    int dist;  /* tenths of a unit, CAM_DIST_MIN..CAM_DIST_MAX */
} camera;

#define CAM_PITCH_MAX 80
#define CAM_DIST_MIN 30
#define CAM_DIST_MAX 120
#define CAMERA_INIT {0, 20, 50}

/* Advance the camera by one frame of held input. */
camera camera_update(camera c, unsigned input);

#endif
