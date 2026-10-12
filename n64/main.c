#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include "cube.h"

#define STICK_DEADZONE 32

/* Stick moves, D-pad orbits, C-up/C-down zoom in/out. */
static unsigned read_input(void)
{
    joypad_poll();
    joypad_inputs_t in = joypad_get_inputs(JOYPAD_PORT_1);
    joypad_buttons_t b = joypad_get_buttons_held(JOYPAD_PORT_1);
    unsigned bits = 0;
    if (in.stick_x < -STICK_DEADZONE) bits |= MOVE_LEFT;
    if (in.stick_x > STICK_DEADZONE)  bits |= MOVE_RIGHT;
    if (in.stick_y > STICK_DEADZONE)  bits |= MOVE_FWD;
    if (in.stick_y < -STICK_DEADZONE) bits |= MOVE_BACK;
    if (b.d_left)  bits |= CAM_LEFT;
    if (b.d_right) bits |= CAM_RIGHT;
    if (b.d_up)    bits |= CAM_UP;
    if (b.d_down)  bits |= CAM_DOWN;
    if (b.c_up)    bits |= CAM_IN;
    if (b.c_down)  bits |= CAM_OUT;
    return bits;
}

static void draw_mesh(const mesh *m)
{
    glBegin(GL_QUADS);
    for (int q = 0; q < m->quads; q++) {
        glColor3ub(m->colors[q][0], m->colors[q][1], m->colors[q][2]);
        for (int c = 0; c < 4; c++) {
            const int8_t *v = m->verts[q * 4 + c];
            glVertex3f(v[0], v[1], v[2]);
        }
    }
    glEnd();
}

int main(void)
{
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    gl_init();
    joypad_init();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.1333f, 0.1333f, -0.1f, 0.1f, 0.2f, 100.0f);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    world w = world_init();
    for (;;) {
        rdpq_attach(display_get(), display_get_zbuf());
        gl_context_begin();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -w.cam.dist / 100.0f);
        glRotatef(w.cam.pitch / 10.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(-w.cam.yaw / 10.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(-w.cam.x / 100.0f, 0.0f, -w.cam.z / 100.0f);
        draw_mesh(&floor_mesh);

        glTranslatef(w.x / 100.0f, 0.0f, w.z / 100.0f);
        glRotatef((float)w.spin, 1.0f, 0.0f, 0.0f);
        glRotatef((float)w.spin, 0.0f, 1.0f, 0.0f);
        draw_mesh(&cube_mesh);

        gl_context_end();
        rdpq_detach_show();
        w = world_update(w, read_input());
    }
}
