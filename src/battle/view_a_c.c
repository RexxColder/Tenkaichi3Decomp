#include "common.h"
#include "battle/view_a.h"
#include "sys/heap.h"

/*
 * IconWin, 0x25D290..0x25D468: a small movie window ("fl_in" / "fl_out") whose fifth texture is an icon picked
 * from a second sprite resource. Used all over the menu overlay. See battle/view_a.h.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern void *Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void Flash_Create(Flash *flash, void *data, void *tex);
extern void func_0010D648(Flash *flash);
extern void func_0010D6F0(Flash *flash);
extern void func_0010D750(Flash *flash);
extern void func_0010D810(Flash *flash, s32 arg);
extern void func_0010D878(Flash *flash, char *label, s32 arg);

/* Defined in this object in the original, in .sdata: `IconWin *gIconWin = NULL;` (see the report on the data layout). */
extern IconWin *gIconWin;

/* Allocates the window: movie and three textures from the pack, two more from the icon resource. */
void IconWin_Init(u32 *pack, FlashTexRes *icons) {
    FlashTexRes *res = NULL;

    gIconWin = Heap_Alloc(sizeof(IconWin), 0x20, 0, HEAP_ANY);
    memset(gIconWin, 0, sizeof(IconWin));

    res = PACK_AT(pack, 1);
    Res_RelocateOffsets(&res, res, res);
    gIconWin->tex[1] = res->tex;
    gIconWin->tex[0] = res->tex + FLASH_TEX_SIZE;
    gIconWin->tex[3] = res->tex + FLASH_TEX_SIZE * 2;
    gIconWin->tex[2] = icons->tex;
    gIconWin->tex[4] = icons->tex + FLASH_TEX_SIZE;
    gIconWin->icons = icons;

    Flash_Create(&gIconWin->flash, PACK_AT(pack, 2), gIconWin->tex);
    func_0010D810(&gIconWin->flash, 1);
}

/* Destroys the movie and frees the window. */
void IconWin_Term(void) {
    func_0010D648(&gIconWin->flash);
    if (gIconWin != NULL) {
        Heap_Free(gIconWin);
        gIconWin = NULL;
    }
}

/* Steps and draws the window. */
void IconWin_Draw(void) {
    s32 i;

    func_0010D6F0(&gIconWin->flash);
    /* a loop over one movie, as in the other windows: the call is not a tail call */
    for (i = 0; i < 1; i++) {
        func_0010D750(&gIconWin->flash);
    }
}

/* Starts the opening animation. */
void IconWin_Open(void) {
    func_0010D878(&gIconWin->flash, "fl_in", 1);
}

/* Starts the closing animation. */
void IconWin_Close(void) {
    func_0010D878(&gIconWin->flash, "fl_out", 1);
}

/* Shows icon n: texture n + 1 of the icon resource. */
void IconWin_SetIcon(s32 icon) {
    if (gIconWin->icons != NULL) {
        gIconWin->tex[4] = gIconWin->icons->tex + icon * FLASH_TEX_SIZE + FLASH_TEX_SIZE;
    }
}
