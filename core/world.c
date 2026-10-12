#include "world.h"

/* A small test course: a wall, three steps up to a floating platform, a pillar, and a
 * curb low enough to walk up. */
const part level_parts[] = {
    {-4000, 1000, 0, 1000, 2000, 6000, {110, 120, 140}},     /* wall */
    {0, 300, -4000, 2000, 600, 2000, {196, 40, 28}},         /* step 1, top 0.6 */
    {0, 600, -6000, 2000, 1200, 2000, {245, 205, 48}},       /* step 2, top 1.2 */
    {0, 900, -8000, 2000, 1800, 2000, {75, 151, 75}},        /* step 3, top 1.8 */
    {3500, 1650, -8000, 3000, 300, 2000, {13, 105, 172}},    /* floating platform, top 1.8 */
    {6000, 1500, -4000, 1000, 3000, 1000, {107, 50, 124}},   /* pillar */
    {0, 150, 4000, 4000, 300, 1000, {160, 165, 170}},        /* curb, 0.3 high: step up */
};
const int level_part_count = sizeof level_parts / sizeof level_parts[0];

#define FLOOR_TRIS (FLOOR_TILES * FLOOR_TILES * 2)
#define PART_TRIS 10 /* 5 faces: no bottom */
#define LEVEL_TRIS (FLOOR_TRIS + PART_TRIS * (int)(sizeof level_parts / sizeof level_parts[0]))
static int16_t level_verts[LEVEL_TRIS * 3][3];
static uint8_t level_colors[LEVEL_TRIS][3];
const mesh level_mesh = {level_verts, level_colors, LEVEL_TRIS};

#define MOVE_SPEED 60 /* thousandths of a unit per frame */
#define DIAGONAL 2896 /* 1/sqrt(2), scaled by 4096 */
#define ORBIT_SPEED 20
#define ZOOM_SPEED 100
#define JUMP_SPEED 98 /* peaks about 1.15 units up after ~0.4 s */
#define GRAVITY 4
#define STEP_HEIGHT 400 /* ledges up to this high are walked up, not jumped */
#define EASE 8      /* the camera closes 1/EASE of the gap each frame */
#define TURN_EASE 4 /* players turn faster than the camera follows */

/* Append a quad (corners in order) as two triangles. Coordinates in thousandths. */
static int add_quad(int t, const int c[4][3], const uint8_t color[3], int shade)
{
    static const int corner[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; k++)
        for (int a = 0; a < 3; a++)
            level_verts[t * 3 + k][a] = c[corner[k]][a] * MESH_ONE / 1000;
    for (int i = 0; i < 2; i++)
        for (int a = 0; a < 3; a++)
            level_colors[t + i][a] = color[a] * shade / 100;
    return t + 2;
}

