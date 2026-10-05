/*
 * Graphics Synthesizer, reference implementation in software.
 *
 * The game's drawing code runs unchanged and produces what it produced on the PS2: one DMA chain per frame on the
 * VIF1 channel, carrying GIF packets (register writes, primitives, texture uploads) for the GS and data for the
 * VU1 vertex programs. This file interprets the chain and draws the GS part into an ordinary RGBA image:
 *     DMA chain  ->  VIF1 commands  ->  GIF packets  ->  GS registers and vertices  ->  rasteriser.
 * It is the readable, dependency-free reference: screenshots for tests, and the specification by example for the
 * GPU renderer, which will take over from the "GS registers and vertices" stage.
 *
 * Not here yet: the VU1 vertex programs (3D models: their data is skipped), reading the frame buffer back as a
 * texture, mip-mapping, fog, dithering, bilinear filtering.
 * Textures are kept per upload, keyed by their GS memory address (no emulation of GS memory layout): a texture
 * must be drawn with the same base pointer it was uploaded to.
 *
 * Compiled with the host's float unit (this is not simulation code). Enabled with BT3_GS=1.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SURF_W 1024
#define SURF_H 1024

/* ------------------------------------------------------------------------------------------------ GS state */

typedef struct Upload {
    uint32_t bp, psm, w, h; /* w, h: allocated extent */
    uint32_t *px;           /* one word per pixel: colour as stored, or the palette index */
} Upload;

typedef struct Surface {
    uint32_t fbp;
    uint32_t *rgba; /* R | G << 8 | B << 16 | A << 24, A as on the GS (0x80 = opaque) */
    uint32_t *z;
    unsigned drawn; /* pixels written since the last frame end */
} Surface;

typedef struct Vertex {
    float x, y;
    uint32_t z;
    float s, t, q;
    int u, v;
    uint8_t r, g, b, a;
} Vertex;

static struct {
    uint64_t prim, rgbaq, st, uv, texa, prmodecont, prmode;
    uint64_t tex0[2], clamp[2], xyoffset[2], scissor[2], alpha[2], test[2], frame[2], zbuf[2];
    uint64_t bitbltbuf, trxpos, trxreg, trxdir;
    float q;
    Vertex vtx[3];
    int vcount; /* vertices in the queue */
    int strip;  /* vertices seen since the last PRIM write */
    /* host -> GS transfer in progress */
    Upload *dst;
    uint32_t tx, ty, tw, th, tpos;
} gs;

static Upload sUploads[4096];
static int sUploadCount;
static Surface sSurfaces[16];
static int sSurfaceCount;
static unsigned sFrame, sMissingTex;
static unsigned sStat[8]; /* per frame: DMA tags, VIF codes, DIRECT quadwords, GIF tags, register writes, vertices, image quadwords, unknown VIF */

static Upload *upload_find(uint32_t bp) {
    int i;
    for (i = 0; i < sUploadCount; i++) {
        if (sUploads[i].bp == bp) {
            return &sUploads[i];
        }
    }
    return NULL;
}

static Upload *upload_get(uint32_t bp, uint32_t psm, uint32_t w, uint32_t h) {
    Upload *u = upload_find(bp);

    if (u == NULL) {
        if (sUploadCount == 4096) {
            sUploadCount = 0; /* crude: start over */
        }
        u = &sUploads[sUploadCount++];
        memset(u, 0, sizeof(*u));
        u->bp = bp;
    }
    if (u->psm != psm || u->w < w || u->h < h || u->px == NULL) {
        uint32_t nw = u->psm == psm && u->w > w ? u->w : w, nh = u->psm == psm && u->h > h ? u->h : h;
        uint32_t *px = calloc((size_t)nw * nh, 4);
        uint32_t y;

        if (u->px != NULL && u->psm == psm) {
            for (y = 0; y < u->h; y++) {
                memcpy(px + (size_t)y * nw, u->px + (size_t)y * u->w, (size_t)u->w * 4);
            }
        }
        free(u->px);
        u->px = px;
        u->w = nw;
        u->h = nh;
        u->psm = psm;
    }
    return u;
}

