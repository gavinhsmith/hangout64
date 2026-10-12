/* Host-side checks for core. Run with `make -C core test`. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "cube.h"

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
    assert(world_update(w, 0).spin == 1);

    /* Camera starts behind the cube looking down -Z: forward is -Z, right is +X. */
    world f = run(w, MOVE_FWD, 10);
    assert(f.z == -60 && f.x == 0);
    world r = run(w, MOVE_RIGHT, 10);
    assert(r.x == 60 && r.z == 0);

    /* The camera follows, lagging behind, then settles exactly on the cube. */
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
    assert(run(w, MOVE_FWD, 1000).z == -CUBE_LIMIT);

    /* Cube faces are planar: all 4 corners share one axis. */
    for (int q = 0; q < cube_mesh.quads; q++) {
        int shared = 0;
        for (int axis = 0; axis < 3; axis++) {
            int v = cube_mesh.verts[q * 4][axis], same = 1;
            for (int c = 1; c < 4; c++)
                same &= cube_mesh.verts[q * 4 + c][axis] == v;
            shared += same;
        }
        assert(shared == 1);
    }

    /* Floor: flat, covering the cube's range, checkered. */
    assert(floor_mesh.quads == FLOOR_TILES * FLOOR_TILES);
    for (int i = 0; i < floor_mesh.quads * 4; i++)
        assert(floor_mesh.verts[i][1] == FLOOR_Y);
    assert(floor_mesh.verts[0][0] == -FLOOR_TILES && floor_mesh.verts[0][2] == -FLOOR_TILES);
    assert(floor_mesh.colors[0][0] != floor_mesh.colors[1][0]);

    puts("core: ok");
    return EXIT_SUCCESS;
}
