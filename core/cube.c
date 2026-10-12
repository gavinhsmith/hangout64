#include "cube.h"

static const int8_t cube_verts[CUBE_FACES * 4][3] = {
    { 1, -1, -1}, { 1,  1, -1}, { 1,  1,  1}, { 1, -1,  1}, /* +X */
    {-1, -1,  1}, {-1,  1,  1}, {-1,  1, -1}, {-1, -1, -1}, /* -X */
    {-1,  1, -1}, {-1,  1,  1}, { 1,  1,  1}, { 1,  1, -1}, /* +Y */
    {-1, -1,  1}, {-1, -1, -1}, { 1, -1, -1}, { 1, -1,  1}, /* -Y */
    {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}, /* +Z */
    { 1, -1, -1}, {-1, -1, -1}, {-1,  1, -1}, { 1,  1, -1}, /* -Z */
};

static const uint8_t cube_colors[CUBE_FACES][3] = {
    {230,  60,  60}, { 60, 200,  80}, { 70, 110, 230},
    {240, 200,  50}, {200,  80, 220}, { 60, 210, 220},
};

const mesh cube_mesh = {cube_verts, cube_colors, CUBE_FACES};

#define FLOOR_QUADS (FLOOR_TILES * FLOOR_TILES)
static int8_t floor_verts[FLOOR_QUADS * 4][3];
static uint8_t floor_colors[FLOOR_QUADS][3];
const mesh floor_mesh = {floor_verts, floor_colors, FLOOR_QUADS};

#define MOVE_SPEED 6 /* hundredths of a unit per frame */
#define ORBIT_SPEED 20
#define ZOOM_SPEED 10
#define EASE 8 /* the camera closes 1/EASE of the gap each frame */

world world_init(void)
{
    for (int i = 0; i < FLOOR_TILES; i++) {
        for (int j = 0; j < FLOOR_TILES; j++) {
            int q = i * FLOOR_TILES + j, x0 = 2 * i - FLOOR_TILES, z0 = 2 * j - FLOOR_TILES;
            const int8_t corners[4][3] = {
                {x0, FLOOR_Y, z0}, {x0, FLOOR_Y, z0 + 2}, {x0 + 2, FLOOR_Y, z0 + 2}, {x0 + 2, FLOOR_Y, z0},
            };
            for (int c = 0; c < 4; c++)
                for (int k = 0; k < 3; k++)
                    floor_verts[q * 4 + c][k] = corners[c][k];
            uint8_t shade = (i + j) % 2 ? 60 : 90;
            floor_colors[q][0] = shade;
            floor_colors[q][1] = shade;
            floor_colors[q][2] = shade + 10;
        }
    }
    view v = {0, 0, 0, 200, 500};
    return (world){0, 0, 0, v, v};
}

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* Bhaskara I's approximation: within 0.2% of sin. */
int isin(int deg)
{
    deg %= 360;
    if (deg < 0)
        deg += 360;
    int sign = 1;
    if (deg >= 180) {
        deg -= 180;
        sign = -1;
    }
    int p = deg * (180 - deg);
    return sign * 4 * 4096 * p / (40500 - p);
}

int icos(int deg)
{
    return isin(deg + 90);
}

/* Move cur 1/EASE of the way to target, at least 1 so it always arrives. */
static int ease(int cur, int target)
{
    int d = target - cur, step = d / EASE;
    if (step == 0 && d != 0)
        step = d > 0 ? 1 : -1;
    return cur + step;
}

world world_update(world w, unsigned input)
{
    w.spin = (w.spin + 1) % 360;

    /* Strafe relative to the camera: forward is away from it, along the ground. */
    int right = !!(input & MOVE_RIGHT) - !!(input & MOVE_LEFT);
    int fwd = !!(input & MOVE_FWD) - !!(input & MOVE_BACK);
    int s = isin(w.cam.yaw / 10), c = icos(w.cam.yaw / 10);
    w.x = clamp(w.x + (right * c - fwd * s) * MOVE_SPEED / 4096, -CUBE_LIMIT, CUBE_LIMIT);
    w.z = clamp(w.z + (-right * s - fwd * c) * MOVE_SPEED / 4096, -CUBE_LIMIT, CUBE_LIMIT);

    w.target.x = w.x;
    w.target.z = w.z;
    if (input & CAM_LEFT)  w.target.yaw -= ORBIT_SPEED;
    if (input & CAM_RIGHT) w.target.yaw += ORBIT_SPEED;
    if (input & CAM_UP)    w.target.pitch += ORBIT_SPEED;
    if (input & CAM_DOWN)  w.target.pitch -= ORBIT_SPEED;
    if (input & CAM_IN)    w.target.dist -= ZOOM_SPEED;
    if (input & CAM_OUT)   w.target.dist += ZOOM_SPEED;
    w.target.pitch = clamp(w.target.pitch, -CAM_PITCH_MAX, CAM_PITCH_MAX);
    w.target.dist = clamp(w.target.dist, CAM_DIST_MIN, CAM_DIST_MAX);

    w.cam.x = ease(w.cam.x, w.target.x);
    w.cam.z = ease(w.cam.z, w.target.z);
    w.cam.yaw = ease(w.cam.yaw, w.target.yaw);
    w.cam.pitch = ease(w.cam.pitch, w.target.pitch);
    w.cam.dist = ease(w.cam.dist, w.target.dist);
    return w;
}
