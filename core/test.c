/* Host-side checks for core. Run with `make -C core test`. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "cube.h"

int main(void)
{
    assert(cube_step(0) == 1);
    assert(cube_step(359) == 0);

    /* Each face is planar: all 4 corners share one axis at +/-1. */
    for (int f = 0; f < CUBE_FACES; f++) {
        int shared = 0;
        for (int axis = 0; axis < 3; axis++) {
            int v = cube_verts[f * 4][axis], same = 1;
            for (int c = 1; c < 4; c++)
                same &= cube_verts[f * 4 + c][axis] == v;
            shared += same;
        }
        assert(shared == 1);
    }

    puts("core: ok");
    return EXIT_SUCCESS;
}
