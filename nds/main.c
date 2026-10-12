#include <nds.h>
#include <stdio.h>
#include "world.h"

/* D-pad moves, A/B/X/Y orbit, L/R zoom in/out. */
static unsigned read_input(u32 keys)
{
    unsigned in = 0;
    if (keys & KEY_LEFT)  in |= MOVE_LEFT;
    if (keys & KEY_RIGHT) in |= MOVE_RIGHT;
    if (keys & KEY_UP)    in |= MOVE_FWD;
    if (keys & KEY_DOWN)  in |= MOVE_BACK;
    if (keys & KEY_Y)     in |= CAM_LEFT;
    if (keys & KEY_A)     in |= CAM_RIGHT;
    if (keys & KEY_X)     in |= CAM_UP;
    if (keys & KEY_B)     in |= CAM_DOWN;
    if (keys & KEY_L)     in |= CAM_IN;
    if (keys & KEY_R)     in |= CAM_OUT;
    return in;
}

/* Tenths of a degree -> DS angle units (wrapped first so it can't overflow). */
static int angle(int deci)
{
    return (deci % 3600 + 3600) % 3600 * DEGREES_IN_CIRCLE / 3600;
}

/* Hundredths of a unit -> f32. */
static int f32h(int hundredths)
{
    return hundredths * 4096 / 100;
}

/* v16 is 4.12 and only reaches +/-8, the floor needs +/-10: send half-size
 * vertices (8.8 -> 4.12 is *16, halved is *8) and scale the matrix by 2. */
static void draw_mesh(const mesh *m)
{
    glPushMatrix();
    glScalef32(inttof32(2), inttof32(2), inttof32(2));
    glBegin(GL_TRIANGLES);
    for (int t = 0; t < m->tris; t++) {
        glColor3b(m->colors[t][0], m->colors[t][1], m->colors[t][2]);
        for (int c = 0; c < 3; c++) {
            const int16_t *v = m->verts[t * 3 + c];
            glVertex3v16(v[0] * 8, v[1] * 8, v[2] * 8);
        }
    }
    glEnd();
    glPopMatrix(1);
}

/* The DS renders at its native 256x192 (still 4:3, so same camera). */
int main(void)
{
    videoSetMode(MODE_0_3D);
    consoleDemoInit(); /* text console on the bottom screen */
    printf("hangout64\n\nD-pad:   move\nA/B/X/Y: orbit\nL/R:     zoom\nSTART:   exit\n");
    glInit();
    glClearColor(3, 3, 5, 31);
    glClearPolyID(63);
    glClearDepth(GL_MAX_DEPTH);
    glViewport(0, 0, 255, 191);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(53, 256.0 / 192.0, 0.2, 100);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);

    world w = world_init();
    for (;;) {
        scanKeys();
        if (keysDown() & KEY_START)
            break;

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef32(0, 0, -f32h(w.cam.dist));
        glRotateXi(angle(w.cam.pitch));
        glRotateYi(angle(-w.cam.yaw));
        glTranslatef32(-f32h(w.cam.x), 0, -f32h(w.cam.z));
        draw_mesh(&floor_mesh);

        glTranslatef32(f32h(w.x), inttof32(FLOOR_Y), f32h(w.z));
        glRotateYi(angle(w.heading));
        draw_mesh(&guy_mesh);

        glFlush(0);
        swiWaitForVBlank();
        w = world_update(w, read_input(keysHeld()));
    }
    return 0;
}
