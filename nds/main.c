#include <nds.h>
#include "cube.h"

/* The DS renders at its native 256x192 (still 4:3, so same camera). */
int main(void)
{
    videoSetMode(MODE_0_3D);
    videoSetModeSub(MODE_0_2D); /* blank (black) bottom screen instead of white */
    glInit();
    glClearColor(3, 3, 5, 31);
    glClearPolyID(63);
    glClearDepth(GL_MAX_DEPTH);
    glViewport(0, 0, 255, 191);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(53, 256.0 / 192.0, 0.2, 100);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);

    int angle = 0;
    for (;;) {
        scanKeys();
        if (keysDown() & KEY_START)
            break;

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef32(0, 0, inttof32(-5));
        glRotateXi(degreesToAngle(angle));
        glRotateYi(degreesToAngle(angle));

        glBegin(GL_QUADS);
        for (int f = 0; f < CUBE_FACES; f++) {
            glColor3b(cube_colors[f][0], cube_colors[f][1], cube_colors[f][2]);
            for (int c = 0; c < 4; c++) {
                const int8_t *v = cube_verts[f * 4 + c];
                glVertex3v16(inttov16(v[0]), inttov16(v[1]), inttov16(v[2]));
            }
        }
        glEnd();

        glFlush(0);
        swiWaitForVBlank();
        angle = cube_step(angle);
    }
    return 0;
}
