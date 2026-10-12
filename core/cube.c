#include "cube.h"

const int8_t cube_verts[CUBE_FACES * 4][3] = {
    { 1, -1, -1}, { 1,  1, -1}, { 1,  1,  1}, { 1, -1,  1}, /* +X */
    {-1, -1,  1}, {-1,  1,  1}, {-1,  1, -1}, {-1, -1, -1}, /* -X */
    {-1,  1, -1}, {-1,  1,  1}, { 1,  1,  1}, { 1,  1, -1}, /* +Y */
    {-1, -1,  1}, {-1, -1, -1}, { 1, -1, -1}, { 1, -1,  1}, /* -Y */
    {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}, /* +Z */
    { 1, -1, -1}, {-1, -1, -1}, {-1,  1, -1}, { 1,  1, -1}, /* -Z */
};

const uint8_t cube_colors[CUBE_FACES][3] = {
    {230,  60,  60}, { 60, 200,  80}, { 70, 110, 230},
    {240, 200,  50}, {200,  80, 220}, { 60, 210, 220},
};

int cube_step(int angle)
{
    return (angle + 1) % 360;
}

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

camera camera_update(camera c, unsigned input)
{
    if (input & CAM_LEFT)  c.yaw -= 2;
    if (input & CAM_RIGHT) c.yaw += 2;
    if (input & CAM_UP)    c.pitch += 2;
    if (input & CAM_DOWN)  c.pitch -= 2;
    if (input & CAM_IN)    c.dist -= 1;
    if (input & CAM_OUT)   c.dist += 1;
    c.yaw = (c.yaw % 360 + 360) % 360;
    c.pitch = clamp(c.pitch, -CAM_PITCH_MAX, CAM_PITCH_MAX);
    c.dist = clamp(c.dist, CAM_DIST_MIN, CAM_DIST_MAX);
    return c;
}
