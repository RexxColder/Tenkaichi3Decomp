/* Shared between the GS front end (gs_core.c: DMA chain -> VIF1 -> GIF -> registers, GS memory, software
   rasteriser) and the GPU back end (gs_gpu.c). */
#ifndef GS_INTERNAL_H
#define GS_INTERNAL_H
#include <stdint.h>

typedef struct GsVertex {
    float x, y;       /* frame-buffer pixels */
    uint32_t z;
    float s, t, q;    /* texture coordinates when PRIM.FST = 0 */
    int u, v;         /* texel coordinates, 12.4, when PRIM.FST = 1 */
    uint8_t r, g, b, a;
} GsVertex;

typedef struct GsState {
    uint64_t prim, rgbaq, st, uv, texa, prmodecont, prmode;
    uint64_t tex0[2], tex1[2], clamp[2], xyoffset[2], scissor[2], alpha[2], test[2], frame[2], zbuf[2];
    uint64_t bitbltbuf, trxpos, trxreg, trxdir;
    float q;
    GsVertex vtx[3];
    int vcount; /* vertices in the queue */
    int strip;  /* vertices seen since the last PRIM write */
    /* host -> GS transfer in progress */
    int transfer;
    uint32_t tx, ty, tw, th, tpos;
} GsState;

extern GsState gGs;
extern uint32_t gGsPageGen[512]; /* bumped whenever an upload writes into that 8 KB page of GS memory */
extern unsigned gGsFrame;

uint32_t Gs_VramRead(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y);
int Gs_PsmBits(uint32_t psm);
uint32_t Gs_Expand(uint32_t c, uint32_t psm); /* stored colour -> R | G << 8 | B << 16 | A << 24 */

/* GPU back end (gs_gpu.c). Primitive = PRIM type: 0 point, 1 line, 3 triangle, 6 sprite (strips and fans arrive
   as single lines / triangles). The state to draw with is gGs. */
int GsGpu_Init(void);
void GsGpu_Draw(int type, int ctx, const GsVertex *v);
void GsGpu_FrameEnd(void);
#endif
