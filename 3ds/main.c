#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include "cube.h"
#include "vshader_shbin.h"

#define DISPLAY_TRANSFER_FLAGS                                                   \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |                              \
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |                              \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define BG_VERTS 6
#define CUBE_VERTS (CUBE_FACES * 6)

typedef struct {
    float pos[3];
    float clr[3];
} vertex;

/* Background quad (clip space, drawn with identity matrices) then the cube as triangles. */
static vertex *build_vbo(void)
{
    static const float bg[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    static const int quad[6] = {0, 1, 2, 0, 2, 3};
    vertex *vbo = linearAlloc(sizeof(vertex) * (BG_VERTS + CUBE_VERTS));
    vertex *out = vbo;

    for (int i = 0; i < 6; i++)
        *out++ = (vertex){{bg[quad[i]][0], bg[quad[i]][1], -0.5f}, {0.1f, 0.1f, 0.15f}};
    for (int f = 0; f < CUBE_FACES; f++) {
        for (int i = 0; i < 6; i++) {
            const int8_t *v = cube_verts[f * 4 + quad[i]];
            *out++ = (vertex){{v[0], v[1], v[2]},
                              {cube_colors[f][0] / 255.0f, cube_colors[f][1] / 255.0f,
                               cube_colors[f][2] / 255.0f}};
        }
    }
    return vbo;
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);
    printf("hangout64\n\nPress START to exit.\n");

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

    int angle = 0;
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;

        C3D_Mtx mv;
        Mtx_Identity(&mv);
        Mtx_Translate(&mv, 0.0f, 0.0f, -5.0f, true);
        Mtx_RotateX(&mv, C3D_AngleFromDegrees(angle), true);
        Mtx_RotateY(&mv, C3D_AngleFromDegrees(angle), true);

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
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, u_mv, &mv);
        C3D_DrawArrays(GPU_TRIANGLES, BG_VERTS, CUBE_VERTS);
        C3D_FrameEnd(0);

        angle = cube_step(angle);
    }

    linearFree(vbo);
    shaderProgramFree(&prog);
    DVLB_Free(dvlb);
    C3D_RenderTargetDelete(top);
    C3D_Fini();
    gfxExit();
    return 0;
}