static Surface *surface_get(uint32_t fbp) {
    int i;
    for (i = 0; i < sSurfaceCount; i++) {
        if (sSurfaces[i].fbp == fbp) {
            return &sSurfaces[i];
        }
    }
    if (sSurfaceCount == 16) {
        return &sSurfaces[0];
    }
    sSurfaces[sSurfaceCount].fbp = fbp;
    sSurfaces[sSurfaceCount].rgba = calloc(SURF_W * SURF_H, 4);
    sSurfaces[sSurfaceCount].z = calloc(SURF_W * SURF_H, 4);
    return &sSurfaces[sSurfaceCount++];
}

/* ------------------------------------------------------------------------------------------------ textures */

static int psm_bits(uint32_t psm) {
    switch (psm) {
    case 0x00: return 32;
    case 0x01: return 24;
    case 0x02: case 0x0A: return 16;
    case 0x13: case 0x1B: return 8;
    case 0x14: case 0x24: case 0x2C: return 4;
    default: return 32;
    }
}

/* A stored colour of format psm as R, G, B, A (TEXA supplies alpha where the format has none or one bit). */
static uint32_t expand(uint32_t c, uint32_t psm) {
    uint32_t ta0 = gs.texa & 0xFF, ta1 = (gs.texa >> 32) & 0xFF, aem = (gs.texa >> 15) & 1;

    if (psm == 0x00) {
        return c;
    }
    if (psm == 0x01) {
        c &= 0xFFFFFF;
        return c | ((aem && c == 0 ? 0 : ta0) << 24);
    }
    {
        uint32_t r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
        uint32_t a = (c & 0x8000) ? ta1 : (aem && (c & 0x7FFF) == 0 ? 0 : ta0);
        return r | g << 8 | b << 16 | a << 24;
    }
}

static uint32_t texel(int ctx, int u, int v) {
    uint64_t t0 = gs.tex0[ctx], cl = gs.clamp[ctx];
    uint32_t tbp = t0 & 0x3FFF, psm = (t0 >> 20) & 0x3F, tw = 1u << ((t0 >> 26) & 15), th = 1u << ((t0 >> 30) & 15);
    uint32_t cbp = (t0 >> 37) & 0x3FFF, cpsm = (t0 >> 51) & 15, csa = (t0 >> 56) & 31;
    uint32_t wms = cl & 3, wmt = (cl >> 2) & 3;
    int minu = (cl >> 4) & 0x3FF, maxu = (cl >> 14) & 0x3FF, minv = (cl >> 24) & 0x3FF, maxv = (cl >> 34) & 0x3FF;
    Upload *tex = upload_find(tbp), *clut;
    uint32_t c;

    if (wms == 0) { u &= (int)tw - 1; } else if (wms == 1) { u = u < 0 ? 0 : u >= (int)tw ? (int)tw - 1 : u; }
    else if (wms == 2) { u = u < minu ? minu : u > maxu ? maxu : u; } else { u = (u & minu) | maxu; }
    if (wmt == 0) { v &= (int)th - 1; } else if (wmt == 1) { v = v < 0 ? 0 : v >= (int)th ? (int)th - 1 : v; }
    else if (wmt == 2) { v = v < minv ? minv : v > maxv ? maxv : v; } else { v = (v & minv) | maxv; }
    if (tex == NULL) {
        sMissingTex++;
        return 0x80FFFFFFu;
    }
    if (u < 0 || v < 0 || (uint32_t)u >= tex->w || (uint32_t)v >= tex->h) {
        return 0;
    }
    c = tex->px[(size_t)v * tex->w + u];
    if (psm_bits(psm) > 8) {
        return expand(c, psm);
    }
    clut = upload_find(cbp);
    if (clut == NULL) {
        return 0x80000000u | (c * 0x010101u); /* no palette: show the index as grey */
    }
    if (psm_bits(psm) == 8) {
        c = (c & 0xE7) | ((c & 8) << 1) | ((c & 0x10) >> 1); /* the GS reads an 8-bit palette in this order */
        if (clut->w >= 16) {
            c = (c >> 4) * clut->w + (c & 15);
        }
    } else {
        c = (c & 15) + csa * 16;
        if (clut->w >= 8 && clut->w < 16) {
            c = (c >> 3) * clut->w + (c & 7);
        }
    }
    if (c >= clut->w * clut->h) {
        return 0;
    }
    return expand(clut->px[c], cpsm);
}

