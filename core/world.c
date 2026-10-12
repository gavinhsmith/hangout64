#include "world.h"

#define FLOOR_TRIS (FLOOR_TILES * FLOOR_TILES * 2)
static int16_t floor_verts[FLOOR_TRIS * 3][3];
static uint8_t floor_colors[FLOOR_TRIS][3];
const mesh floor_mesh = {floor_verts, floor_colors, FLOOR_TRIS};

#define MOVE_SPEED 6 /* hundredths of a unit per frame */
#define ORBIT_SPEED 20
#define ZOOM_SPEED 10
#define EASE 8      /* the camera closes 1/EASE of the gap each frame */
#define TURN_EASE 4 /* the guy turns faster than the camera follows */

world world_init(void)
{
    static const int tri_corners[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < FLOOR_TILES; i++) {
        for (int j = 0; j < FLOOR_TILES; j++) {
            int tile = i * FLOOR_TILES + j, x0 = 2 * i - FLOOR_TILES, z0 = 2 * j - FLOOR_TILES;
            const int corners[4][2] = {{x0, z0}, {x0, z0 + 2}, {x0 + 2, z0 + 2}, {x0 + 2, z0}};
            for (int k = 0; k < 6; k++) {
                int16_t *v = floor_verts[tile * 6 + k];
                v[0] = corners[tri_corners[k]][0] * MESH_ONE;
                v[1] = FLOOR_Y * MESH_ONE;
                v[2] = corners[tri_corners[k]][1] * MESH_ONE;
            }
            uint8_t shade = (i + j) % 2 ? 60 : 90;
            for (int t = 0; t < 2; t++) {
                floor_colors[tile * 2 + t][0] = shade;
                floor_colors[tile * 2 + t][1] = shade;
                floor_colors[tile * 2 + t][2] = shade + 10;
            }
        }
    }
    view v = {0, 0, 0, 200, 500};
    return (world){0, 0, 0, 0, v, v};
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

/* Move cur 1/rate of the way to target, at least 1 so it always arrives. */
static int ease(int cur, int target, int rate)
{
    int d = target - cur, step = d / rate;
    if (step == 0 && d != 0)
        step = d > 0 ? 1 : -1;
    return cur + step;
}

static int wrap3600(int a)
{
    return (a % 3600 + 3600) % 3600;
}

world world_update(world w, unsigned input)
{
    /* Strafe relative to the camera: forward is away from it, along the ground. */
    int right = !!(input & MOVE_RIGHT) - !!(input & MOVE_LEFT);
    int fwd = !!(input & MOVE_FWD) - !!(input & MOVE_BACK);
    int s = isin(w.cam.yaw / 10), c = icos(w.cam.yaw / 10);
    w.x = clamp(w.x + (right * c - fwd * s) * MOVE_SPEED / 4096, -GUY_LIMIT, GUY_LIMIT);
    w.z = clamp(w.z + (-right * s - fwd * c) * MOVE_SPEED / 4096, -GUY_LIMIT, GUY_LIMIT);

    /* Face the way we're moving: the camera's yaw plus the stick direction
     * (forward 0, left +90, right -90, back 180; diagonals in between). */
    if (right || fwd) {
        static const int offset[3][3] = {
            /* fwd: -1     0     1 */
            {1350, 900, 450},    /* right = -1 */
            {1800, 0, 0},        /* right =  0 */
            {-1350, -900, -450}, /* right =  1 */
        };
        w.target_heading = wrap3600(w.cam.yaw + offset[right + 1][fwd + 1]);
    }
    /* Turn the short way round. */
    int turn = wrap3600(w.target_heading - w.heading + 1800) - 1800;
    w.heading = wrap3600(w.heading + ease(0, turn, TURN_EASE));

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

    w.cam.x = ease(w.cam.x, w.target.x, EASE);
    w.cam.z = ease(w.cam.z, w.target.z, EASE);
    w.cam.yaw = ease(w.cam.yaw, w.target.yaw, EASE);
    w.cam.pitch = ease(w.cam.pitch, w.target.pitch, EASE);
    w.cam.dist = ease(w.cam.dist, w.target.dist, EASE);
    return w;
}
