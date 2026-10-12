/* Host-side checks for core. Run with `make -C core test`. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "world.h"

/* Run `frames` frames with player 0 holding `buttons`, looking along `yaw`. */
static world run(world w, unsigned buttons, int yaw, int frames)
{
    input in[MAX_PLAYERS] = {{buttons, yaw}};
    while (frames--)
        w = world_update(w, in);
    return w;
}

static camera follow(camera c, unsigned buttons, const player *p, int frames)
{
    while (frames--)
        c = camera_update(c, buttons, p);
    return c;
}

int main(void)
{
    assert(isin(0) == 0 && isin(90) == 4096 && isin(270) == -4096 && icos(0) == 4096);
    assert(abs(isin(30) - 2048) < 20 && abs(isin(-30) + 2048) < 20);

    world w = world_init(1);
    const player *p0 = &w.players[0];
    assert(w.player_count == 1 && p0->x == 0 && p0->z == 0);

    /* Movement is relative to the viewer's camera yaw: at 0, forward is -Z, right is +X. */
    world f = run(w, MOVE_FWD, 0, 10);
    assert(f.players[0].z == -600 && f.players[0].x == 0);
    world r = run(w, MOVE_RIGHT, 0, 10);
    assert(r.players[0].x == 600 && r.players[0].z == 0);
    world side = run(w, MOVE_FWD, 900, 10); /* camera orbited to the +X side */
    assert(side.players[0].x == -600 && abs(side.players[0].z) <= 10);

    /* Diagonals move at the same speed (within rounding), not sqrt(2) faster. */
    world d = run(w, MOVE_FWD | MOVE_RIGHT, 0, 10);
    int dist2 = d.players[0].x * d.players[0].x + d.players[0].z * d.players[0].z;
    assert(d.players[0].x > 0 && d.players[0].z < 0 && dist2 > 580 * 580 && dist2 <= 600 * 600);

    /* Jump: up about 1.2 units, back down, no jumping again mid-air. */
    world j = run(w, JUMP, 0, 1);
    assert(j.players[0].y > 0 && !j.players[0].on_ground);
    int peak = 0;
    for (int i = 0; i < 100; i++) {
        j = run(j, 0, 0, 1);
        peak = j.players[0].y > peak ? j.players[0].y : peak;
    }
    assert(peak > 1100 && peak < 1300 && j.players[0].y == 0 && j.players[0].on_ground);
    world air = run(w, JUMP, 0, 10);
    assert(run(air, JUMP, 0, 1).players[0].vy < air.players[0].vy);

    /* Collision: the wall (x -4.5..-3.5) stops you; the side of step 1 (z -5..-3,
     * 0.6 high) can't be walked up; jumping gets you on top; you land on tops. */
    world wall = run(w, MOVE_LEFT, 0, 200);
    assert(wall.players[0].x >= -3150 && wall.players[0].x < -3090);
    world step = run(w, MOVE_FWD, 0, 200);
    assert(step.players[0].z >= -2650 && step.players[0].z < -2590 && step.players[0].y == 0);
    world up = run(step, MOVE_FWD | JUMP, 0, 25);
    up = run(up, 0, 0, 60);
    assert(up.players[0].on_ground && up.players[0].y == 600);
    world drop = w;
    drop.players[0].z = -4000;
    drop.players[0].y = 3000;
    drop = run(drop, 0, 0, 100);
    assert(drop.players[0].y == 600 && drop.players[0].on_ground);
    for (int i = 0; i < 4; i++) { /* standing on a part: grounded every frame, not every other */
        drop = run(drop, 0, 0, 1);
        assert(drop.players[0].on_ground && drop.players[0].y == 600);
    }

    /* Players move independently from their own inputs. */
    world two = world_init(2);
    input in[MAX_PLAYERS] = {{MOVE_RIGHT, 0}, {MOVE_FWD, 0}};
    for (int i = 0; i < 10; i++)
        two = world_update(two, in);
    assert(two.players[0].x == 600 && two.players[0].z == 0);
    assert(two.players[1].x == 1500 && two.players[1].z == -600);

    /* Players turn (the short way) to face where they move, and keep facing it. */
    assert(run(w, MOVE_RIGHT, 0, 100).players[0].heading == 2700);
    assert(run(w, MOVE_BACK, 0, 100).players[0].heading == 1800);
    assert(run(w, MOVE_LEFT | MOVE_FWD, 0, 100).players[0].heading == 450);
    assert(run(r, 0, 0, 100).players[0].heading == 2700);
    world turn = w;
    turn.players[0].heading = 100;
    turn.players[0].target_heading = 3500;
    assert(run(turn, 0, 0, 1).players[0].heading < 100);
    assert(run(turn, 0, 0, 100).players[0].heading == 3500);

    /* The camera follows with a lag, then settles exactly; orbit/zoom ease and clamp. */
    camera c = follow(camera_init(), 0, &f.players[0], 1);
    assert(c.now.z > f.players[0].z && c.now.z < 0);
    c = follow(c, 0, &f.players[0], 200);
    assert(c.now.z == f.players[0].z && c.now.x == f.players[0].x);
    camera o = follow(camera_init(), CAM_RIGHT, p0, 1);
    assert(o.target.yaw == 20 && o.now.yaw > 0 && o.now.yaw < 20);
    o = follow(o, CAM_UP | CAM_IN, p0, 300);
    assert(o.now.pitch == CAM_PITCH_MAX && o.now.dist == CAM_DIST_MIN);

    /* Standing on a part, the camera settles at its height; a jump from there only
     * raises the camera by half the jump. */
    camera onstep = follow(camera_init(), 0, &drop.players[0], 300);
    assert(onstep.now.y == 600);
    world hop = run(drop, JUMP, 0, 10);
    onstep = camera_update(onstep, 0, &hop.players[0]);
    assert(onstep.target.y == 600 + (hop.players[0].y - 600) / 2);

    /* Model: feet on y = 0, about 2.4 units tall. */
    int lo = 0, hi = 0;
    for (int i = 0; i < guy_mesh.tris * 3; i++) {
        lo = guy_mesh.verts[i][1] < lo ? guy_mesh.verts[i][1] : lo;
        hi = guy_mesh.verts[i][1] > hi ? guy_mesh.verts[i][1] : hi;
    }
    assert(lo == 0 && abs(hi - 24 * MESH_ONE / 10) <= 1);

    /* Level mesh: checkered floor at FLOOR_Y, then 10 triangles per part, its top face
     * first, drawn at the part's top. */
    int floor_tris = FLOOR_TILES * FLOOR_TILES * 2;
    assert(level_mesh.tris == floor_tris + 10 * level_part_count);
    for (int i = 0; i < floor_tris * 3; i++)
        assert(level_mesh.verts[i][1] == FLOOR_Y * MESH_ONE);
    assert(level_mesh.colors[0][0] != level_mesh.colors[2][0]);
    const part *s1 = &level_parts[1];
    assert(level_mesh.verts[(floor_tris + 10) * 3][1] == (FLOOR_Y * 1000 + s1->y + s1->sy / 2) * MESH_ONE / 1000);

    puts("core: ok");
    return EXIT_SUCCESS;
}