static void build_level_mesh(void)
{
    const int fy = FLOOR_Y * 1000;
    int t = 0;
    for (int i = 0; i < FLOOR_TILES; i++) {
        for (int j = 0; j < FLOOR_TILES; j++) {
            int x0 = (2 * i - FLOOR_TILES) * 1000, z0 = (2 * j - FLOOR_TILES) * 1000;
            const int c[4][3] = {{x0, fy, z0}, {x0, fy, z0 + 2000}, {x0 + 2000, fy, z0 + 2000}, {x0 + 2000, fy, z0}};
            uint8_t shade = (i + j) % 2 ? 60 : 90;
            const uint8_t color[3] = {shade, shade, shade + 10};
            t = add_quad(t, c, color, 100);
        }
    }
    /* Parts: flat-shaded by face direction so the edges read without lighting. */
    for (int i = 0; i < level_part_count; i++) {
        const part *p = &level_parts[i];
        int x0 = p->x - p->sx / 2, x1 = p->x + p->sx / 2;
        int y0 = fy + p->y - p->sy / 2, y1 = fy + p->y + p->sy / 2;
        int z0 = p->z - p->sz / 2, z1 = p->z + p->sz / 2;
        const int faces[5][4][3] = {
            {{x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}}, /* top */
            {{x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}}, /* +X */
            {{x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, {x0, y0, z0}}, /* -X */
            {{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}}, /* +Z */
            {{x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}}, /* -Z */
        };
        static const int shade[5] = {100, 80, 80, 65, 65};
        for (int f = 0; f < 5; f++)
            t = add_quad(t, faces[f], p->color, shade[f]);
    }
}

world world_init(int players)
{
    build_level_mesh();
    world w = {0};
    w.player_count = players;
    for (int i = 0; i < players; i++) {
        w.players[i].x = i * 1500; /* side by side, standing on the floor */
        w.players[i].on_ground = 1;
    }
    return w;
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

/* The part the player's box overlaps, or NULL. Touching (e.g. standing on top) isn't
 * overlapping. */
static const part *hit_part(const player *p)
{
    for (int i = 0; i < level_part_count; i++) {
        const part *q = &level_parts[i];
        if (p->x + PLAYER_RADIUS > q->x - q->sx / 2 && p->x - PLAYER_RADIUS < q->x + q->sx / 2 &&
            p->z + PLAYER_RADIUS > q->z - q->sz / 2 && p->z - PLAYER_RADIUS < q->z + q->sz / 2 &&
            p->y + PLAYER_HEIGHT > q->y - q->sy / 2 && p->y < q->y + q->sy / 2)
            return q;
    }
    return 0;
}

/* Move *pos (p->x or p->z) by d. Walking into a part either steps up onto it (if
 * grounded and it's low enough, with room above) or stops flush against its face. */
static void move_axis(player *p, int *pos, int d, int along_x)
{
    if (d == 0)
        return;
    int old = *pos;
    *pos = clamp(*pos + d, -PLAYER_LIMIT, PLAYER_LIMIT);
    const part *q = hit_part(p);
    if (!q)
        return;

    int top = q->y + q->sy / 2, old_y = p->y;
    if (p->on_ground && top - p->y <= STEP_HEIGHT) {
        p->y = top;
        if (!hit_part(p))
            return;
        p->y = old_y;
    }

    int lo = along_x ? q->x - q->sx / 2 : q->z - q->sz / 2;
    int hi = along_x ? q->x + q->sx / 2 : q->z + q->sz / 2;
    *pos = d > 0 ? lo - PLAYER_RADIUS : hi + PLAYER_RADIUS;
    /* Another part in the way, or flush would mean moving backwards: stay put. */
    if (hit_part(p) || (d > 0 ? *pos < old : *pos > old))
        *pos = old;
}

static player update_player(player p, input in)
{
    /* Strafe relative to the camera: forward is away from it, along the ground. */
    int right = !!(in.buttons & MOVE_RIGHT) - !!(in.buttons & MOVE_LEFT);
    int fwd = !!(in.buttons & MOVE_FWD) - !!(in.buttons & MOVE_BACK);
    int s = isin(in.yaw / 10), c = icos(in.yaw / 10);
    int step = right && fwd ? MOVE_SPEED * DIAGONAL / 4096 : MOVE_SPEED; /* same speed diagonally */

    /* One axis at a time, so walls block one direction and you slide along the other. */
    move_axis(&p, &p.x, (right * c - fwd * s) * step / 4096, 1);
    move_axis(&p, &p.z, (-right * s - fwd * c) * step / 4096, 0);

    /* Jump from the floor or a part; gravity brings you back. */
    if ((in.buttons & JUMP) && p.on_ground)
        p.vy = JUMP_SPEED;
    /* Gravity first, so standing still still pushes into the ground each frame and
     * on_ground stays set. */
    p.vy -= GRAVITY;
    int dy = p.vy;
    p.y += dy;
    p.on_ground = 0;
    const part *q = hit_part(&p);
    if (q && dy <= 0) { /* landed on top */
        p.y = q->y + q->sy / 2;
        p.vy = 0;
        p.on_ground = 1;
    } else if (q) { /* bumped your head */
        p.y = q->y - q->sy / 2 - PLAYER_HEIGHT;
        p.vy = 0;
    }
    if (p.y <= 0) {
        p.y = p.vy = 0;
        p.on_ground = 1;
    }

    /* Face the way you're moving: the camera's yaw plus the stick direction
     * (forward 0, left +90, right -90, back 180; diagonals in between). */
    if (right || fwd) {
        static const int offset[3][3] = {
            /* fwd: -1     0     1 */
            {1350, 900, 450},    /* right = -1 */
            {1800, 0, 0},        /* right =  0 */
            {-1350, -900, -450}, /* right =  1 */
        };
        p.target_heading = wrap3600(in.yaw + offset[right + 1][fwd + 1]);
    }
    /* Turn the short way round. */
    int turn = wrap3600(p.target_heading - p.heading + 1800) - 1800;
    p.heading = wrap3600(p.heading + ease(0, turn, TURN_EASE));
    return p;
}

world world_update(world w, const input inputs[MAX_PLAYERS])
{
    for (int i = 0; i < w.player_count; i++)
        w.players[i] = update_player(w.players[i], inputs[i]);
    return w;
}

camera camera_init(void)
{
    view v = {0, 0, 0, 0, 200, 5000};
    return (camera){v, v, 0};
}

camera camera_update(camera c, unsigned buttons, const player *follow)
{
    c.target.x = follow->x;
    /* Follow the ground they stand on fully, but only half of a jump, so they stay in
     * frame and you still see the height. Dropping below it (walking off) counts too. */
    if (follow->on_ground || follow->y < c.ground)
        c.ground = follow->y;
    c.target.y = c.ground + (follow->y - c.ground) / 2;
    c.target.z = follow->z;
    if (buttons & CAM_LEFT)  c.target.yaw -= ORBIT_SPEED;
    if (buttons & CAM_RIGHT) c.target.yaw += ORBIT_SPEED;
    if (buttons & CAM_UP)    c.target.pitch += ORBIT_SPEED;
    if (buttons & CAM_DOWN)  c.target.pitch -= ORBIT_SPEED;
    if (buttons & CAM_IN)    c.target.dist -= ZOOM_SPEED;
    if (buttons & CAM_OUT)   c.target.dist += ZOOM_SPEED;
    c.target.pitch = clamp(c.target.pitch, -CAM_PITCH_MAX, CAM_PITCH_MAX);
    c.target.dist = clamp(c.target.dist, CAM_DIST_MIN, CAM_DIST_MAX);

    c.now.x = ease(c.now.x, c.target.x, EASE);
    c.now.y = ease(c.now.y, c.target.y, EASE);
    c.now.z = ease(c.now.z, c.target.z, EASE);
    c.now.yaw = ease(c.now.yaw, c.target.yaw, EASE);
    c.now.pitch = ease(c.now.pitch, c.target.pitch, EASE);
    c.now.dist = ease(c.now.dist, c.target.dist, EASE);
    return c;
}
