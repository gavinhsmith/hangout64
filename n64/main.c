#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include "cube.h"

int main(void)
{
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    gl_init();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.1333f, 0.1333f, -0.1f, 0.1f, 0.2f, 100.0f);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    int angle = 0;
    for (;;) {
        rdpq_attach(display_get(), display_get_zbuf());
        gl_context_begin();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -5.0f);
        glRotatef((float)angle, 1.0f, 0.0f, 0.0f);
        glRotatef((float)angle, 0.0f, 1.0f, 0.0f);

        glBegin(GL_QUADS);
        for (int f = 0; f < CUBE_FACES; f++) {
            glColor3ub(cube_colors[f][0], cube_colors[f][1], cube_colors[f][2]);
            for (int c = 0; c < 4; c++) {
                const int8_t *v = cube_verts[f * 4 + c];
                glVertex3f(v[0], v[1], v[2]);
            }
        }
        glEnd();

        gl_context_end();
        rdpq_detach_show();
        angle = cube_step(angle);
    }
}