/* ------------------------------------------------------------------------------------------------ pixels */

static void pixel(Surface *sf, int ctx, int x, int y, uint32_t z, uint32_t src) {
    uint64_t test = gs.test[ctx], al = gs.alpha[ctx], zb = gs.zbuf[ctx], pr = gs.prim;
    uint64_t sc = gs.scissor[ctx];
    uint32_t *dp, d, sr, sg, sb, sa, out[3];
    int i, write_rgb = 1, write_z = !((zb >> 32) & 1);

    if (x < (int)(sc & 0x7FF) || x > (int)((sc >> 16) & 0x7FF) || y < (int)((sc >> 32) & 0x7FF) || y > (int)((sc >> 48) & 0x7FF) ||
        x < 0 || y < 0 || x >= SURF_W || y >= SURF_H) {
        return;
    }
    sa = src >> 24;
    if (test & 1) { /* alpha test */
        uint32_t ref = (test >> 4) & 0xFF;
        int pass;
        switch ((test >> 1) & 7) {
        case 0: pass = 0; break;
        case 1: pass = 1; break;
        case 2: pass = sa < ref; break;
        case 3: pass = sa <= ref; break;
        case 4: pass = sa == ref; break;
        case 5: pass = sa >= ref; break;
        case 6: pass = sa > ref; break;
        default: pass = sa != ref; break;
        }
        if (!pass) {
            switch ((test >> 12) & 3) {
            case 0: return;                        /* keep */
            case 1: write_z = 0; break;            /* frame buffer only */
            case 2: write_rgb = 0; break;          /* z only */
            default: write_z = 0; break;           /* RGB only */
            }
        }
    }
    if ((test >> 16) & 1) { /* depth test */
        uint32_t zd = sf->z[y * SURF_W + x];
        switch ((test >> 17) & 3) {
        case 0: return;
        case 1: break;
        case 2: if (z < zd) { return; } break;
        default: if (z <= zd) { return; } break;
        }
    }
    dp = &sf->rgba[y * SURF_W + x];
    d = *dp;
    sr = src & 0xFF; sg = (src >> 8) & 0xFF; sb = (src >> 16) & 0xFF;
    out[0] = sr; out[1] = sg; out[2] = sb;
    if ((pr >> 6) & 1) { /* blending: ((A - B) * C >> 7) + D */
        uint32_t s[3] = {sr, sg, sb};
        uint32_t c = ((al >> 4) & 3) == 0 ? sa : ((al >> 4) & 3) == 1 ? d >> 24 : (uint32_t)((al >> 32) & 0xFF);
        for (i = 0; i < 3; i++) {
            int dc = (int)((d >> (8 * i)) & 0xFF);
            int a = (al & 3) == 0 ? (int)s[i] : (al & 3) == 1 ? dc : 0;
            int b = ((al >> 2) & 3) == 0 ? (int)s[i] : ((al >> 2) & 3) == 1 ? dc : 0;
            int dd = ((al >> 6) & 3) == 0 ? (int)s[i] : ((al >> 6) & 3) == 1 ? dc : 0;
            int v = (((a - b) * (int)c) >> 7) + dd;
            out[i] = v < 0 ? 0 : v > 255 ? 255 : (uint32_t)v;
        }
    }
    if (write_rgb) {
        *dp = out[0] | out[1] << 8 | out[2] << 16 | sa << 24;
        sf->drawn++;
    }
    if (write_z) {
        sf->z[y * SURF_W + x] = z;
    }
}

