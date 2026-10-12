#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include "world.h"

/* Power-of-two texture holding the 320x240 frame, so plain GL 1.1 works everywhere. */
#define TEX_W 512
#define TEX_H 256
#define TICK_MS (1000.0 / 60.0)

/* WASD moves, arrows orbit, Q/E zoom in/out. */
static unsigned read_input(void)
{
    const bool *k = SDL_GetKeyboardState(NULL);
    unsigned in = 0;
    if (k[SDL_SCANCODE_A])     in |= MOVE_LEFT;
    if (k[SDL_SCANCODE_D])     in |= MOVE_RIGHT;
    if (k[SDL_SCANCODE_W])     in |= MOVE_FWD;
    if (k[SDL_SCANCODE_S])     in |= MOVE_BACK;
    if (k[SDL_SCANCODE_LEFT])  in |= CAM_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) in |= CAM_RIGHT;
    if (k[SDL_SCANCODE_UP])    in |= CAM_UP;
    if (k[SDL_SCANCODE_DOWN])  in |= CAM_DOWN;
    if (k[SDL_SCANCODE_Q])     in |= CAM_IN;
    if (k[SDL_SCANCODE_E])     in |= CAM_OUT;
    return in;
}

static void draw_mesh(const mesh *m)
{
    const float k = 1.0f / MESH_ONE;
    glBegin(GL_TRIANGLES);
    for (int t = 0; t < m->tris; t++) {
        glColor3ubv(m->colors[t]);
        for (int c = 0; c < 3; c++) {
            const int16_t *v = m->verts[t * 3 + c];
            glVertex3f(v[0] * k, v[1] * k, v[2] * k);
        }
    }
    glEnd();
}

static void draw_scene(const world *w)
{
    glViewport(0, 0, SCREEN_W, SCREEN_H);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.1333, 0.1333, -0.1, 0.1, 0.2, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -w->cam.dist / 100.0f);
    glRotatef(w->cam.pitch / 10.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(-w->cam.yaw / 10.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(-w->cam.x / 100.0f, 0.0f, -w->cam.z / 100.0f);
    draw_mesh(&floor_mesh);

    glTranslatef(w->x / 100.0f, (float)FLOOR_Y, w->z / 100.0f);
    glRotatef(w->heading / 10.0f, 0.0f, 1.0f, 0.0f);
    draw_mesh(&guy_mesh);
}

/* Grab the 320x240 frame and draw it integer-scaled, centred, black bars around. */
static void present(SDL_Window *win, GLuint tex)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, SCREEN_W, SCREEN_H);

    int w, h;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    int scale = SDL_min(w / SCREEN_W, h / SCREEN_H);
    if (scale < 1)
        scale = 1;
    float x0 = (w - SCREEN_W * scale) / 2.0f, y0 = (h - SCREEN_H * scale) / 2.0f;
    float x1 = x0 + SCREEN_W * scale, y1 = y0 + SCREEN_H * scale;
    float u = (float)SCREEN_W / TEX_W, v = (float)SCREEN_H / TEX_H;

    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, 0, h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x0, y0);
    glTexCoord2f(u, 0); glVertex2f(x1, y0);
    glTexCoord2f(u, v); glVertex2f(x1, y1);
    glTexCoord2f(0, v); glVertex2f(x0, y1);
    glEnd();

    SDL_GL_SwapWindow(win);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_Window *win = SDL_CreateWindow("hangout64", SCREEN_W * 3, SCREEN_H * 3,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win || !SDL_GL_CreateContext(win)) {
        SDL_Log("window: %s", SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(win, SCREEN_W, SCREEN_H);
    SDL_GL_SetSwapInterval(1);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_W, TEX_H, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    world w = world_init();
    double next = (double)SDL_GetTicks();
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT ||
                (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE))
                goto done;
        }
        /* Fixed 60 Hz update regardless of monitor refresh rate. */
        while ((double)SDL_GetTicks() >= next) {
            w = world_update(w, read_input());
            next += TICK_MS;
        }
        draw_scene(&w);
        present(win, tex);
    }
done:
    SDL_Quit();
    return 0;
}
