/*
 * Graphics Synthesizer, GPU back end (SDL3 GPU API: Vulkan on Linux and Android, Direct3D 12 / Metal elsewhere).
 *
 * gs_core.c still decodes what the game sends and keeps GS memory; this file takes over at "a primitive with the
 * current GS registers": it records every primitive of the frame, and at the end of the frame uploads vertices
 * and new textures and replays the list on the GPU.
 *   render targets   one per GS frame-buffer address in use, GS_W x GS_H GS pixels at SCALE times the PS2's
 *                    resolution, each with its own depth buffer
 *   textures         GS memory decoded to ordinary RGBA textures, cached by TEX0 / TEXA and the upload
 *                    generation of the pages they occupy; a frame buffer used as a texture is sampled directly
 *   blending         the GS equation ((A - B) * C >> 7) + D mapped onto blend factors
 *   alpha            1.0 = 0x80, so that "source alpha" blending needs no extra scaling
 * Not handled yet: a target sampled while it is being drawn to, region clamp / repeat, destination-alpha tests,
 * frame-buffer masks, 16-bit targets' precision.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gs_internal.h"
#include "shaders.h" /* generated: kGsVertSpv, kGsFragSpv */

#define SCALE 2
#define GS_W 1024
#define GS_H 1024
#define MAX_VERTS (1 << 20)
#define MAX_DRAWS (1 << 16)

typedef struct Vtx {
    float x, y, z;
    uint8_t r, g, b, a;
    float s, t, q;
} Vtx;

typedef struct Target {
    uint32_t fbp;
    SDL_GPUTexture *color, *depth;
    SDL_GPUTexture *aux; /* the GS alpha byte, exact (R8): the colour texture keeps alpha rescaled for blending */
    int cleared;
    unsigned draws; /* this frame */
    uint32_t gen;   /* upload generation of its first page when it was last drawn to */
} Target;

typedef struct Tex {
    uint64_t tex0, texa;
    uint32_t gen;
    SDL_GPUTexture *tex;
    unsigned last; /* frame last used */
} Tex;

typedef struct Draw {
    int native; /* 0: primitives; otherwise a native full-screen effect (PORT_FX_*) at this point of the frame */
    uint32_t first, count;
    int target;
    SDL_GPUTexture *tex;
    int sampler;
    int pipeline;
    int32_t mode[4];
    float misc[4];
    float blendc;
    SDL_Rect scissor;
} Draw;

typedef struct Pipe {
    uint32_t key;
    SDL_GPUGraphicsPipeline *p;
} Pipe;

static SDL_Window *sWindow;
static SDL_GPUDevice *sDev;
static SDL_GPUShader *sVs, *sFs;
static SDL_GPUBuffer *sVbuf;
static SDL_GPUTransferBuffer *sVxfer;
static SDL_GPUSampler *sSamplers[8]; /* bit 0: linear, bit 1: clamp U, bit 2: clamp V */
static SDL_GPUTexture *sWhite;
static SDL_GPUGraphicsPipeline *sOutlinePipe;
static Target sTargets[16];
static int sTargetCount;
static Tex sTex[2048];
static int sTexCount;
static Tex *sLast; /* the entry the previous lookup returned */
static Pipe sPipes[1024];
static int sPipeCount;
static Vtx *sVerts;
static uint32_t sVertCount;
static Draw *sDraws;
static uint32_t sDrawCount;
static uint64_t sNextFrameNs;
static unsigned sNative; /* native effect markers seen this frame */
static unsigned sSkipped; /* primitives of PS2-only passes dropped this frame */

/* textures created this frame, to upload in the copy pass */
static struct { SDL_GPUTexture *tex; uint32_t w, h; uint32_t *px; } sPending[512];
static int sPendingCount;

static SDL_GPUShader *shader(const unsigned char *code, size_t size, SDL_GPUShaderStage stage, int samplers, int ubos) {
    SDL_GPUShaderCreateInfo ci;
    SDL_zero(ci);
    ci.code = code;
    ci.code_size = size;
    ci.entrypoint = "main";
    ci.format = SDL_GPU_SHADERFORMAT_SPIRV;
    ci.stage = stage;
    ci.num_samplers = (Uint32)samplers;
    ci.num_uniform_buffers = (Uint32)ubos;
    return SDL_CreateGPUShader(sDev, &ci);
}