/* Vertex colour and texture combined (TEX0.TFX / TCC). */
static uint32_t shade(int ctx, uint32_t vc, float s, float t, float q, int u, int v) {
    uint64_t pr = gs.prim, t0 = gs.tex0[ctx];
    uint32_t tc, c[4], out = 0, tfx = (t0 >> 35) & 3, tcc = (t0 >> 34) & 1;
    int i;

    if (!((pr >> 4) & 1)) {
        return vc;
    }
    if ((pr >> 8) & 1) {
        tc = texel(ctx, u >> 4, v >> 4);
    } else {
        float w = q != 0.0f ? 1.0f / q : 1.0f;
        tc = texel(ctx, (int)(s * w * (float)(1u << ((t0 >> 26) & 15))), (int)(t * w * (float)(1u << ((t0 >> 30) & 15))));
    }
    for (i = 0; i < 3; i++) {
        uint32_t cv = (vc >> (8 * i)) & 0xFF, ct = (tc >> (8 * i)) & 0xFF, av = vc >> 24;
        c[i] = tfx == 0 ? (ct * cv) >> 7 : tfx == 1 ? ct : ((ct * cv) >> 7) + av;
        if (c[i] > 255) { c[i] = 255; }
        out |= c[i] << (8 * i);
    }
    {
        uint32_t av = vc >> 24, at = tc >> 24;
        c[3] = !tcc ? av : tfx == 0 ? (at * av) >> 7 : tfx == 2 ? at + av : at;
        if (c[3] > 255) { c[3] = 255; }
    }
    return out | c[3] << 24;
}

/* ------------------------------------------------------------------------------------------------ primitives */

static uint32_t vcol(const Vertex *v) {
    return v->r | v->g << 8 | v->b << 16 | (uint32_t)v->a << 24;
}

static void draw_sprite(Surface *sf, int ctx, const Vertex *a, const Vertex *b) {
    float x0 = a->x, x1 = b->x, y0 = a->y, y1 = b->y;
    int xa, xb, ya, yb, x, y;
    uint32_t col = vcol(b);

    if (x0 > x1) { const Vertex *t = a; a = b; b = t; x0 = a->x; x1 = b->x; }
    xa = (int)(x0 + 0.9375f); xb = (int)(x1 + 0.9375f);
    if (y0 <= y1) { ya = (int)(y0 + 0.9375f); yb = (int)(y1 + 0.9375f); } else { ya = (int)(y1 + 0.9375f); yb = (int)(y0 + 0.9375f); }
    for (y = ya; y < yb; y++) {
        float fy = y1 != y0 ? ((float)y - y0) / (y1 - y0) : 0.0f;
        for (x = xa; x < xb; x++) {
            float fx = x1 != x0 ? ((float)x - x0) / (x1 - x0) : 0.0f;
            pixel(sf, ctx, x, y, b->z, shade(ctx, col, a->s + (b->s - a->s) * fx, a->t + (b->t - a->t) * fy, b->q,
                                             (int)((float)a->u + (float)(b->u - a->u) * fx), (int)((float)a->v + (float)(b->v - a->v) * fy)));
        }
    }
}

static float edge(const Vertex *a, const Vertex *b, float x, float y) {
    return (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x);
}

static void draw_triangle(Surface *sf, int ctx, const Vertex *a, const Vertex *b, const Vertex *c) {
    float area = edge(a, b, c->x, c->y);
    int gouraud = (gs.prim >> 3) & 1;
    float minx = a->x, maxx = a->x, miny = a->y, maxy = a->y;
    int x, y;

    if (area == 0.0f) {
        return;
    }
    if (b->x < minx) { minx = b->x; } if (c->x < minx) { minx = c->x; }
    if (b->x > maxx) { maxx = b->x; } if (c->x > maxx) { maxx = c->x; }
    if (b->y < miny) { miny = b->y; } if (c->y < miny) { miny = c->y; }
    if (b->y > maxy) { maxy = b->y; } if (c->y > maxy) { maxy = c->y; }
    if (minx < 0) { minx = 0; } if (miny < 0) { miny = 0; }
    if (maxx > SURF_W - 1) { maxx = SURF_W - 1; } if (maxy > SURF_H - 1) { maxy = SURF_H - 1; }
    for (y = (int)miny; y <= (int)maxy; y++) {
        for (x = (int)minx; x <= (int)maxx; x++) {
            float w0 = edge(b, c, (float)x, (float)y) / area, w1 = edge(c, a, (float)x, (float)y) / area, w2 = 1.0f - w0 - w1;
            uint32_t col;

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                continue;
            }
            if (gouraud) {
                col = (uint32_t)(a->r * w0 + b->r * w1 + c->r * w2) | (uint32_t)(a->g * w0 + b->g * w1 + c->g * w2) << 8 |
                      (uint32_t)(a->b * w0 + b->b * w1 + c->b * w2) << 16 | (uint32_t)(a->a * w0 + b->a * w1 + c->a * w2) << 24;
            } else {
                col = vcol(c);
            }
            pixel(sf, ctx, x, y, (uint32_t)((double)a->z * w0 + (double)b->z * w1 + (double)c->z * w2),
                  shade(ctx, col, a->s * w0 + b->s * w1 + c->s * w2, a->t * w0 + b->t * w1 + c->t * w2,
                        a->q * w0 + b->q * w1 + c->q * w2, (int)(a->u * w0 + b->u * w1 + c->u * w2),
                        (int)(a->v * w0 + b->v * w1 + c->v * w2)));
        }
    }
}

