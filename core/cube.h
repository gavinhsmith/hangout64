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

#endif
