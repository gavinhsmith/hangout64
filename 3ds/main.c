#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include "world.h"
#include "vshader_shbin.h"

#define DISPLAY_TRANSFER_FLAGS                                                   \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |                              \
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |                              \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define BG_VERTS 6

typedef struct {
    float pos[3];
    float clr[3];
} vertex;

static const int quad_tris[6] = {0, 1, 2, 0, 2, 3};

static vertex *add_mesh(vertex *out, const mesh *m)
{
    const float k = 1.0f / MESH_ONE;
    for (int t = 0; t < m->tris; t++) {
        for (int c = 0; c < 3; c++) {
            const int16_t *v = m->verts[t * 3 + c];
            *out++ = (vertex){{v[0] * k, v[1] * k, v[2] * k},
                              {m->colors[t][0] / 255.0f, m->colors[t][1] / 255.0f,
                               m->colors[t][2] / 255.0f}};
        }
    }
    return out;
}

/* Background quad (clip space, drawn with identity matrices), then the guy, then the
 * floor, all as triangles. Call after world_init() so the floor exists. */
static vertex *build_vbo(void)
{
    static const float bg[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    vertex *vbo = linearAlloc(sizeof(vertex) * (BG_VERTS + (guy_mesh.tris + floor_mesh.tris) * 3));
    vertex *out = vbo;
    for (int i = 0; i < 6; i++)
        *out++ = (vertex){{bg[quad_tris[i]][0], bg[quad_tris[i]][1], -0.5f}, {0.1f, 0.1f, 0.15f}};
    out = add_mesh(out, &guy_mesh);
    add_mesh(out, &floor_mesh);
    return vbo;
}

/* Circle pad moves, D-pad orbits, L/R zoom in/out. */
static unsigned read_input(u32 keys)
{
    unsigned in = 0;
    if (keys & KEY_CPAD_LEFT)  in |= MOVE_LEFT;
    if (keys & KEY_CPAD_RIGHT) in |= MOVE_RIGHT;
    if (keys & KEY_CPAD_UP)    in |= MOVE_FWD;
    if (keys & KEY_CPAD_DOWN)  in |= MOVE_BACK;
    if (keys & KEY_DLEFT)      in |= CAM_LEFT;
    if (keys & KEY_DRIGHT)     in |= CAM_RIGHT;
    if (keys & KEY_DUP)        in |= CAM_UP;
    if (keys & KEY_DDOWN)      in |= CAM_DOWN;
    if (keys & KEY_L)          in |= CAM_IN;
    if (keys & KEY_R)          in |= CAM_OUT;
    return in;
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);
    printf("hangout64\n\nCircle pad: move\nD-pad:      orbit\nL/R:        zoom\nSTART:      exit\n");

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    /* The top screen is physically rotated: its framebuffer is 240 wide, 400 tall. */
    C3D_RenderTarget *top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(top, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    DVLB_s *dvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_size);
    shaderProgram_s prog;
    shaderProgramInit(&prog);
    shaderProgramSetVsh(&prog, &dvlb->DVLE[0]);
    C3D_BindProgram(&prog);
    int u_proj = shaderInstanceGetUniformLocation(prog.vertexShader, "projection");
    int u_mv = shaderInstanceGetUniformLocation(prog.vertexShader, "modelView");

    C3D_AttrInfo *attr = C3D_GetAttrInfo();
    AttrInfo_Init(attr);
    AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 3);

    world w = world_init();
    vertex *vbo = build_vbo();
    C3D_BufInfo *buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, vbo, sizeof(vertex), 2, 0x10);

    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_CullFace(GPU_CULL_NONE);

    C3D_Mtx proj, identity;
    Mtx_PerspTilt(&proj, C3D_AngleFromDegrees(53.0f), 4.0f / 3.0f, 0.2f, 100.0f, false);
    Mtx_Identity(&identity);

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;
        w = world_update(w, read_input(hidKeysHeld()));

        /* view = translate(0, 0, -dist) rotX(pitch) rotY(-yaw) translate(-cam.x, 0, -cam.z) */
        C3D_Mtx view, guy;
        Mtx_Identity(&view);
        Mtx_Translate(&view, 0.0f, 0.0f, -w.cam.dist / 100.0f, true);
        Mtx_RotateX(&view, C3D_AngleFromDegrees(w.cam.pitch / 10.0f), true);
        Mtx_RotateY(&view, C3D_AngleFromDegrees(-w.cam.yaw / 10.0f), true);
        Mtx_Translate(&view, -w.cam.x / 100.0f, 0.0f, -w.cam.z / 100.0f, true);
        Mtx_Copy(&guy, &view);
        Mtx_Translate(&guy, w.x / 100.0f, (float)FLOOR_Y, w.z / 100.0f, true);
        Mtx_RotateY(&guy, C3D_AngleFromDegrees(w.heading / 10.0f), true);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_RenderTargetClear(top, C3D_CLEAR_ALL, 0x000000FF, 0);
        C3D_FrameDrawOn(top);
        /* Pillarbox: the middle 320 of the 400 columns (y in the rotated framebuffer). */
        C3D_SetViewport(0, (400 - SCREEN_W) / 2, SCREEN_H, SCREEN_W);

        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, u_proj, &identity);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, u_mv, &identity);
        C3D_DrawArrays(GPU_TRIANGLES, 0, BG_VERTS);

        C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, u_proj, &proj);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, u_mv, &guy);
        C3D_DrawArrays(GPU_TRIANGLES, BG_VERTS, guy_mesh.tris * 3);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, u_mv, &view);
        C3D_DrawArrays(GPU_TRIANGLES, BG_VERTS + guy_mesh.tris * 3, floor_mesh.tris * 3);
        C3D_FrameEnd(0);
    }

    linearFree(vbo);
    shaderProgramFree(&prog);
    DVLB_Free(dvlb);
    C3D_RenderTargetDelete(top);
    C3D_Fini();
    gfxExit();
    return 0;
}
