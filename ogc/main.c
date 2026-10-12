#include <gccore.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif
#include "world.h"

#define FIFO_SIZE (256 * 1024)

static GXRModeObj mode;
static void *xfb[2];
static void *frame_tex;      /* the 320x240 frame, as an RGBA8 texture */
static int quad_x, quad_w;   /* where the frame lands in the full-size EFB */

static const GXColor BG = {25, 25, 38, 255};
static const GXColor BLACK = {0, 0, 0, 255};

/* Clear the 320x240 scene area to the background (and far Z). A texture copy is the
 * only way GX clears part of the EFB; the copied pixels are overwritten next frame.
 * Copy clears only touch Z while Z updates are enabled. */
static void clear_scene_area(void)
{
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetCopyClear(BG, GX_MAX_Z24);
    GX_SetTexCopySrc(0, 0, SCREEN_W, SCREEN_H);
    GX_SetTexCopyDst(SCREEN_W, SCREEN_H, GX_TF_RGBA8, GX_FALSE);
    GX_CopyTex(frame_tex, GX_TRUE);
    GX_SetCopyClear(BLACK, GX_MAX_Z24);
}

static void video_init(void)
{
    VIDEO_Init();
    mode = *VIDEO_GetPreferredMode(NULL);
    VIDEO_Configure(&mode);

    for (int i = 0; i < 2; i++) {
        xfb[i] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(&mode));
        VIDEO_ClearFrameBuffer(&mode, xfb[i], COLOR_BLACK);
    }
    VIDEO_SetNextFramebuffer(xfb[0]);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (mode.viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    void *fifo = memalign(32, FIFO_SIZE);
    memset(fifo, 0, FIFO_SIZE);
    GX_Init(fifo, FIFO_SIZE);

    u32 xfb_height = GX_SetDispCopyYScale(GX_GetYScaleFactor(mode.efbHeight, mode.xfbHeight));
    GX_SetDispCopySrc(0, 0, mode.fbWidth, mode.efbHeight);
    GX_SetDispCopyDst(mode.fbWidth, xfb_height);
    /* No vertical deflicker filter: it would blur the doubled lines. */
    GX_SetCopyFilter(mode.aa, mode.sample_pattern, GX_FALSE, mode.vfilter);
    GX_SetFieldMode(mode.field_rendering, mode.viHeight == 2 * mode.xfbHeight ? GX_ENABLE : GX_DISABLE);
    GX_SetDispCopyGamma(GX_GM_1_0);
    GX_SetCullMode(GX_CULL_NONE);

    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 8); /* 8.8 = MESH_ONE */
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetNumTevStages(1);

    frame_tex = memalign(32, GX_GetTexBufferSize(SCREEN_W, SCREEN_H, GX_TF_RGBA8, GX_FALSE, 0));
    quad_x = 0;
    quad_w = mode.fbWidth;
#ifdef HW_RVL
    /* Wii set to 16:9: the TV stretches the picture, so draw it 3/4 wide to stay 4:3. */
    if (CONF_GetAspectRatio() == CONF_ASPECT_16_9) {
        quad_w = mode.fbWidth * 3 / 4;
        quad_x = (mode.fbWidth - quad_w) / 2;
    }
#endif
    clear_scene_area();
}

static void draw_mesh(const mesh *m, Mtx mv)
{
    GX_LoadPosMtxImm(mv, GX_PNMTX0);
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, m->tris * 3);
    for (int t = 0; t < m->tris; t++) {
        for (int c = 0; c < 3; c++) {
            const int16_t *v = m->verts[t * 3 + c];
            GX_Position3s16(v[0], v[1], v[2]);
            GX_Color3u8(m->colors[t][0], m->colors[t][1], m->colors[t][2]);
        }
    }
    GX_End();
}

/* Pass 1: the 3D scene into the top-left 320x240 of the EFB. */
static void draw_scene(const world *w, const view *v)
{
    GX_SetViewport(0, 0, SCREEN_W, SCREEN_H, 0, 1);
    GX_SetScissor(0, 0, SCREEN_W, SCREEN_H);
    Mtx44 proj;
    guPerspective(proj, 53.0f, 4.0f / 3.0f, 0.2f, 100.0f);
    GX_LoadProjectionMtx(proj, GX_PERSPECTIVE);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);

    /* view = translate(0, 0, -dist) rotX(pitch) rotY(-yaw) translate(-x, -y, -z) */
    Mtx rx, ry, t, rot, view, model, mv;
    guMtxRotDeg(rx, 'x', v->pitch / 10.0f);
    guMtxRotDeg(ry, 'y', -v->yaw / 10.0f);
    guMtxConcat(rx, ry, rot);
    guMtxTrans(t, -v->x / 1000.0f, -v->y / 1000.0f, -v->z / 1000.0f);
    guMtxConcat(rot, t, view);
    guMtxTransApply(view, view, 0.0f, 0.0f, -v->dist / 1000.0f);
    draw_mesh(&level_mesh, view);

    /* player = translate(x, FLOOR_Y + y, z) rotY(heading) */
    for (int i = 0; i < w->player_count; i++) {
        const player *p = &w->players[i];
        guMtxRotDeg(ry, 'y', p->heading / 10.0f);
        guMtxTransApply(ry, model, p->x / 1000.0f, FLOOR_Y + p->y / 1000.0f, p->z / 1000.0f);
        guMtxConcat(view, model, mv);
        draw_mesh(&guy_mesh, mv);
    }
}

/* Pass 2: copy the 320x240 frame to a texture and draw it over the whole EFB with
 * nearest filtering (the VI and copy scalers can only filter). */
