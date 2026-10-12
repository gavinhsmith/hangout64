#include <gccore.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif
#include "cube.h"

#define FIFO_SIZE (256 * 1024)

static GXRModeObj mode;
static void *xfb[2];

static void video_init(void)
{
    VIDEO_Init();
    mode = *VIDEO_GetPreferredMode(NULL);
    /* Render 320x240; the EFB copy doubles it vertically, the VI horizontally. */
    mode.fbWidth = SCREEN_W;
    mode.efbHeight = SCREEN_H;
#ifdef HW_RVL
    /* Wii set to 16:9: the TV stretches the picture, so narrow it 3/4 to stay 4:3. */
    if (CONF_GetAspectRatio() == CONF_ASPECT_16_9) {
        mode.viWidth = 480;
        mode.viXOrigin = (VI_MAX_WIDTH_NTSC - mode.viWidth) / 2;
    }
#endif
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

    GX_SetCopyClear((GXColor){25, 25, 38, 255}, GX_MAX_Z24);
    GX_SetViewport(0, 0, mode.fbWidth, mode.efbHeight, 0, 1);
    u32 xfb_height = GX_SetDispCopyYScale(GX_GetYScaleFactor(mode.efbHeight, mode.xfbHeight));
    GX_SetScissor(0, 0, mode.fbWidth, mode.efbHeight);
    GX_SetDispCopySrc(0, 0, mode.fbWidth, mode.efbHeight);
    GX_SetDispCopyDst(mode.fbWidth, xfb_height);
    GX_SetCopyFilter(mode.aa, mode.sample_pattern, GX_TRUE, mode.vfilter);
    GX_SetFieldMode(mode.field_rendering, mode.viHeight == 2 * mode.xfbHeight ? GX_ENABLE : GX_DISABLE);
    GX_SetDispCopyGamma(GX_GM_1_0);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);

    /* Untextured, unlit, per-vertex colour. */
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(0);
    GX_SetNumTevStages(1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

    Mtx44 proj;
    guPerspective(proj, 53.0f, 4.0f / 3.0f, 0.2f, 100.0f);
    GX_LoadProjectionMtx(proj, GX_PERSPECTIVE);
}

#define STICK_DEADZONE 32

/* GC pad: stick or D-pad orbits, L/R zoom in/out. Wii Remote: D-pad orbits, -/+ zoom.
 * START / HOME sets *quit. */
static unsigned read_input(int *quit)
{
    PAD_ScanPads();
    u16 b = PAD_ButtonsHeld(0);
    s8 x = PAD_StickX(0), y = PAD_StickY(0);
    unsigned cam = 0;
    if ((b & PAD_BUTTON_LEFT) || x < -STICK_DEADZONE)  cam |= CAM_LEFT;
    if ((b & PAD_BUTTON_RIGHT) || x > STICK_DEADZONE)  cam |= CAM_RIGHT;
    if ((b & PAD_BUTTON_UP) || y > STICK_DEADZONE)     cam |= CAM_UP;
    if ((b & PAD_BUTTON_DOWN) || y < -STICK_DEADZONE)  cam |= CAM_DOWN;
    if (b & PAD_TRIGGER_L)                             cam |= CAM_IN;
    if (b & PAD_TRIGGER_R)                             cam |= CAM_OUT;
    *quit = (PAD_ButtonsDown(0) & PAD_BUTTON_START) != 0;
#ifdef HW_RVL
    WPAD_ScanPads();
    u32 w = WPAD_ButtonsHeld(0);
    if (w & WPAD_BUTTON_LEFT)  cam |= CAM_LEFT;
    if (w & WPAD_BUTTON_RIGHT) cam |= CAM_RIGHT;
    if (w & WPAD_BUTTON_UP)    cam |= CAM_UP;
    if (w & WPAD_BUTTON_DOWN)  cam |= CAM_DOWN;
    if (w & WPAD_BUTTON_MINUS) cam |= CAM_IN;
    if (w & WPAD_BUTTON_PLUS)  cam |= CAM_OUT;
    if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
        *quit = 1;
#endif
    return cam;
}

int main(void)
{
    video_init();
    PAD_Init();
#ifdef HW_RVL
    WPAD_Init();
#endif

    int angle = 0, fb = 0, quit = 0;
    camera cam = CAMERA_INIT;
    while (!quit) {
        Mtx rx, ry, model, view, mv;
        guMtxRotDeg(rx, 'x', (f32)angle);
        guMtxRotDeg(ry, 'y', (f32)angle);
        guMtxConcat(rx, ry, model);
        guMtxRotDeg(rx, 'x', (f32)cam.pitch);
        guMtxRotDeg(ry, 'y', (f32)-cam.yaw);
        guMtxConcat(rx, ry, view);
        guMtxConcat(view, model, mv);
        guMtxTransApply(mv, mv, 0.0f, 0.0f, -cam.dist / 10.0f);
        GX_LoadPosMtxImm(mv, GX_PNMTX0);

        GX_Begin(GX_QUADS, GX_VTXFMT0, CUBE_FACES * 4);
        for (int f = 0; f < CUBE_FACES; f++) {
            for (int c = 0; c < 4; c++) {
                const int8_t *v = cube_verts[f * 4 + c];
                GX_Position3s8(v[0], v[1], v[2]);
                GX_Color3u8(cube_colors[f][0], cube_colors[f][1], cube_colors[f][2]);
            }
        }
        GX_End();

        GX_DrawDone();
        fb ^= 1;
        GX_SetColorUpdate(GX_TRUE);
        GX_CopyDisp(xfb[fb], GX_TRUE);
        VIDEO_SetNextFramebuffer(xfb[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        angle = cube_step(angle);
        cam = camera_update(cam, read_input(&quit));
    }
    exit(0);
}