int GsGpu_Init(void) {
    SDL_GPUBufferCreateInfo bi;
    SDL_GPUTransferBufferCreateInfo ti;
    static uint32_t white = 0xFFFFFFFFu;
    int i;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "bt3: SDL_Init: %s\n", SDL_GetError());
        return 0;
    }
    sDev = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, getenv("BT3_GPU_DEBUG") != NULL, NULL);
    sWindow = sDev ? SDL_CreateWindow("Budokai Tenkaichi 3 (port)", 512 * SCALE, 448 * SCALE, SDL_WINDOW_RESIZABLE) : NULL;
    if (sWindow == NULL || !SDL_ClaimWindowForGPUDevice(sDev, sWindow)) {
        fprintf(stderr, "bt3: no GPU window: %s\n", SDL_GetError());
        return 0;
    }
    SDL_SetGPUSwapchainParameters(sDev, sWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    sVs = shader(kGsVertSpv, sizeof(kGsVertSpv), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    sFs = shader(kGsFragSpv, sizeof(kGsFragSpv), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    if (sVs == NULL || sFs == NULL) {
        fprintf(stderr, "bt3: shaders: %s\n", SDL_GetError());
        return 0;
    }
    SDL_zero(bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bi.size = MAX_VERTS * sizeof(Vtx);
    sVbuf = SDL_CreateGPUBuffer(sDev, &bi);
    SDL_zero(ti);
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    ti.size = MAX_VERTS * sizeof(Vtx);
    sVxfer = SDL_CreateGPUTransferBuffer(sDev, &ti);
    for (i = 0; i < 8; i++) {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter = si.mag_filter = (i & 1) ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        si.address_mode_u = (i & 2) ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_v = (i & 4) ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sSamplers[i] = SDL_CreateGPUSampler(sDev, &si);
    }
    sVerts = malloc(MAX_VERTS * sizeof(Vtx));
    sDraws = malloc(MAX_DRAWS * sizeof(Draw));
    {
        SDL_GPUTextureCreateInfo ci;
        SDL_zero(ci);
        ci.type = SDL_GPU_TEXTURETYPE_2D;
        ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        ci.width = ci.height = 1;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        sWhite = SDL_CreateGPUTexture(sDev, &ci);
        sPending[sPendingCount].tex = sWhite;
        sPending[sPendingCount].w = sPending[sPendingCount].h = 1;
        sPending[sPendingCount].px = malloc(4);
        memcpy(sPending[sPendingCount].px, &white, 4);
        sPendingCount++;
    }
    {   /* native outline: destination minus source on the colour channels */
        SDL_GPUShader *vs = shader(kFxVertSpv, sizeof(kFxVertSpv), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
        SDL_GPUShader *fs = shader(kOutlineFragSpv, sizeof(kOutlineFragSpv), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
        SDL_GPUColorTargetDescription cd;
        SDL_GPUGraphicsPipelineCreateInfo ci;
        SDL_zero(cd);
        SDL_zero(ci);
        cd.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        cd.blend_state.enable_blend = true;
        cd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        cd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        cd.blend_state.color_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
        cd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        cd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        cd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        cd.blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B;
        cd.blend_state.enable_color_write_mask = true;
        ci.vertex_shader = vs;
        ci.fragment_shader = fs;
        ci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        ci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        ci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        ci.target_info.color_target_descriptions = &cd;
        ci.target_info.num_color_targets = 1;
        sOutlinePipe = vs && fs ? SDL_CreateGPUGraphicsPipeline(sDev, &ci) : NULL;
        if (sOutlinePipe == NULL) {
            fprintf(stderr, "bt3: outline pipeline: %s\n", SDL_GetError());
            return 0;
        }
    }
    fprintf(stderr, "bt3: GPU renderer: %s\n", SDL_GetGPUDeviceDriver(sDev));
    return 1;
}

static int target_get(uint32_t fbp, int create) {
    SDL_GPUTextureCreateInfo ci;
    int i;

    for (i = 0; i < sTargetCount; i++) {
        if (sTargets[i].fbp == fbp) {
            return i;
        }
    }
    if (!create || sTargetCount == 16) {
        return -1;
    }
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = GS_W * SCALE;
    ci.height = GS_H * SCALE;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    sTargets[sTargetCount].color = SDL_CreateGPUTexture(sDev, &ci);
    ci.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    sTargets[sTargetCount].aux = SDL_CreateGPUTexture(sDev, &ci);
    ci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    sTargets[sTargetCount].depth = SDL_CreateGPUTexture(sDev, &ci);
    sTargets[sTargetCount].fbp = fbp;
    sTargets[sTargetCount].cleared = 0;
    sTargets[sTargetCount].draws = 0;
    return sTargetCount++;
}

/* GS memory -> an RGBA texture for the current TEX0 (alpha rescaled so that 0x80 is 1.0). */
static SDL_GPUTexture *texture_get(int ctx) {
    uint64_t t0 = gGs.tex0[ctx] & 0x1FFFFFFFFFFFFFFFull, texa = gGs.texa;
    uint32_t tbp = t0 & 0x3FFF, tbw = (t0 >> 14) & 0x3F, psm = (t0 >> 20) & 0x3F, tw = 1u << ((t0 >> 26) & 15), th = 1u << ((t0 >> 30) & 15);
    uint32_t cbp = (t0 >> 37) & 0x3FFF, cpsm = (t0 >> 51) & 15;
    uint32_t bits = (uint32_t)Gs_PsmBits(psm), gen = 0, pages, i, x, y, *px;
    SDL_GPUTextureCreateInfo ci;
    Tex *t;

    if (tw > 1024) { tw = 1024; }
    if (th > 1024) { th = 1024; }
    pages = ((tbw ? tbw : 1) * 64 * th * (bits == 24 ? 32 : bits) / 8 + 8191) / 8192;
    /* `gen` identifies the CONTENT: a hash over the pages the texture and its palette occupy */
    for (i = 0; i <= pages; i++) {
        gen = (gen ^ Gs_PageHash(tbp / 32 + i)) * 16777619u + i;
    }
    if (bits <= 8) {
        gen = (gen ^ Gs_PageHash(cbp / 32)) * 16777619u;
        gen = (gen ^ Gs_PageHash(cbp / 32 + 1)) * 16777619u;
    }
    if (sLast != NULL && sLast->tex0 == t0 && sLast->texa == texa && sLast->gen == gen) {
        sLast->last = gGsFrame;
        return sLast->tex; /* the common case: the same texture as the previous primitive */
    }
    for (i = 0; i < (uint32_t)sTexCount; i++) {
        if (sTex[i].tex0 == t0 && sTex[i].texa == texa && sTex[i].gen == gen) {
            sTex[i].last = gGsFrame;
            sLast = &sTex[i];
            return sTex[i].tex;
        }
    }
    if (sPendingCount == 512) {
        return sWhite;
    }
    if (sTexCount == 2048) { /* evict the least recently used */
        int old = 0;
        for (i = 1; i < 2048; i++) {
            if (sTex[i].last < sTex[old].last) {
                old = (int)i;
            }
        }
        if (sTex[old].last == gGsFrame) {
            return sWhite; /* everything in the cache is in use by this frame's draw list: cannot evict */
        }
        SDL_ReleaseGPUTexture(sDev, sTex[old].tex);
        sTex[old] = sTex[--sTexCount];
        sLast = NULL;
    }
    px = malloc((size_t)tw * th * 4);
    for (y = 0; y < th; y++) {
        for (x = 0; x < tw; x++) {
            uint32_t c = Gs_VramRead(tbp, tbw, psm, x, y), a;
            if (bits > 8) {
                c = Gs_Expand(c, psm);
            } else if (bits == 8) {
                c = (c & 0xE7) | ((c & 8) << 1) | ((c & 0x10) >> 1);
                c = Gs_Expand(Gs_VramRead(cbp, 1, cpsm, c & 15, c >> 4), cpsm);
            } else {
                c = Gs_Expand(Gs_VramRead(cbp, 1, cpsm, c & 7, c >> 3), cpsm);
            }
            a = (c >> 24) * 255 / 128;
            px[y * tw + x] = (c & 0xFFFFFF) | (a > 255 ? 255 : a) << 24;
        }
    }
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = tw;
    ci.height = th;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    t = &sTex[sTexCount++];
    sLast = t;
    t->tex0 = t0;
    t->texa = texa;
    t->gen = gen;
    t->last = gGsFrame;
    t->tex = SDL_CreateGPUTexture(sDev, &ci);
    sPending[sPendingCount].tex = t->tex;
    sPending[sPendingCount].w = tw;
    sPending[sPendingCount].h = th;
    sPending[sPendingCount].px = px;
    sPendingCount++;
    return t->tex;
}

/* ((A - B) * C >> 7) + D as blend factors. Each of A, B, D is source, destination or zero, so the result is
   ks * Cs + kd * Cd with ks, kd from {0, 1, C, -C, 1 - C, 1 + C}. */
static void blend_of(uint64_t alpha, int abe, SDL_GPUColorTargetBlendState *b, uint32_t *key) {
    int A = alpha & 3, B = (alpha >> 2) & 3, C = (alpha >> 4) & 3, D = (alpha >> 6) & 3, k[2], i;
    SDL_GPUBlendFactor fc = C == 0 ? SDL_GPU_BLENDFACTOR_SRC_ALPHA : C == 1 ? SDL_GPU_BLENDFACTOR_DST_ALPHA : SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
    SDL_GPUBlendFactor fi = C == 0 ? SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA : C == 1 ? SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA : SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
    SDL_GPUBlendFactor f[2];
    int neg[2];

    SDL_zerop(b);
    b->color_write_mask = 0xF;
    b->enable_color_write_mask = false;
    *key = 0;
    if (!abe) {
        return;
    }
    /* k[i]: 0 nothing, 1 one, 2 C, 3 -C, 4 1 - C, 5 1 + C; i = 0 source, 1 destination */
    for (i = 0; i < 2; i++) {
        int c = (A == i) - (B == i), one = D == i;
        k[i] = c == 0 ? one : c > 0 ? (one ? 5 : 2) : (one ? 4 : 3);
        neg[i] = k[i] == 3;
        f[i] = k[i] == 0 ? SDL_GPU_BLENDFACTOR_ZERO : k[i] == 1 || k[i] == 5 ? SDL_GPU_BLENDFACTOR_ONE : k[i] == 4 ? fi : fc;
    }
    b->enable_blend = true;
    b->src_color_blendfactor = f[0];
    b->dst_color_blendfactor = f[1];
    b->color_blend_op = neg[0] ? SDL_GPU_BLENDOP_REVERSE_SUBTRACT : neg[1] ? SDL_GPU_BLENDOP_SUBTRACT : SDL_GPU_BLENDOP_ADD;
    b->src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE; /* the frame buffer keeps the source alpha */
    b->dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    b->alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    *key = 1u | (uint32_t)k[0] << 1 | (uint32_t)k[1] << 4 | (uint32_t)C << 7;
}

static int pipeline_get(int ctx, int topo) {
    uint64_t test = gGs.test[ctx], zb = gGs.zbuf[ctx];
    int zte = (test >> 16) & 1, ztst = (test >> 17) & 3, zwrite = !((zb >> 32) & 1);
    SDL_GPUColorTargetDescription cd, cds[2];
    SDL_GPUGraphicsPipelineCreateInfo ci;
    SDL_GPUVertexBufferDescription vb;
    SDL_GPUVertexAttribute at[3];
    uint32_t bkey, key, wmask;
    int i;

    SDL_zero(cd);
    cd.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    blend_of(gGs.alpha[ctx], (int)((gGs.prim >> 6) & 1), &cd.blend_state, &bkey);
    if (!zte) {
        ztst = 1;
    }
    /* FRAME.FBMSK, in the whole-channel forms ordinary drawing uses (alpha only, everything but alpha, ...):
       a channel whose eight mask bits are all set is not written. Partial masks are not representable. */
    {
        uint32_t m = (uint32_t)(gGs.frame[ctx] >> 32);
        wmask = ((m & 0xFF) != 0xFF ? SDL_GPU_COLORCOMPONENT_R : 0) | ((m & 0xFF00) != 0xFF00 ? SDL_GPU_COLORCOMPONENT_G : 0) |
                ((m & 0xFF0000) != 0xFF0000 ? SDL_GPU_COLORCOMPONENT_B : 0) | ((m & 0xFF000000u) != 0xFF000000u ? SDL_GPU_COLORCOMPONENT_A : 0);
        cd.blend_state.color_write_mask = (SDL_GPUColorComponentFlags)wmask;
        cd.blend_state.enable_color_write_mask = true;
    }
    key = bkey | (uint32_t)ztst << 10 | (uint32_t)zwrite << 12 | (uint32_t)topo << 13 | wmask << 16;
    for (i = 0; i < sPipeCount; i++) {
        if (sPipes[i].key == key) {
            return i;
        }
    }
    if (sPipeCount == 1024) {
        return 0;
    }
    SDL_zero(ci);
    SDL_zero(vb);
    SDL_zero(at);
    vb.slot = 0;
    vb.pitch = sizeof(Vtx);
    vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    at[0].location = 0; at[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; at[0].offset = 0;
    at[1].location = 1; at[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM; at[1].offset = 12;
    at[2].location = 2; at[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; at[2].offset = 16;
    ci.vertex_shader = sVs;
    ci.fragment_shader = sFs;
    ci.vertex_input_state.vertex_buffer_descriptions = &vb;
    ci.vertex_input_state.num_vertex_buffers = 1;
    ci.vertex_input_state.vertex_attributes = at;
    ci.vertex_input_state.num_vertex_attributes = 3;
    ci.primitive_type = topo == 0 ? SDL_GPU_PRIMITIVETYPE_TRIANGLELIST : topo == 1 ? SDL_GPU_PRIMITIVETYPE_LINELIST : SDL_GPU_PRIMITIVETYPE_POINTLIST;
    ci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    ci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    ci.depth_stencil_state.enable_depth_test = true;
    ci.depth_stencil_state.enable_depth_write = zwrite;
    ci.depth_stencil_state.compare_op = ztst == 0 ? SDL_GPU_COMPAREOP_NEVER : ztst == 1 ? SDL_GPU_COMPAREOP_ALWAYS :
                                        ztst == 2 ? SDL_GPU_COMPAREOP_GREATER_OR_EQUAL : SDL_GPU_COMPAREOP_GREATER;
    cds[0] = cd;
    SDL_zero(cds[1]); /* the exact alpha byte: never blended, written whenever the frame's alpha is writable */
    cds[1].format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    cds[1].blend_state.color_write_mask = (wmask & SDL_GPU_COLORCOMPONENT_A) ? SDL_GPU_COLORCOMPONENT_R : 0;
    cds[1].blend_state.enable_color_write_mask = true;
    ci.target_info.color_target_descriptions = cds;
    ci.target_info.num_color_targets = 2;
    ci.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    ci.target_info.has_depth_stencil_target = true;
    sPipes[sPipeCount].key = key;
    sPipes[sPipeCount].p = SDL_CreateGPUGraphicsPipeline(sDev, &ci);
    if (sPipes[sPipeCount].p == NULL) {
        fprintf(stderr, "bt3: pipeline: %s\n", SDL_GetError());
        return 0;
    }
    return sPipeCount++;
}

static void put(const GsVertex *v, float x, float y, float s, float t, float q, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float zmax) {
    Vtx *o = &sVerts[sVertCount++];
    o->x = x;
    o->y = y;
    o->z = (float)((double)v->z / zmax);
    o->r = r; o->g = g; o->b = b; o->a = a;
    o->s = s; o->t = t; o->q = q;
}

void GsGpu_Draw(int type, int ctx, const GsVertex *v) {
    uint64_t prim = gGs.prim, t0 = gGs.tex0[ctx], test = gGs.test[ctx], cl = gGs.clamp[ctx], sc = gGs.scissor[ctx];
    int tme = (prim >> 4) & 1, fst = (prim >> 8) & 1, gouraud = (prim >> 3) & 1, n = type == 6 ? 2 : type == 3 ? 3 : type == 1 ? 2 : 1;
    float tw = (float)(1u << ((t0 >> 26) & 15)), th = (float)(1u << ((t0 >> 30) & 15)), us = 1.0f, vs = 1.0f;
    float zmax = ((gGs.zbuf[ctx] >> 24) & 15) == 0 ? 4294967295.0f : ((gGs.zbuf[ctx] >> 24) & 15) == 1 ? 16777215.0f : 65535.0f;
    float s[3], t[3], q[3];
    const GsVertex *flat = &v[n - 1];
    Draw d, *last;
    int i, src;

    if (sVertCount + 6 > MAX_VERTS || sDrawCount == MAX_DRAWS) {
        return;
    }
    /* Passes that only make sense on the PS2's memory layout are not drawn here: they belong to full-screen
       effects that get native versions (see docs/port/README.md). Dropped:
         - drawing through a 16-bit view of the frame buffer (the "channel shuffle" between halves of a pixel),
         - drawing INTO the depth buffer's memory as if it were a picture,
         - sampling the depth buffer's memory, or the top byte of a buffer as an 8-bit index (PSMT8H / T4HL / T4HH). */
    {
        uint32_t fpsm = (uint32_t)((gGs.frame[ctx] >> 24) & 0x3F), fbp = (uint32_t)(gGs.frame[ctx] & 0x1FF), zbp = (uint32_t)(gGs.zbuf[ctx] & 0x1FF);
        uint32_t tpsm = (uint32_t)((t0 >> 20) & 0x3F), tbp = (uint32_t)(t0 & 0x3FFF);
        if (Gs_PsmBits(fpsm) == 16 || fbp == zbp) {
            sSkipped++;
            return;
        }
        if (tme && ((tpsm & 0x30) == 0x30 || tbp / 32 == zbp || tpsm == 0x1B || tpsm == 0x24 || tpsm == 0x2C)) {
            sSkipped++;
            return;
        }
    }
    memset(&d, 0, sizeof(d));
    d.target = target_get((uint32_t)(gGs.frame[ctx] & 0x1FF), 1);
    if (d.target < 0) {
        return;
    }
    d.tex = sWhite;
    if (tme) {
        src = target_get((uint32_t)((t0 & 0x3FFF) / 32), 0);
        /* a frame buffer used as a texture: only if nothing was uploaded over it since it was drawn */
        if (src >= 0 && (t0 & 0x1F) == 0 && sTargets[src].cleared && sTargets[src].gen == gGsPageGen[sTargets[src].fbp & 511]) {
            if (src == d.target || type == 6) {
                /* A sprite that copies one frame buffer into another (or into itself) is a step of a full-screen
                   effect (glare's shrink / blur / add chain, blur feedback): those get native versions. Triangles
                   textured with a buffer are kept: that is how the shadow is projected onto the ground. */
                sSkipped++;
                return;
            }
            d.tex = sTargets[src].color; /* a frame buffer used as a texture */
            us = tw / (float)GS_W;
            vs = th / (float)GS_H;
        } else {
            d.tex = texture_get(ctx);
        }
    }
    for (i = 0; i < n; i++) {
        if (fst) {
            s[i] = (float)v[i].u / 16.0f / tw * us;
            t[i] = (float)v[i].v / 16.0f / th * vs;
            q[i] = 1.0f;
        } else {
            s[i] = v[i].s * us;
            t[i] = v[i].t * vs;
            q[i] = v[i].q != 0.0f ? v[i].q : 1.0f;
        }
    }
    d.sampler = (int)((gGs.tex1[ctx] >> 5) & 1) | ((cl & 3) ? 2 : 0) | (((cl >> 2) & 3) ? 4 : 0);
    d.pipeline = pipeline_get(ctx, type == 1 ? 1 : type == 0 ? 2 : 0);
    d.mode[0] = tme;
    d.mode[1] = (int32_t)((t0 >> 35) & 3);
    d.mode[2] = (int32_t)((t0 >> 34) & 1);
    d.mode[3] = (test & 1) && ((test >> 12) & 3) == 0 ? (int32_t)((test >> 1) & 7) + 1 : 0;
    d.misc[0] = (float)((test >> 4) & 0xFF);
    d.blendc = (float)((gGs.alpha[ctx] >> 32) & 0xFF) / 128.0f;
    d.scissor.x = (int)(sc & 0x7FF) * SCALE;
    d.scissor.y = (int)((sc >> 32) & 0x7FF) * SCALE;
    d.scissor.w = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d.scissor.x;
    d.scissor.h = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d.scissor.y;
    d.first = sVertCount;
    if (type == 6) { /* sprite: two corners, flat colour and depth of the second vertex */
        const GsVertex *a = &v[0], *b = &v[1];
        put(b, a->x, a->y, s[0], t[0], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, b->x, a->y, s[1], t[0], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, a->x, b->y, s[0], t[1], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, b->x, a->y, s[1], t[0], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, b->x, b->y, s[1], t[1], 1.0f, b->r, b->g, b->b, b->a, zmax);
        put(b, a->x, b->y, s[0], t[1], 1.0f, b->r, b->g, b->b, b->a, zmax);
        if (!fst) { /* a sprite's texture coordinates are not divided by Q per pixel: do it here */
            for (i = 0; i < 6; i++) {
                sVerts[d.first + i].s /= q[1];
                sVerts[d.first + i].t /= q[1];
            }
        }
    } else {
        for (i = 0; i < n; i++) {
            const GsVertex *c = gouraud ? &v[i] : flat;
            put(&v[i], v[i].x, v[i].y, s[i], t[i], q[i], c->r, c->g, c->b, c->a, zmax);
        }
    }
    d.count = sVertCount - d.first;
    sTargets[d.target].draws++;
    sTargets[d.target].gen = gGsPageGen[sTargets[d.target].fbp & 511];
    last = sDrawCount ? &sDraws[sDrawCount - 1] : NULL;
    if (last != NULL && !last->native && last->target == d.target && last->tex == d.tex && last->sampler == d.sampler && last->pipeline == d.pipeline &&
        memcmp(last->mode, d.mode, sizeof(d.mode)) == 0 && last->misc[0] == d.misc[0] && last->blendc == d.blendc &&
        memcmp(&last->scissor, &d.scissor, sizeof(SDL_Rect)) == 0) {
        last->count += d.count; /* same state as the previous primitive: one draw call */
    } else {
        sDraws[sDrawCount++] = d;
    }
}

/* A marker from the game's display list (port/src/gs_marker.c): draw the native version of an effect here, on
   the frame buffer the current context draws to. 1 = the outline. */
void GsGpu_Native(int effect) {
    uint64_t sc = gGs.scissor[0];
    Draw d;

    if (effect != 1 || sDrawCount == MAX_DRAWS) {
        return;
    }
    sNative++;
    memset(&d, 0, sizeof(d));
    d.native = effect;
    d.target = target_get((uint32_t)(gGs.frame[0] & 0x1FF), 0);
    if (d.target < 0) {
        return;
    }
    d.scissor.x = (int)(sc & 0x7FF) * SCALE;
    d.scissor.y = (int)((sc >> 32) & 0x7FF) * SCALE;
    d.scissor.w = ((int)((sc >> 16) & 0x7FF) + 1) * SCALE - d.scissor.x;
    d.scissor.h = ((int)((sc >> 48) & 0x7FF) + 1) * SCALE - d.scissor.y;
    sDraws[sDrawCount++] = d;
}

void GsGpu_FrameEnd(void) {
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass *copy;
    SDL_GPURenderPass *pass = NULL;
    SDL_GPUTexture *swap = NULL;
    SDL_Event ev;
    Uint32 sw = 0, sh = 0;
    int cur = -1, best = -1, i;
    uint32_t n;

    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT || (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE)) {
            exit(0);
        }
    }
    if (getenv("BT3_GS_VERBOSE") != NULL && gGsFrame % 30 == 0) {
        fprintf(stderr, "gpu: frame %u: %u draws, %u vertices, %d targets, %d textures, %d pipelines, %u primitives of PS2-only passes dropped, %u native effects\n",
                gGsFrame, sDrawCount, sVertCount, sTargetCount, sTexCount, sPipeCount, sSkipped, sNative);
    }
    cmd = SDL_AcquireGPUCommandBuffer(sDev);
    /* uploads: this frame's vertices and the textures decoded for it */
    copy = SDL_BeginGPUCopyPass(cmd);
    if (sVertCount != 0) {
        SDL_GPUTransferBufferLocation src;
        SDL_GPUBufferRegion dst;
        void *p = SDL_MapGPUTransferBuffer(sDev, sVxfer, true);
        memcpy(p, sVerts, sVertCount * sizeof(Vtx));
        SDL_UnmapGPUTransferBuffer(sDev, sVxfer);
        src.transfer_buffer = sVxfer;
        src.offset = 0;
        dst.buffer = sVbuf;
        dst.offset = 0;
        dst.size = sVertCount * sizeof(Vtx);
        SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    }
    for (i = 0; i < sPendingCount; i++) {
        SDL_GPUTransferBufferCreateInfo ti;
        SDL_GPUTransferBuffer *tb;
        SDL_GPUTextureTransferInfo src;
        SDL_GPUTextureRegion dst;
        void *p;
        SDL_zero(ti);
        ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        ti.size = sPending[i].w * sPending[i].h * 4;
        tb = SDL_CreateGPUTransferBuffer(sDev, &ti);
        p = SDL_MapGPUTransferBuffer(sDev, tb, false);
        memcpy(p, sPending[i].px, ti.size);
        SDL_UnmapGPUTransferBuffer(sDev, tb);
        SDL_zero(src);
        src.transfer_buffer = tb;
        SDL_zero(dst);
        dst.texture = sPending[i].tex;
        dst.w = sPending[i].w;
        dst.h = sPending[i].h;
        dst.d = 1;
        SDL_UploadToGPUTexture(copy, &src, &dst, false);
        SDL_ReleaseGPUTransferBuffer(sDev, tb);
        free(sPending[i].px);
    }
    sPendingCount = 0;
    SDL_EndGPUCopyPass(copy);
    /* replay the frame */
    for (n = 0; n < sDrawCount; n++) {
        const Draw *d = &sDraws[n];
        SDL_GPUBufferBinding vb;
        SDL_GPUTextureSamplerBinding ts;
        SDL_FColor bc;
        struct { int32_t mode[4]; float misc[4]; } fu;

        if (d->native) { /* a native full-screen effect: its own pass on the colour texture alone, reading the alpha copy */
            SDL_GPUColorTargetInfo ft;
            SDL_GPUTextureSamplerBinding fs;
            SDL_Rect sc;
            float params[4];
            Target *t = &sTargets[d->target];
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
                pass = NULL;
            }
            cur = -1;
            if (!t->cleared) {
                continue;
            }
            SDL_zero(ft);
            ft.texture = t->color;
            ft.load_op = SDL_GPU_LOADOP_LOAD;
            ft.store_op = SDL_GPU_STOREOP_STORE;
            pass = SDL_BeginGPURenderPass(cmd, &ft, 1, NULL);
            SDL_BindGPUGraphicsPipeline(pass, sOutlinePipe);
            fs.texture = t->aux;
            fs.sampler = sSamplers[6]; /* nearest, clamped */
            SDL_BindGPUFragmentSamplers(pass, 0, &fs, 1);
            params[0] = 1.0f / (float)GS_W;
            params[1] = 1.0f / (float)GS_H;
            params[2] = 100.0f / 255.0f; /* the dark rectangle's 0x64 */
            params[3] = getenv("BT3_FX_DEBUG") != NULL ? 1.0f : 0.0f;
            SDL_PushGPUFragmentUniformData(cmd, 0, params, sizeof(params));
            sc = d->scissor;
            SDL_SetGPUScissor(pass, &sc);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
            pass = NULL;
            continue;
        }
        if (d->target != cur) {
            SDL_GPUColorTargetInfo ct, cts[2];
            SDL_GPUDepthStencilTargetInfo dt;
            Target *t = &sTargets[d->target];
            if (pass != NULL) {
                SDL_EndGPURenderPass(pass);
            }
            SDL_zero(ct);
            SDL_zero(dt);
            ct.texture = t->color;
            ct.load_op = t->cleared ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
            ct.store_op = SDL_GPU_STOREOP_STORE;
            cts[1] = ct;
            cts[1].texture = t->aux;
            if (getenv("BT3_GPU_MAGENTA") != NULL) { ct.clear_color.r = 1.0f; ct.clear_color.b = 1.0f; ct.clear_color.a = 1.0f; }
            dt.texture = t->depth;
            dt.load_op = t->cleared ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
            dt.store_op = SDL_GPU_STOREOP_STORE;
            dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
            dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
            t->cleared = 1;
            cts[0] = ct;
            pass = SDL_BeginGPURenderPass(cmd, cts, 2, &dt);
            cur = d->target;
            vb.buffer = sVbuf;
            vb.offset = 0;
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        }
        SDL_BindGPUGraphicsPipeline(pass, sPipes[d->pipeline].p);
        ts.texture = d->tex;
        ts.sampler = sSamplers[d->sampler];
        SDL_BindGPUFragmentSamplers(pass, 0, &ts, 1);
        memcpy(fu.mode, d->mode, sizeof(fu.mode));
        memcpy(fu.misc, d->misc, sizeof(fu.misc));
        SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));
        bc.r = bc.g = bc.b = bc.a = d->blendc;
        SDL_SetGPUBlendConstants(pass, bc);
        SDL_SetGPUScissor(pass, &d->scissor);
        SDL_DrawGPUPrimitives(pass, d->count, 1, d->first, 0);
    }
    if (pass != NULL) {
        SDL_EndGPURenderPass(pass);
    }
    /* show the buffer that was drawn to most */
    for (i = 0; i < sTargetCount; i++) {
        if (best < 0 || sTargets[i].draws > sTargets[best].draws) {
            best = i;
        }
    }
    if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, sWindow, &swap, &sw, &sh) && swap != NULL && best >= 0 && sTargets[best].cleared) {
        SDL_GPUBlitInfo bl;
        SDL_zero(bl);
        bl.source.texture = sTargets[best].color;
        bl.source.w = 512 * SCALE;
        bl.source.h = 448 * SCALE;
        bl.destination.texture = swap;
        bl.destination.w = sw;
        bl.destination.h = sh;
        bl.load_op = SDL_GPU_LOADOP_DONT_CARE;
        bl.filter = SDL_GPU_FILTER_LINEAR;
        SDL_BlitGPUTexture(cmd, &bl);
    }
    SDL_SubmitGPUCommandBuffer(cmd);
    /* BT3_SHOT=<n>: every n frames, read the shown buffer back and write port/build/shots/gpu_NNNNN.ppm */
    {
        static int every = -1;
        if (every < 0) {
            every = getenv("BT3_SHOT") != NULL ? atoi(getenv("BT3_SHOT")) : 0;
        }
        if (every > 0 && gGsFrame % (unsigned)every == 0 && best >= 0 && sTargets[best].cleared) {
            SDL_GPUTransferBufferCreateInfo ti;
            SDL_GPUTransferBuffer *tb;
            SDL_GPUTextureRegion src;
            SDL_GPUTextureTransferInfo dst;
            SDL_GPUCommandBuffer *c2 = SDL_AcquireGPUCommandBuffer(sDev);
            SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(c2);
            SDL_GPUFence *fence;
            Uint32 w = 512 * SCALE, h = 448 * SCALE, x, y;
            uint8_t *px;
            char name[64];
            FILE *fp;

            SDL_zero(ti);
            ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
            ti.size = w * h * 4;
            tb = SDL_CreateGPUTransferBuffer(sDev, &ti);
            SDL_zero(src);
            src.texture = getenv("BT3_SHOT_AUX") != NULL ? sTargets[best].aux : sTargets[best].color;
            src.w = w;
            src.h = h;
            src.d = 1;
            SDL_zero(dst);
            dst.transfer_buffer = tb;
            SDL_DownloadFromGPUTexture(cp, &src, &dst);
            SDL_EndGPUCopyPass(cp);
            fence = SDL_SubmitGPUCommandBufferAndAcquireFence(c2);
            SDL_WaitForGPUFences(sDev, true, &fence, 1);
            SDL_ReleaseGPUFence(sDev, fence);
            px = SDL_MapGPUTransferBuffer(sDev, tb, false);
            snprintf(name, sizeof(name), "port/build/shots/gpu_%05u.ppm", gGsFrame);
            fp = fopen(name, "wb");
            if (fp != NULL && px != NULL) {
                fprintf(fp, "P6\n%u %u\n255\n", w, h);
                for (y = 0; y < h; y++) {
                    for (x = 0; x < w; x++) {
                        if (getenv("BT3_SHOT_AUX") != NULL) { /* one byte per pixel: show it as grey, times 16 to see small ids */
                            uint8_t g = (uint8_t)(px[y * w + x] * 16), t[3] = {g, g, px[y * w + x]};
                            fwrite(t, 1, 3, fp);
                        } else {
                            fwrite(&px[(y * w + x) * 4], 1, 3, fp);
                        }
                    }
                }
                fclose(fp);
            }
            SDL_UnmapGPUTransferBuffer(sDev, tb);
            SDL_ReleaseGPUTransferBuffer(sDev, tb);
        }
    }
    for (i = 0; i < sTargetCount; i++) {
        sTargets[i].draws = 0;
    }
    sVertCount = 0;
    sDrawCount = 0;
    sSkipped = 0;
    sNative = 0;
    /* the game runs at 30 frames per second (two vertical blanks per frame) */
    {
        uint64_t now = SDL_GetTicksNS();
        if (sNextFrameNs > now && sNextFrameNs - now < 100000000ull) {
            SDL_DelayPrecise(sNextFrameNs - now);
            sNextFrameNs += 33366700ull;
        } else {
            sNextFrameNs = now + 33366700ull;
        }
    }
}