static void draw_line(Surface *sf, int ctx, const Vertex *a, const Vertex *b) {
    float dx = b->x - a->x, dy = b->y - a->y;
    int n = (int)((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy)), i;

    for (i = 0; i <= n; i++) {
        float f = n ? (float)i / (float)n : 0.0f;
        pixel(sf, ctx, (int)(a->x + dx * f), (int)(a->y + dy * f), b->z, vcol(b));
    }
}

/* A vertex arrives (XYZ2 / XYZF2 with kick = 1, XYZ3 / XYZF3 with kick = 0). */
static void vertex(uint32_t x, uint32_t y, uint32_t z, int kick) {
    int ctx = (gs.prim >> 9) & 1, type = gs.prim & 7;
    Surface *sf;
    Vertex v;

    sStat[5]++;
    v.x = ((float)(int)x - (float)(gs.xyoffset[ctx] & 0xFFFF)) / 16.0f;
    v.y = ((float)(int)y - (float)((gs.xyoffset[ctx] >> 32) & 0xFFFF)) / 16.0f;
    v.z = z;
    v.r = gs.rgbaq & 0xFF; v.g = (gs.rgbaq >> 8) & 0xFF; v.b = (gs.rgbaq >> 16) & 0xFF; v.a = (gs.rgbaq >> 24) & 0xFF;
    memcpy(&v.s, &gs.st, 4);
    { uint32_t tt = (uint32_t)(gs.st >> 32); memcpy(&v.t, &tt, 4); }
    v.q = gs.q;
    v.u = gs.uv & 0x3FFF; v.v = (gs.uv >> 16) & 0x3FFF;
    if (gs.vcount < 3) {
        gs.vtx[gs.vcount++] = v;
    } else {
        gs.vtx[0] = gs.vtx[1]; gs.vtx[1] = gs.vtx[2]; gs.vtx[2] = v;
    }
    gs.strip++;
    sf = surface_get(gs.frame[ctx] & 0x1FF);
    switch (type) {
    case 0: if (kick) { pixel(sf, ctx, (int)v.x, (int)v.y, v.z, vcol(&v)); } gs.vcount = 0; break;
    case 1: if (gs.vcount == 2) { if (kick) { draw_line(sf, ctx, &gs.vtx[0], &gs.vtx[1]); } gs.vcount = 0; } break;
    case 2: if (gs.vcount == 2) { if (kick) { draw_line(sf, ctx, &gs.vtx[0], &gs.vtx[1]); } gs.vtx[0] = gs.vtx[1]; gs.vcount = 1; } break;
    case 3: if (gs.vcount == 3) { if (kick) { draw_triangle(sf, ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]); } gs.vcount = 0; } break;
    case 4: if (gs.vcount == 3 && kick) { draw_triangle(sf, ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]); } break;
    case 5:
        if (gs.vcount == 3) {
            if (kick) { draw_triangle(sf, ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]); }
            gs.vtx[1] = gs.vtx[2]; /* fan: keep the first vertex */
            gs.vcount = 2;
        }
        break;
    case 6: if (gs.vcount == 2) { if (kick) { draw_sprite(sf, ctx, &gs.vtx[0], &gs.vtx[1]); } gs.vcount = 0; } break;
    default: break;
    }
}

/* ------------------------------------------------------------------------------------------------ registers */

