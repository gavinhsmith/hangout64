#include <libdragon.h>
#include <GL/gl.h>
#include <GL/gl_integration.h>
#include "world.h"

#define STICK_DEADZONE 32

static place pl; /* the loaded place; mesh parts are drawn from it */

/* Stick moves, A jumps, D-pad orbits, C-up/C-down zoom in/out. */
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
    if (b.a)       bits |= JUMP;
    return bits;
}

static void draw_mesh(const mesh *m)
{
    const float k = 1.0f / MESH_ONE;
    glBegin(GL_TRIANGLES);
    for (int t = 0; t < m->tris; t++) {
        glColor3ub(m->colors[t][0], m->colors[t][1], m->colors[t][2]);
        for (int c = 0; c < 3; c++) {
            const int16_t *v = m->verts[t * 3 + c];
            glVertex3f(v[0] * k, v[1] * k, v[2] * k);
        }
    }
    glEnd();
}

/* Record a mesh once as a display list (an RSP command block) so drawing it each frame
 * costs the CPU one call instead of a GL call per vertex. */
static GLuint compile_mesh(const mesh *m)
{
    GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    draw_mesh(m);
    glEndList();
    return list;
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

    assertf(place_load(&pl, default_place, default_place_size) == PLACE_OK, "built-in place failed to load");
    world w = world_init(&pl, 1);
    camera cam = camera_init();
    GLuint level_list = compile_mesh(&level_mesh), guy_list = compile_mesh(&guy_mesh);
    GLuint mesh_lists[PLACE_MAX_MESHES];
    for (int i = 0; i < pl.mesh_count; i++)
        mesh_lists[i] = compile_mesh(&pl.meshes[i]);
    for (;;) {
        rdpq_attach(display_get(), display_get_zbuf());
        gl_context_begin();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        const view *v = &cam.now;
        glTranslatef(0.0f, 0.0f, -v->dist / 1000.0f);
        glRotatef(v->pitch / 10.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(-v->yaw / 10.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(-v->x / 1000.0f, -v->y / 1000.0f, -v->z / 1000.0f);
        glCallList(level_list);

        /* Mesh parts: the model's origin sits at the part's bottom centre. */
        for (int i = 0; i < pl.part_count; i++) {
            const part *q = &pl.parts[i];
            if (q->mesh < 0)
                continue;
            glPushMatrix();
            glTranslatef(q->x / 1000.0f, FLOOR_Y + (q->y - q->sy / 2) / 1000.0f, q->z / 1000.0f);
            glCallList(mesh_lists[q->mesh]);
            glPopMatrix();
        }

        for (int i = 0; i < w.player_count; i++) {
            const player *p = &w.players[i];
            glPushMatrix();
            glTranslatef(p->x / 1000.0f, FLOOR_Y + p->y / 1000.0f, p->z / 1000.0f);
            glRotatef(p->heading / 10.0f, 0.0f, 1.0f, 0.0f);
            glCallList(guy_list);
            glPopMatrix();
        }

        gl_context_end();
        rdpq_detach_show();
        unsigned b = read_input();
        input in[MAX_PLAYERS] = {{b, cam.now.yaw}};
        w = world_update(w, in);
        cam = camera_update(cam, b, &w.players[0]);
    }
}