static void draw_upscaled(void)
{
    GX_SetTexCopySrc(0, 0, SCREEN_W, SCREEN_H);
    GX_SetTexCopyDst(SCREEN_W, SCREEN_H, GX_TF_RGBA8, GX_FALSE);
    GX_CopyTex(frame_tex, GX_TRUE); /* clear to black: it's under the bars when 16:9 */
    GX_PixModeSync();
    GX_InvalidateTexAll();

    GXTexObj tex;
    GX_InitTexObj(&tex, frame_tex, SCREEN_W, SCREEN_H, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&tex, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GX_LoadTexObj(&tex, GX_TEXMAP0);

    GX_SetViewport(0, 0, mode.fbWidth, mode.efbHeight, 0, 1);
    GX_SetScissor(0, 0, mode.fbWidth, mode.efbHeight);
    Mtx44 ortho;
    guOrtho(ortho, 0, mode.efbHeight, 0, mode.fbWidth, 0, 1);
    GX_LoadProjectionMtx(ortho, GX_ORTHOGRAPHIC);
    Mtx id;
    guMtxIdentity(id);
    GX_LoadPosMtxImm(id, GX_PNMTX0);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetNumChans(0);
    GX_SetNumTexGens(1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);

    f32 x0 = quad_x, x1 = quad_x + quad_w, y1 = mode.efbHeight;
    GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
    GX_Position3f32(x0, 0, -0.5f); GX_TexCoord2f32(0, 0);
    GX_Position3f32(x1, 0, -0.5f); GX_TexCoord2f32(1, 0);
    GX_Position3f32(x1, y1, -0.5f); GX_TexCoord2f32(1, 1);
    GX_Position3f32(x0, y1, -0.5f); GX_TexCoord2f32(0, 1);
    GX_End();
}

#ifdef HW_RVL
/* Power button on the console or a Wii Remote. Dolphin's Stop presses it too and
 * waits for the program to shut down. */
static volatile int power_pressed;
static void on_power(void) { power_pressed = 1; }
static void on_wiimote_power(s32 chan) { (void)chan; power_pressed = 1; }
#endif

#define STICK_DEADZONE 32

/* GC pad: stick moves, A jumps, C-stick or D-pad orbits, L/R zoom in/out.
 * Wii Remote: D-pad moves, A jumps, B + D-pad orbits, -/+ zoom. START / HOME sets *quit. */
static unsigned read_input(int *quit)
{
    PAD_ScanPads();
    u16 b = PAD_ButtonsHeld(0);
    s8 x = PAD_StickX(0), y = PAD_StickY(0), cx = PAD_SubStickX(0), cy = PAD_SubStickY(0);
    unsigned in = 0;
    if (x < -STICK_DEADZONE) in |= MOVE_LEFT;
    if (x > STICK_DEADZONE)  in |= MOVE_RIGHT;
    if (y > STICK_DEADZONE)  in |= MOVE_FWD;
    if (y < -STICK_DEADZONE) in |= MOVE_BACK;
    if ((b & PAD_BUTTON_LEFT) || cx < -STICK_DEADZONE)  in |= CAM_LEFT;
    if ((b & PAD_BUTTON_RIGHT) || cx > STICK_DEADZONE)  in |= CAM_RIGHT;
    if ((b & PAD_BUTTON_UP) || cy > STICK_DEADZONE)     in |= CAM_UP;
    if ((b & PAD_BUTTON_DOWN) || cy < -STICK_DEADZONE)  in |= CAM_DOWN;
    if (b & PAD_TRIGGER_L) in |= CAM_IN;
    if (b & PAD_TRIGGER_R) in |= CAM_OUT;
    if (b & PAD_BUTTON_A)  in |= JUMP;
    *quit = (PAD_ButtonsDown(0) & PAD_BUTTON_START) != 0;
#ifdef HW_RVL
    WPAD_ScanPads();
    u32 w = WPAD_ButtonsHeld(0);
    int orbit = (w & WPAD_BUTTON_B) != 0;
    if (w & WPAD_BUTTON_LEFT)  in |= orbit ? CAM_LEFT : MOVE_LEFT;
    if (w & WPAD_BUTTON_RIGHT) in |= orbit ? CAM_RIGHT : MOVE_RIGHT;
    if (w & WPAD_BUTTON_UP)    in |= orbit ? CAM_UP : MOVE_FWD;
    if (w & WPAD_BUTTON_DOWN)  in |= orbit ? CAM_DOWN : MOVE_BACK;
    if (w & WPAD_BUTTON_MINUS) in |= CAM_IN;
    if (w & WPAD_BUTTON_PLUS)  in |= CAM_OUT;
    if (w & WPAD_BUTTON_A)     in |= JUMP;
    if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) || power_pressed)
        *quit = 1;
#endif
    return in;
}

int main(void)
{
    video_init();
    PAD_Init();
#ifdef HW_RVL
    WPAD_Init();
    SYS_SetPowerCallback(on_power);
    WPAD_SetPowerButtonCallback(on_wiimote_power);
#endif

    int fb = 0, quit = 0;
    world w = world_init(1);
    camera cam = camera_init();
    while (!quit) {
        draw_scene(&w, &cam.now);
        draw_upscaled();

        GX_DrawDone();
        fb ^= 1;
        GX_SetColorUpdate(GX_TRUE);
        GX_CopyDisp(xfb[fb], GX_TRUE); /* clears the whole EFB to black */
        clear_scene_area();
        VIDEO_SetNextFramebuffer(xfb[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        unsigned b = read_input(&quit);
        input in[MAX_PLAYERS] = {{b, cam.now.yaw}};
        w = world_update(w, in);
        cam = camera_update(cam, b, &w.players[0]);
    }
#ifdef HW_RVL
    if (power_pressed)
        SYS_ResetSystem(SYS_POWEROFF, 0, 0);
#endif
    exit(0);
}
