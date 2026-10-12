/* Host-side checks for core. Run with `make -C core test`. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "world.h"

static world run(world w, unsigned input, int frames)
{
    while (frames--)
        w = world_update(w, input);
    return w;
}

int main(void)
{
    assert(isin(0) == 0 && isin(90) == 4096 && isin(270) == -4096 && icos(0) == 4096);
    assert(abs(isin(30) - 2048) < 20 && abs(isin(-30) + 2048) < 20);

    world w = world_init();

    /* Camera starts behind the guy looking down -Z: forward is -Z, right is +X. */
    world f = run(w, MOVE_FWD, 10);
    assert(f.z == -60 && f.x == 0);
    world r = run(w, MOVE_RIGHT, 10);
    assert(r.x == 60 && r.z == 0);

    /* The camera follows, lagging behind, then settles exactly on the guy. */
    assert(f.cam.z > f.z && f.cam.z < 0);
    f = run(f, 0, 200);
    assert(f.cam.z == f.z && f.cam.x == f.x);

    /* Camera orbited to the +X side: forward now moves -X. */
    world side = w;
    side.cam.yaw = side.target.yaw = 900;
    side = run(side, MOVE_FWD, 10);
    assert(side.x == -60 && abs(side.z) <= 1);

    /* Orbit input eases the camera to its target; limits hold. */
    world o = run(w, CAM_RIGHT, 1);
    assert(o.target.yaw == 20 && o.cam.yaw > 0 && o.cam.yaw < 20);
    o = run(o, CAM_UP | CAM_IN, 300);
    assert(o.target.pitch == CAM_PITCH_MAX && o.target.dist == CAM_DIST_MIN);
    assert(o.cam.pitch == CAM_PITCH_MAX && o.cam.dist == CAM_DIST_MIN);
    assert(run(w, MOVE_FWD, 1000).z == -GUY_LIMIT);

    /* The guy turns (the short way) to face where it moves. */
    assert(run(w, MOVE_RIGHT, 100).heading == 2700);
    assert(run(w, MOVE_BACK, 100).heading == 1800);
    assert(run(w, MOVE_LEFT | MOVE_FWD, 100).heading == 450);
    world turn = w;
    turn.heading = 100;
    turn.target_heading = 3500;
    assert(run(turn, 0, 1).heading < 100);
    assert(run(turn, 0, 100).heading == 3500);
    assert(run(r, 0, 100).heading == 2700); /* keeps facing that way when stopped */

    /* Model: feet on y = 0, about 2.4 units tall. */
    int lo = 0, hi = 0;
    for (int i = 0; i < guy_mesh.tris * 3; i++) {
        lo = guy_mesh.verts[i][1] < lo ? guy_mesh.verts[i][1] : lo;
        hi = guy_mesh.verts[i][1] > hi ? guy_mesh.verts[i][1] : hi;
    }
    assert(lo == 0 && abs(hi - 24 * MESH_ONE / 10) <= 1);

    /* Floor: flat, covering the guy's range, checkered. */
    assert(floor_mesh.tris == FLOOR_TILES * FLOOR_TILES * 2);
    for (int i = 0; i < floor_mesh.tris * 3; i++)
        assert(floor_mesh.verts[i][1] == FLOOR_Y * MESH_ONE);
    assert(floor_mesh.verts[0][0] == -FLOOR_TILES * MESH_ONE);
    assert(floor_mesh.colors[0][0] == floor_mesh.colors[1][0]); /* a tile's two halves */
    assert(floor_mesh.colors[0][0] != floor_mesh.colors[2][0]); /* next tile */

    puts("core: ok");
    return EXIT_SUCCESS;
}
