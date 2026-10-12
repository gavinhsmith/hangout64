#include <nds.h>
#include <stdio.h>
#include "world.h"

/* D-pad moves, A jumps, R + D-pad orbits, X/Y zoom in/out. */
static unsigned read_input(u32 keys)
{
    unsigned in = 0;
    int orbit = (keys & KEY_R) != 0;
    if (keys & KEY_LEFT)  in |= orbit ? CAM_LEFT : MOVE_LEFT;
    if (keys & KEY_RIGHT) in |= orbit ? CAM_RIGHT : MOVE_RIGHT;
    if (keys & KEY_UP)    in |= orbit ? CAM_UP : MOVE_FWD;
    if (keys & KEY_DOWN)  in |= orbit ? CAM_DOWN : MOVE_BACK;
    if (keys & KEY_A)     in |= JUMP;
    if (keys & KEY_X)     in |= CAM_IN;
    if (keys & KEY_Y)     in |= CAM_OUT;
    return in;
}

/* Tenths of a degree -> DS angle units (wrapped first so it can't overflow). */
static int angle(int deci)
{
    return (deci % 3600 + 3600) % 3600 * DEGREES_IN_CIRCLE / 3600;
}

/* Thousandths of a unit -> f32. */
static int f32m(int thousandths)
{
    return thousandths * 4096 / 1000;
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
    printf("hangout64\n\nD-pad:     move\nA:         jump\nR + D-pad: orbit\nX/Y:       zoom\nSTART:     exit\n");
    glInit();
    glClearColor(3, 3, 5, 31);
    glClearPolyID(63);
    glClearDepth(GL_MAX_DEPTH);
    glViewport(0, 0, 255, 191);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(53, 256.0 / 192.0, 0.2, 100);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);

    world w = world_init(1);
    camera cam = camera_init();
    for (;;) {
        scanKeys();
        if (keysDown() & KEY_START)
            break;

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        const view *v = &cam.now;
        glTranslatef32(0, 0, -f32m(v->dist));
        glRotateXi(angle(v->pitch));
        glRotateYi(angle(-v->yaw));
        glTranslatef32(-f32m(v->x), -f32m(v->y), -f32m(v->z));
        draw_mesh(&level_mesh);

        for (int i = 0; i < w.player_count; i++) {
            const player *p = &w.players[i];
            glPushMatrix();
            glTranslatef32(f32m(p->x), inttof32(FLOOR_Y) + f32m(p->y), f32m(p->z));
            glRotateYi(angle(p->heading));
            draw_mesh(&guy_mesh);
            glPopMatrix(1);
        }

        glFlush(0);
        swiWaitForVBlank();
        unsigned b = read_input(keysHeld());
        input in[MAX_PLAYERS] = {{b, cam.now.yaw}};
        w = world_update(w, in);
        cam = camera_update(cam, b, &w.players[0]);
    }
    return 0;
}