static void transfer_begin(void) {
    uint32_t dbp = (gs.bitbltbuf >> 32) & 0x3FFF, dbw = (gs.bitbltbuf >> 48) & 0x3F, dpsm = (gs.bitbltbuf >> 56) & 0x3F;

    gs.dst = NULL;
    if ((gs.trxdir & 3) != 0) {
        return; /* only host -> GS */
    }
    gs.tx = (gs.trxpos >> 32) & 0x7FF;
    gs.ty = (gs.trxpos >> 48) & 0x7FF;
    gs.tw = gs.trxreg & 0xFFF;
    gs.th = (gs.trxreg >> 32) & 0xFFF;
    gs.tpos = 0;
    (void)dbw;
    gs.dst = upload_get(dbp, dpsm, gs.tx + gs.tw, gs.ty + gs.th);
}

static void transfer_data(const uint8_t *p, uint32_t bytes) {
    Upload *u = gs.dst;
    uint32_t bits, total, i;

    if (u == NULL || gs.tw == 0) {
        return;
    }
    bits = (uint32_t)psm_bits(u->psm);
    total = gs.tw * gs.th;
    for (i = 0; i * bits < bytes * 8 && gs.tpos < total; gs.tpos++) {
        uint32_t x = gs.tx + gs.tpos % gs.tw, y = gs.ty + gs.tpos / gs.tw, c;

        switch (bits) {
        case 32: c = p[i * 4] | p[i * 4 + 1] << 8 | p[i * 4 + 2] << 16 | (uint32_t)p[i * 4 + 3] << 24; i++; break;
        case 24: c = p[i * 3] | p[i * 3 + 1] << 8 | p[i * 3 + 2] << 16; i++; break;
        case 16: c = p[i * 2] | p[i * 2 + 1] << 8; i++; break;
        case 8: c = p[i]; i++; break;
        default: c = (p[i / 2] >> ((i & 1) * 4)) & 15; i++; break;
        }
        u->px[(size_t)y * u->w + x] = c;
    }
}

static void reg_write(uint32_t addr, uint64_t d) {
    sStat[4]++;
    switch (addr) {
    case 0x00: gs.prim = d; gs.vcount = 0; gs.strip = 0; break;
    case 0x01: gs.rgbaq = d; { uint32_t q = (uint32_t)(d >> 32); memcpy(&gs.q, &q, 4); } break;
    case 0x02: gs.st = d; break;
    case 0x03: gs.uv = d; break;
    case 0x04: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)((d >> 32) & 0xFFFFFF), 1); break;
    case 0x05: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)(d >> 32), 1); break;
    case 0x06: case 0x07: gs.tex0[addr - 6] = d; break;
    case 0x08: case 0x09: gs.clamp[addr - 8] = d; break;
    case 0x0C: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)((d >> 32) & 0xFFFFFF), 0); break;
    case 0x0D: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)(d >> 32), 0); break;
    case 0x16: case 0x17: gs.tex0[addr - 0x16] = (gs.tex0[addr - 0x16] & ~0x1FFFFFE003F00000ull) | (d & 0x1FFFFFE003F00000ull); break; /* TEX2: PSM and CLUT fields */
    case 0x18: case 0x19: gs.xyoffset[addr - 0x18] = d; break;
    case 0x1A: gs.prmodecont = d; break;
    case 0x1B: gs.prmode = d; break;
    case 0x3B: gs.texa = d; break;
    case 0x40: case 0x41: gs.scissor[addr - 0x40] = d; break;
    case 0x42: case 0x43: gs.alpha[addr - 0x42] = d; break;
    case 0x47: case 0x48: gs.test[addr - 0x47] = d; break;
    case 0x4C: case 0x4D: gs.frame[addr - 0x4C] = d; break;
    case 0x4E: case 0x4F: gs.zbuf[addr - 0x4E] = d; break;
    case 0x50: gs.bitbltbuf = d; break;
    case 0x51: gs.trxpos = d; break;
    case 0x52: gs.trxreg = d; break;
    case 0x53: gs.trxdir = d; transfer_begin(); break;
    default: break;
    }
}

/* ------------------------------------------------------------------------------------------------ GIF */

/* One run of GIF data (whole quadwords). */
static void gif(const uint8_t *p, uint32_t qwc) {
    static uint64_t tag_lo, tag_hi; /* current tag: persists across runs */
    static uint32_t loops, reg;     /* loops left, register index within the loop */
    const uint64_t *q = (const uint64_t *)p;

    while (qwc != 0) {
        uint32_t flg, nreg;

        if (loops == 0) {
            sStat[3]++;
            tag_lo = q[0];
            tag_hi = q[1];
            q += 2;
            qwc--;
            loops = tag_lo & 0x7FFF;
            reg = 0;
            if (loops != 0 && ((tag_lo >> 46) & 1) && ((tag_lo >> 58) & 3) == 0) {
                reg_write(0, (tag_lo >> 47) & 0x7FF);
            }
            continue;
        }
        flg = (tag_lo >> 58) & 3;
        nreg = (tag_lo >> 60) & 15;
        if (nreg == 0) {
            nreg = 16;
        }
        if (flg == 0) { /* PACKED: one quadword per register */
            uint32_t r = (tag_hi >> (4 * reg)) & 15;
            uint64_t lo = q[0], hi = q[1];

            switch (r) {
            case 0x0: reg_write(0, lo & 0x7FF); break;
            case 0x1: reg_write(1, (lo & 0xFF) | ((lo >> 32) & 0xFF) << 8 | (hi & 0xFF) << 16 | ((hi >> 32) & 0xFF) << 24 |
                                   (uint64_t)*(uint32_t *)&gs.q << 32); break;
            case 0x2: gs.st = lo; { uint32_t qq = (uint32_t)hi; memcpy(&gs.q, &qq, 4); } break;
            case 0x3: reg_write(3, (lo & 0x3FFF) | ((lo >> 32) & 0x3FFF) << 16); break;
            case 0x4: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)((hi >> 4) & 0xFFFFFF), !((hi >> 47) & 1)); break;
            case 0x5: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)hi, !((hi >> 47) & 1)); break;
            case 0xC: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)((hi >> 4) & 0xFFFFFF), 0); break;
            case 0xD: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)hi, 0); break;
            case 0xE: reg_write((uint32_t)(hi & 0xFF), lo); break;
            case 0xF: break;
            default: reg_write(r, lo); break;
            }
            q += 2;
            qwc--;
            if (++reg == nreg) {
                reg = 0;
                loops--;
            }
        } else if (flg == 1) { /* REGLIST: 64 bits per register */
            int half;
            for (half = 0; half < 2 && loops != 0; half++) {
                uint32_t r = (tag_hi >> (4 * reg)) & 15;
                if (r != 0xE && r != 0xF) {
                    reg_write(r, q[half]);
                }
                if (++reg == nreg) {
                    reg = 0;
                    loops--;
                }
            }
            q += 2;
            qwc--;
        } else { /* IMAGE */
            uint32_t n = loops < qwc ? loops : qwc;
            sStat[6] += n;
            transfer_data((const uint8_t *)q, n * 16);
            q += 2 * n;
            qwc -= n;
            loops -= n;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ VIF1 */

static uint32_t sVifCl = 1, sVifWl = 1;

/* One run of VIF data (32-bit words). GIF data (DIRECT) goes to the GS; everything for VU1 is skipped.
   A command's data may continue in the next run (the command word often sits in the DMA tag, its data behind). */
static void vif(const uint32_t *w, uint32_t count) {
    static uint32_t direct; /* quadwords of GIF data still to come */
    static uint32_t skip;   /* words of other data still to come */
    uint32_t i = 0;

    while (i < count) {
        uint32_t code, cmd, num, imm;

        if (direct != 0) {
            uint32_t qwc = (count - i) / 4 < direct ? (count - i) / 4 : direct;
            if (qwc == 0) {
                break; /* misaligned run: drop it */
            }
            sStat[2] += qwc;
            gif((const uint8_t *)&w[i], qwc);
            i += qwc * 4;
            direct -= qwc;
            continue;
        }
        if (skip != 0) {
            uint32_t n = count - i < skip ? count - i : skip;
            i += n;
            skip -= n;
            continue;
        }
        code = w[i++];
        cmd = (code >> 24) & 0x7F;
        num = (code >> 16) & 0xFF;
        imm = code & 0xFFFF;
        sStat[1]++;
        if (cmd == 0x01) {
            sVifCl = imm & 0xFF;
            sVifWl = (imm >> 8) & 0xFF;
        } else if (cmd == 0x20) {
            skip = 1;
        } else if (cmd == 0x30 || cmd == 0x31) {
            skip = 4;
        } else if (cmd == 0x4A) {
            skip = (num ? num : 256) * 2;
        } else if (cmd == 0x50 || cmd == 0x51) {
            direct = imm ? imm : 65536;
        } else if (cmd >= 0x60) {
            uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, n = num ? num : 256;
            uint32_t bits = vn == 3 && vl == 3 ? 16 : (32u >> vl) * (vn + 1);
            if (sVifWl > sVifCl && sVifWl != 0) {
                n = sVifCl * (n / sVifWl) + (n % sVifWl > sVifCl ? sVifCl : n % sVifWl);
            }
            skip = (n * bits + 31) / 32;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ DMA and frames */

static void screenshot(void) {
    static int every = -1;
    Surface *best = NULL;
    char name[64];
    FILE *fp;
    int i, x, y;

    if (every < 0) {
        every = getenv("BT3_SHOT") != NULL ? atoi(getenv("BT3_SHOT")) : 0;
    }
    for (i = 0; i < sSurfaceCount; i++) {
        if (best == NULL || sSurfaces[i].drawn > best->drawn) {
            best = &sSurfaces[i];
        }
    }
    if (every > 0 && best != NULL && sFrame % (unsigned)every == 0) {
        snprintf(name, sizeof(name), "port/build/shots/frame_%05u.ppm", sFrame);
        fp = fopen(name, "wb");
        if (fp != NULL) {
            fprintf(fp, "P6\n640 448\n255\n");
            for (y = 0; y < 448; y++) {
                for (x = 0; x < 640; x++) {
                    fwrite(&best->rgba[y * SURF_W + x], 1, 3, fp);
                }
            }
            fclose(fp);
        }
    }
    for (i = 0; i < sSurfaceCount; i++) {
        sSurfaces[i].drawn = 0;
    }
    sFrame++;
}

/* Carries out a source-chain transfer on VIF1 that starts at `tadr` (PS2 address = host address). */
void Port_GsVif1Chain(uint32_t tadr, int tte) {
    static int on = -1;
    uint32_t stack[2], sp = 0;
    int guard;

    if (on < 0) {
        on = getenv("BT3_GS") != NULL;
    }
    if (!on) {
        return;
    }
    for (guard = 0; guard < 100000; guard++) {
        const uint32_t *tag = (const uint32_t *)(uintptr_t)tadr;
        uint32_t qwc = tag[0] & 0xFFFF, id = (tag[0] >> 28) & 7, addr = tag[1] & 0x7FFFFFFF;
        const uint32_t *data;
        int end = 0;

        switch (id) {
        case 0: data = (const uint32_t *)(uintptr_t)addr; end = 1; break;                 /* REFE */
        case 1: data = tag + 4; tadr += 16 + qwc * 16; break;                             /* CNT */
        case 2: data = tag + 4; tadr = addr; break;                                       /* NEXT */
        case 3: case 4: data = (const uint32_t *)(uintptr_t)addr; tadr += 16; break;      /* REF, REFS */
        case 5: data = tag + 4; if (sp < 2) { stack[sp++] = tadr + 16 + qwc * 16; } tadr = addr; break; /* CALL */
        case 6: data = tag + 4; if (sp > 0) { tadr = stack[--sp]; } else { end = 1; } break;          /* RET */
        default: data = tag + 4; end = 1; break;                                          /* END */
        }
        sStat[0]++;
        if (tte) {
            vif(tag + 2, 2);
        }
        vif(data, qwc * 4);
        if (end) {
            break;
        }
    }
    if (getenv("BT3_GS_VERBOSE") != NULL && (sFrame < 4 || sFrame % 60 == 0)) {
        fprintf(stderr, "gs: frame %u: %u DMA tags, %u VIF codes, %u DIRECT qwords, %u GIF tags, %u register writes, %u vertices, "
                        "%u image qwords, %d surfaces\n", sFrame, sStat[0], sStat[1], sStat[2], sStat[3], sStat[4], sStat[5],
                sStat[6], sSurfaceCount);
    }
    memset(sStat, 0, sizeof(sStat));
    screenshot();
    if (sMissingTex != 0 && getenv("BT3_GS_VERBOSE") != NULL) {
        fprintf(stderr, "gs: frame %u: %u texel reads from textures that were never uploaded\n", sFrame, sMissingTex);
    }
    sMissingTex = 0;
}
