#include "common.h"
#include "battle/btl_seq.h"
#include "battle/battle_setup.h"
#include "battle/battle_work.h"
#include "sys/heap.h"
#include "sys/adx.h"

/* ---- Skill-list text of the pause menu, 0x215420-0x216AC0 ----
 * The list is a UTF-16 script. Every line starts with two characters whose second one is a hex digit (a mask
 * tested against work->unk10), optionally followed by "&ddd" (unlock condition), then a tag:
 *   '$n'   page n title          '*abc' entry (three digits a, b, c)      '%0' / '%1' detail lines
 *   '#'    note                  '@'    end of the script
 * Lines are separated by func_002153E0(). BtlTextList / BtlTextWork are in battle/btl_seq.h.
 *
 * What the matching code says about the original source file (see also the note above BtlSeq_FirstState):
 * - BtlText_DrawPart (0x215420) was defined in the same file as, and above, BtlText_DrawList: two branches of
 *   DrawList are annulled / not annulled the right way only when the compiler has already compiled DrawPart.
 *   That is why this file starts at 0x215420 and not at 0x215540. The file may well have started earlier still.
 * - BtlText_IsLineDimmed was `static`: DrawList keeps the text pointer in a register across the call, which the
 *   compiler only does for a function it has marked `const` itself, and it only does that for static ones. */

extern f32 gBtlTextAlpha;
extern u64 *gSaveData; /* local view: only charaBits (+0xC10) is used here */

extern BtlTextWork *func_002145C0(void);
extern u16 *func_00214FE0(void);
extern u16 *func_00214FF0(void);
extern u16 *func_002153E0(u16 *line);        /* start of the next line */
extern void func_00215350(void *pkt, s32 x0, s32 y0, s32 x1, s32 y1); /* GS scissor */
extern void func_00215140(void *pkt, s32 x0, s32 y0, s32 x1, s32 y1, s32 u, s32 v, s32 w, s32 h, u32 color, s32 part);
extern s32 func_0020C9B8(s32 side);
extern s32 func_0023A458(void);
extern void func_0023A2D0(s32 font);
extern s32 func_0023A488(void);
extern f32 func_0023A5F0(u16 *str, s32 width);
extern void func_0023A848(s32 x, s32 y, u16 *str);
extern void func_0023AC60(void);
extern void func_0023AD48(void);
extern void func_0023AE48(f32 a);
extern void func_0023AE68(f32 sx, f32 sy);
extern void func_0023AE80(f32 *sx, f32 *sy);
extern void func_0023AED8(s32 x0, s32 y0, s32 x1, s32 y1);
extern void func_0023AF28(s32 align);
extern void func_0023AF48(s32 a);
extern void func_0023AF68(s32 a);
extern void func_0023AFE0(s32 r, s32 g, s32 b, s32 a);
extern void func_0023B010(s32 r, s32 g, s32 b, s32 a);
extern void func_0023B040(s32 r, s32 g, s32 b, s32 a);
extern s32 func_0023D1A0(u16 *str);
extern s32 Battle_IsSplitScreen(void);


/* Draws one part of the list sprite sheet: a w x h rectangle at (x, y); a negative h flips it vertically. */
void BtlText_DrawPart(void *pkt, s32 x, s32 y, s32 w, s32 h, s32 part) {
    func_00215140(pkt, x, y, x + w, y + h, 0, 0, w, h < 0 ? -h : h, 0x80808080, part);
}

/* Draws the icon of a page title: digit '0'..'8' picks part 7..14 ('7' and '8' share the last one). */
void BtlText_DrawPageIcon(void *pkt, s32 x, s32 y, u16 digit) {
    s32 w = func_0023A488();
    s32 idx = digit - '0';
    s32 part = 0;

    w *= 20;
    switch (idx) {
    case 0:
        part = 0;
        break;
    case 1:
        part = 1;
        break;
    case 2:
        part = 2;
        break;
    case 3:
        part = 3;
        break;
    case 4:
        part = 4;
        break;
    case 5:
        part = 5;
        break;
    case 6:
        part = 6;
        break;
    case 7:
    case 8:
        part = 7;
        break;
    }
    BtlText_DrawPart(pkt, x, y, w, 0x20, part + 7);
}
static s32 BtlText_IsLineDimmed(s32 page, s32 entry, s32 side);

/* Reads the mask digit of a line, steps over the two leading characters and tells if the line is shown. */
s32 BtlText_CheckLineMask(u16 **cursor, s32 side) {
    BtlTextWork *work = func_002145C0();
    u16 *p = *cursor;
    u32 mask = 0;
    s32 shift;

    switch (p[1]) {
    case 'F':
        mask = 0xF;
        break;
    case '1':
        mask = 1;
        break;
    case '2':
        mask = 2;
        break;
    case '4':
        mask = 4;
        break;
    case '8':
        mask = 8;
        break;
    case 'E':
        mask = 0xE;
        break;
    case 'D':
        mask = 0xD;
        break;
    case 'B':
        mask = 0xB;
        break;
    case '7':
        mask = 7;
        break;
    }
    p += 2;
    shift = work->unk10;
    *cursor = p;
    return (mask >> shift) & 1;
}

/* Steps over an "&ddd" unlock prefix; 0 when the line is locked (character ddd not unlocked in the save). */
s32 BtlText_CheckUnlock(u16 **cursor) {
    BtlTextWork *work = func_002145C0();
    s32 ret = 1;
    u16 *p = *cursor;

    if (p[0] == '&') {
        s32 id = (p[1] - '0') * 100;

        id += (p[2] - '0') * 10;
        id += p[3] - '0';

        p += 4;
        if (id == 0x56) {
            if (func_0020C9B8(work->side) != 0) {
                ret = 0;
            }
        } else {
            ret = (gSaveData[0xC10 / 8 + id / 64] >> (id % 64)) & 1;
        }
    }
    *cursor = p;
    return ret;
}

/* Stub: never dims a line. It has to be static: only then does the compiler see that it is a `const` function,
 * which BtlText_DrawList needs (p stays in a register across the call). */
static s32 BtlText_IsLineDimmed(s32 page, s32 entry, s32 side) {
    return 0;
}

/* A colour component scaled by the line alpha. */
#define BTLTEXT_COL(v) ((u8)(u32)(gBtlTextAlpha * (v)))

/* Draws the list inside the clip rectangle (x0, y0)-(x1, y1). mode 0: the page title and the entries of the
 * current page, scrolled, with the selected one highlighted; other modes: the detail lines ('%') and the note
 * ('#') of the selected entry.
 * Matching notes: the locals are declared in the order of their stack slots (most of them are spilled); the
 * three entry digits are an array; `line` is a register copy of p taken once per line. */
void BtlText_DrawList(void *pkt, s32 x0, s32 x1, s32 y0, s32 y1, s32 mode) {
    s32 icon[3];
    u16 *p;
    f32 sx;
    f32 sy;
    BtlTextWork *work;
    s32 side;
    s32 lineH;
    s32 go;
    s32 page;
    s32 entry;
    s32 y;
    BtlTextList *list;
    f32 fit;
    s32 tag;
    u16 *line;
    s32 h;

    work = func_002145C0();
    go = 1;
    page = -1;
    side = work->side;
    y = y0;
    entry = 0;
    list = &work->list[side];
    p = func_00214FE0();
    lineH = func_0023A488() * 20;
    if (p == NULL) {
        return;
    }
    if (mode == 0) {
        y -= list->scroll[list->page] * 28;
    }
    func_0023AC60();
    func_0023AF48(1);
    func_0023AE68(1.0f, 1.0f);
    Battle_IsSplitScreen();
    func_0023AF68(2);
    func_0023AFE0(0xFF, 0xFF, 0xFF, 0x80);
    func_0023B040(0, 0x10, 0x10, 0x40);
    if (mode == 0) {
        func_0023AED8(x0, y0, x1, y1);
        func_00215350(pkt, x0, y0, x1, y1);
    } else {
        func_0023AED8(x0, y0 - 7, x1, y1 + 7);
        func_00215350(pkt, x0, y0 - 7, x1, y1 + 7);
    }
    p += 1;
    while (go && *p != 0) {
        if (BtlText_CheckLineMask(&p, side) == 0) {
            p = func_002153E0(p);
            continue;
        }
        if (BtlText_CheckUnlock(&p) == 0) {
            gBtlTextAlpha = 0.5f;
        }
        line = p;
        tag = *line;
        switch (tag) {
        case '$':
            page = line[1] - '0';
            entry = -1;
            break;
        case '*':
            entry++;
            break;
        }
        if (tag != '@' && page != list->page) {
            p = func_002153E0(line);
            continue;
        }
        if (tag != '$' && BtlText_IsLineDimmed(page, entry, side) != 0) {
            gBtlTextAlpha = 0.5f;
        }
        if (mode == 0) {
            switch (tag) {
            case '$':
                func_0023AC60();
                func_00215350(pkt, 0, 0, 0x1FF, 0x1BF);
                func_0023AED8(0, 0, 0x1FF, 0x1BF);
                func_0023AFE0(0xFF, 0xFF, 0xFF, 0x80);
                func_0023B040(0x56, 0x3D, 0x39, 0x80);
                func_0023A848(x0 + 0x1E, y0 - 0x22, p + 2);
                BtlText_DrawPageIcon(pkt, x0, y0 - 0x28, p[1]);
                func_00215350(pkt, x0, y0, x1, y1);
                func_0023AD48();
                break;
            case '*':
                if (y0 < y + 0x20 && y < y1) {
                    icon[0] = p[1] - '0';
                    icon[1] = p[2] - '0';
                    icon[2] = p[3] - '0';
                    if (entry == list->cursor[list->page]) {
                        func_0023AFE0(BTLTEXT_COL(255.0f), BTLTEXT_COL(247.0f), BTLTEXT_COL(15.0f), 0x80);
                        func_0023B040(BTLTEXT_COL(28.0f), BTLTEXT_COL(32.0f), BTLTEXT_COL(21.0f), 0x40);
                    } else {
                        func_0023AFE0(BTLTEXT_COL(255.0f), BTLTEXT_COL(255.0f), BTLTEXT_COL(255.0f), 0x80);
                        func_0023B040(BTLTEXT_COL(32.0f), BTLTEXT_COL(43.0f), BTLTEXT_COL(94.0f), 0x40);
                    }
                    fit = func_0023A5F0(p + 2, (x1 - x0) - 0x44);
                    if (fit < 1.0f) {
                        func_0023AC60();
                        func_0023AE80(&sx, &sy);
                        func_0023AE68(sx * fit, sy);
                        func_0023A848(x0 + 0xF, y + 3, p + 4);
                        func_0023AD48();
                    } else {
                        func_0023A848(x0 + 0xF, y + 3, p + 4);
                    }
                    h = lineH - 2;
                    if (entry == list->cursor[list->page]) {
                        BtlText_DrawPart(pkt, x0 - 1, y, h, 0x1F, 0x11);
                    } else {
                        BtlText_DrawPart(pkt, x0 - 1, y, h, 0x1F, 0x10);
                    }
                    if (icon[0] > 0) {
                        BtlText_DrawPart(pkt, x1 - 0x35, y - 5, h, 0x1F, 0x13);
                    }
                    if (icon[1] > 0) {
                        BtlText_DrawPart(pkt, x1 - 0x20, y - 5, h, 0x1F, icon[1] + 0x1C);
                    }
                    if (icon[2] > 0) {
                        BtlText_DrawPart(pkt, x1 - 0x20, y - 5, h, 0x1F, icon[2] + 0x13);
                    }
                }
                y += 0x1C;
                break;
            case '@':
                go = 0;
                break;
            }
        } else {
            switch (tag) {
            case '%':
                switch (line[1]) {
                case '0':
                    if (entry != list->cursor[list->page]) {
                        break;
                    }
                    func_0023AFE0(BTLTEXT_COL(170.0f), BTLTEXT_COL(215.0f), BTLTEXT_COL(255.0f), 0x80);
                    func_0023B040(BTLTEXT_COL(43.0f), BTLTEXT_COL(54.0f), BTLTEXT_COL(64.0f), 0x40);
                    fit = func_0023A5F0(p + 2, (x1 - x0) - 0x1E);
                    if (fit < 1.0f) {
                        func_0023AC60();
                        func_0023AE80(&sx, &sy);
                        func_0023AE68(sx * fit, sy);
                        func_0023A848(x0 + 0xF, y + 3, p + 2);
                        func_0023AD48();
                    } else {
                        func_0023A848(x0 + 0xF, y + 3, p + 2);
                    }
                    y += 0x1C;
                    break;
                case '1':
                    if (entry != list->cursor[list->page]) {
                        break;
                    }
                    func_0023AFE0(BTLTEXT_COL(239.0f), BTLTEXT_COL(89.0f), BTLTEXT_COL(89.0f), 0x80);
                    func_0023B040(BTLTEXT_COL(64.0f), BTLTEXT_COL(17.0f), BTLTEXT_COL(0.0f), 0x40);
                    fit = func_0023A5F0(p + 2, (x1 - x0) - 0x1E);
                    if (fit < 1.0f) {
                        func_0023AC60();
                        func_0023AE80(&sx, &sy);
                        func_0023AE68(sx * fit, sy);
                        func_0023A848(x0 + 0xF, y + 3, p + 2);
                        func_0023AD48();
                    } else {
                        func_0023A848(x0 + 0xF, y + 3, p + 2);
                    }
                    y += 0x1C;
                    break;
                }
                break;
            case '#':
                if (entry == list->cursor[list->page]) {
                    func_0023B010(BTLTEXT_COL(128.0f), BTLTEXT_COL(128.0f), BTLTEXT_COL(128.0f), 0x80);
                    func_0023A848(x0 + 0xF, y, p + 1);
                    y += 0x1C;
                    func_0023B010(0x80, 0x80, 0x80, 0x80);
                    if (work->unk10 == 1) {
                        if (func_0023D1A0(p + 1) > 0x20) {
                            y += 0x14;
                        }
                    }
                }
                break;
            case '@':
                go = 0;
                break;
            }
        }
        gBtlTextAlpha = 1.0f;
        p = func_002153E0(p);
    }
    func_0023AD48();
    func_00215350(pkt, 0, 0, 0x1FF, 0x1BF);
}

/* Draws the scroll bar of the current page: the frame, and the thumb when there are more than 7 entries. */
#if 0
/* Not matching: 29 of 102 instructions differ, all from one register swap. The original keeps `half` in s2 and
 * `y1` in s3, this C gets them the other way round (and so schedules two instructions and the epilogue restores
 * in another order). Writing the last argument as `half += 0x1B; ... -half` gives the right registers but then
 * the add is done in place (`addiu s2,s2,27` instead of `addiu t0,s2,27`). */
void BtlText_DrawScrollBar(void *pkt, s32 unused, s32 x, s32 y0, s32 y1) {
    s32 visible = 7;
    BtlTextWork *work = func_002145C0();
    BtlTextList *list = &work->list[work->side];
    s32 h = y1 - y0;
    s32 half = h / 2;
    s32 count = list->count[list->page];
    s32 scroll = list->cursor[list->page];
    s32 over;

    BtlText_DrawPart(pkt, x, y0, 9, h - half, 5);
    BtlText_DrawPart(pkt, x, y1, 9, -half, 5);
    y0 += 2;
    x += 2;
    y1 -= 2;
    over = visible < count;
    count--;
    if (count < 0) {
        count = 0;
    }
    if (over) {
        f32 unit = (f32)(y1 - y0) / (f32)(count + 7);

        y0 += (s32)((f32)scroll * unit);
        y1 = y0 + (s32)(unit * (f32)visible);
        h = y1 - y0;
        half = h / 2;
        BtlText_DrawPart(pkt, x, y0 - 0x1B, 5, h - half + 0x1B, 6);
    }
    BtlText_DrawPart(pkt, x, y1 + 0x1B, 5, -(half + 0x1B), 6);
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/btl_seq", BtlText_DrawScrollBar);
#endif

/* Counts the pages and the entries of each page of the current side's list. */
void BtlText_CountEntries(void) {
    u16 *p;
    s32 go = 1;
    s32 page = -1;
    BtlTextWork *work = func_002145C0();
    s32 side = work->side;
    BtlTextList *list = &work->list[side];
    s32 i;

    p = func_00214FE0();
    if (p == NULL) {
        return;
    }
    list->pages = 0;
    for (i = 9; i >= 0; i--) {
        list->count[i] = 0;
    }
    p += 1;
    while (go && *p != 0) {
        s32 tag;

        if (BtlText_CheckLineMask(&p, side) == 0) {
            p = func_002153E0(p);
            continue;
        }
        BtlText_CheckUnlock(&p);
        tag = *p;
        if (tag == '$') {
            page = p[1] - '0';
        }
        switch (tag) {
        case '$':
            list->pages++;
            break;
        case '*':
            list->count[page]++;
            break;
        case '@':
            go = 0;
            break;
        }
        p = func_002153E0(p);
    }
}

/* Returns the name of the n-th entry of the second script (8 characters into its line), NULL when it has none.
 * The bare `return;` is what the original does when there is no script: v0 is left as the NULL just returned. */
u16 *BtlText_FindEntry(s32 n) {
    u16 *p;
    s32 go = 1;
    u16 *ret = NULL;


    p = func_00214FF0();
    if (p == NULL) {
        return;
    }
    p += 1;
    while (go && *p != 0) {
        p += 2;
        BtlText_CheckUnlock(&p);
        switch (*p) {
        case '*':
            if (n <= 0) {
                ret = p + 4;
                go = 0;
            }
            n--;
            break;
        case '@':
            go = 0;
            break;
        }
        p = func_002153E0(p);
    }
    return ret;
}

/* Draws the name of entry n at (x, y); align 0 / 1 / other picks the font alignment 0 / 2 / 1. */
void BtlText_DrawEntryName(s32 x, s32 y, s32 n, s32 align, f32 alpha) {
    u16 *str = BtlText_FindEntry(n);

    if (str != NULL) {
        s32 font = func_0023A458();

        func_0023AC60();
        func_0023AF48(1);
        func_0023AE48(0.9f);
        func_0023AE68(0.95f, 1.0f);
        func_0023AFE0(0xFF, 0xFF, 0xFF, (u8)(u32)(alpha * 128.0f));
        func_0023AF68(2);
        func_0023B040(0x20, 0x20, 0xFF, (u8)(u32)(alpha * 64.0f));
        switch (align) {
        case 0:
            func_0023AF28(0);
            break;
        case 1:
            func_0023AF28(2);
            break;
        default:
            func_0023AF28(1);
            break;
        }
        func_0023A848(x, y, str);
        func_0023AD48();
        func_0023A2D0(font);
    }
}

/* Battle sequence state machine, 0x216AC0-0x2187E0. See battle/btl_seq.h for the state list.
 *
 * Object boundaries, from .rodata: the tick table of BtlClock_Tick (0x2F1AD8) follows jtbl_002F1A80 of
 * BtlText_CheckLineMask with no padding, and the jump tables after it sit at +0x18 from it, so the text
 * functions and at least BtlClock_Tick were one object. With the file starting at BtlText_DrawPart (0x215420,
 * .rodata from jtbl_002F1A50) every table lands on its original address.
 *
 * Object boundaries, from code generation: a call to a function the compiler has already compiled in the same
 * file is treated differently by the delay-slot pass than a call to an unknown one (it looks through the call
 * when deciding whether a branch must be annulled). Two places depend on it:
 * - BtlText_DrawList needs BtlText_DrawPart known (same file, above it): matches as written.
 * - BtlSeq_CheckBattleEnd needs BtlSeq_TickClocks NOT known. So the state handlers and the win check
 *   (BtlSeqIntroTalk_Setup .. the end; their first jump table, 0x2F1AF0, is 16-byte aligned, which is where a
 *   new object's .rodata would start) were most likely a second source file, and this file should be split
 *   somewhere after BtlSeq_TickClocks (0x217090) and before BtlSeq_CheckBattleEnd (0x217EF0). Until then
 *   BtlSeq_CheckBattleEnd calls it through a second declaration of the same symbol. */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);

/* Local views of things owned by other modules (their headers are still moving). */
typedef struct BtlSeqObj {
    /* 0x000 */ u8 unk0[0xC];
    /* 0x00C */ s32 chara;  /* character id */
    /* 0x010 */ u8 unk10[0x93C - 0x10];
    /* 0x93C */ u8 *talkTbl; /* 4 bytes per opponent character id: {intro line or 0xFF, speaks second, win line or 0xFF, ?} */
} BtlSeqObj;

extern void BtlFacade_SetCtrl10D(s32 side);
extern BtlSeqObj *BtlObj_Get(s32 idx);
extern void BtlObj_SetSubState(BtlSeqObj *obj, s32 a, s32 line); /* mouth / talk animation */
extern void func_00267AC8(BtlSeqTimer *timer, f32 seconds, f32 from, f32 to);
extern s32 func_00267B00(BtlSeqTimer *timer); /* steps a timer, 1 when it ended */
extern void func_00209EE8(s32 side);        /* character flag 0xEF: entrance pose */
extern void func_00209F20(s32 side);        /* character flag 0xF0: end of entrance */
extern void func_00209F58(s32 side);        /* character flag 0xF1: win pose */
extern void func_00209F90(s32 side);        /* character flag 0xF2: lose pose */
extern s32 func_00209FC8(s32 side);         /* pose reached */
extern void DemoCam_PlayObjAnim(s32 side, s32 cut); /* fighter camera cut */
extern void DemoCam_SetScaleHeight(s32 a);
extern s32 DemoCam_PlayStageAnim(s32 cut);          /* stage camera cut */
extern s32 DemoCam_IsActive(void);             /* camera cut still playing */
extern s32 DemoCam_IsInUse(void);
extern void DemoCam_Stop(void);            /* stop the camera cut */
extern void func_00244870(void);
extern void func_00244830(s32 a, f32 seconds);
extern s32 func_002592D8(void);
extern void func_00259360(void);
extern u8 *func_00259528(void);
extern void func_0022AB50(s32 id);          /* HUD announcement */
extern s32 func_00207090(void);             /* any character has flag 0x128 */
extern s32 func_00207270(s32 side);
extern s32 func_0020B8F0(s32 side);         /* character flag 7 */
extern s32 func_0020B878(s32 side);         /* every character of the side has no health */
extern s32 func_0022FB90(void);
extern void func_0022FBB0(s32 pad);
extern s32 func_0022FBD8(void);
extern s32 func_0022FC20(void);
extern s32 func_00212FF8(s32 a, s32 b);     /* pause / result menu update */
extern void func_00213220(void);
extern void Snd_SetPause(s32 a, s32 b);
extern void func_00218A58(s32 on);          /* HUD visibility bits */
extern void func_00218AE8(s32 on);
extern void func_00218B08(s32 on);
extern void func_00218B30(s32 on);
extern void func_00218B58(s32 on);
extern void func_00218B80(s32 on);
extern void Fade_Start(s32 idx, s32 dir, f32 seconds);


s32 BtlSeqIntroTalk_Setup(BtlSeqTalkCtx *ctx);
s32 BtlSeqWinTalk_Setup(BtlSeqTalkCtx *ctx);
s32 BtlSeq_CanDraw(void);
void BtlSeq_JudgeByHealth(BattleResult *result);
void BtlSeq_SetResultPad(void);
s32 BtlSeq_IsModeZero(void);

/* Returns the first state of a table that has an enter handler (0 when none has). */
s32 BtlSeq_FirstState(BtlSeqState *table) {
    s32 i;

    for (i = 0; i < BTL_SEQ_STATE_COUNT; i++) {
        if (table[i].enter != NULL) {
            return i;
        }
    }
    return 0;
}

/* Calls one handler of a state with the state's poll callback loaded into the context; -1 for an empty slot. */
s32 BtlSeq_Call(BtlSeqFunc func, s32 state) {
    if (func == NULL) {
        return -1;
    }
    gBtlSeq->ctx.poll = gBtlSeq->table[state].poll;
    return func(&gBtlSeq->ctx);
}

/* Clears the sequence, picks the state table for the battle mode and enters its first state. */
s32 BtlSeq_Reset(void) {
    memset(gBtlSeq, 0, sizeof(BtlSeq));
    BtlSeq_ResetClocks();
    switch (Battle_GetMode()) {
    case 1:
        gBtlSeq->table = gBtlSeqTblMode1;
        break;
    case 5:
    case 6:
    case 7:
        gBtlSeq->table = gBtlSeqTblMode5to7;
        break;
    default:
        gBtlSeq->table = gBtlSeqTblDefault;
        break;
    }
    gBtlSeq->state = BtlSeq_FirstState(gBtlSeq->table);
    Voice_Stop(0);
    Voice_Stop(1);
    return BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].enter, gBtlSeq->state);
}

/* Allocates the sequence and starts it. */
s32 BtlSeq_Init(void) {
    gBtlSeq = Heap_Alloc(sizeof(BtlSeq), 0x20, 0, HEAP_ANY);
    memset(gBtlSeq, 0, sizeof(BtlSeq));
    return BtlSeq_Reset();
}

/* Frees the sequence. */
void BtlSeq_Term(void) {
    Heap_Free(gBtlSeq);
    gBtlSeq = NULL;
}

/* Runs the current state's preUpdate handler (before the battle simulation). */
s32 BtlSeq_PreUpdate(void) {
    return BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].preUpdate, gBtlSeq->state);
}

/* Runs the current state's update handler and performs the state change it asks for; 1 = leave the battle. */
s32 BtlSeq_Update(void) {
    s32 next = BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].update, gBtlSeq->state);

    if (next != gBtlSeq->state) {
        BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].exit, gBtlSeq->state);
        gBtlSeq->state = next;
        if (gBtlSeq->state == BTL_SEQ_EXIT) {
            if (Battle_IsRematchRequested() || (Battle_GetMode() == 6 && BattleResult_IsReasonBit2())) {
                Battle_GetWork()->flags |= BATTLE_FLAG_RESTART;
                return 0;
            }
            return 1;
        }
        if (gBtlSeq->table[gBtlSeq->state].enter == NULL) {
            gBtlSeq->state = BTL_SEQ_END;
        }
        BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].enter, gBtlSeq->state);
    }
    return 0;
}

/* Returns the battle clock. */
BtlClock *BtlSeq_GetClock(void) {
    return &gBtlSeq->clock;
}

/* Returns the second clock. */
BtlClock *BtlSeq_GetSubClock(void) {
    return &gBtlSeq->subClock;
}

/* Returns the current state. */
s32 BtlSeq_GetState(void) {
    return gBtlSeq->state;
}

/* True while the fight itself is running (state 3). */
s32 BtlSeq_IsFighting(void) {
    return BtlSeq_GetState() == BTL_SEQ_FIGHT;
}

/* True during the finish announcement (state 4). */
s32 BtlSeq_IsFinish(void) {
    return BtlSeq_GetState() == BTL_SEQ_FINISH;
}

/* Runs the current state's extra handler (mode 1: skip the talk). */
s32 BtlSeq_CallExtra(void) {
    return BtlSeq_Call(gBtlSeq->table[gBtlSeq->state].extra, gBtlSeq->state);
}

/* Turns the battle-end check (and the clocks) off or on. */
void BtlSeq_SetEndCheckOff(s32 off) {
    gBtlSeq->endCheckOff = off;
}

/* Returns whether the battle-end check is off. */
s32 BtlSeq_GetEndCheckOff(void) {
    return gBtlSeq->endCheckOff;
}

/* Zeroes a clock. */
void BtlClock_Clear(BtlClock *clock) {
    memset(clock, 0, sizeof(BtlClock));
}

/* Advances a clock by one tick (34, 32, 34 ms in turn), stopping at 9:59:59.999. */
s32 BtlClock_Tick(BtlClock *clock) {
    s32 tickMs[3] = { 34, 32, 34 };

    s32 add = tickMs[clock->ticks++ % 3];

    clock->ms += add;
    if (clock->hours >= 9 && clock->minutes >= 59 && clock->seconds >= 59 && clock->ms >= 999) {
        clock->hours = 9;
        clock->minutes = 59;
        clock->seconds = 59;
        clock->ms = 999;
    } else {
        if (clock->ms >= 1000) {
            clock->ms -= 1000;
            clock->seconds++;
        }
        if (clock->seconds >= 60) {
            clock->seconds -= 60;
            clock->minutes++;
        }
        if (clock->minutes >= 60) {
            clock->minutes -= 60;
            clock->hours++;
        }
    }
    return 1;
}

/* Clears both clocks and loads the time limit into the battle clock. */
void BtlSeq_ResetClocks(void) {
    BtlClock *clock = BtlSeq_GetClock();

    BtlClock_Clear(clock);
    clock->timeLeft = Battle_GetTimeLimit();
    BtlClock_Clear(BtlSeq_GetSubClock());
}

/* Ticks both clocks and updates the time left; 1 when the time limit is reached. */
s32 BtlSeq_TickClocks(void) {
    BtlClock *clock;

    BtlSeq_GetClock();
    BtlClock_Tick(BtlSeq_GetSubClock());
    clock = BtlSeq_GetClock();
    BtlClock_Tick(clock);
    if (Battle_IsTimeLimitOff()) {
        return 0;
    }
    if (clock->minutes * 60 + clock->seconds >= Battle_GetTimeLimit()) {
        clock->timeLeft = 0;
        return 1;
    }
    clock->timeLeft = Battle_GetTimeLimit() - (clock->minutes * 60 + clock->seconds);
    return 0;
}

/* Restarts the second clock. */
void BtlSeq_ResetSubClock(void) {
    BtlClock_Clear(BtlSeq_GetSubClock());
}

/* Seconds left of the time limit, -1 when there is none. */
s32 BtlSeq_GetTimeLeft(void) {
    if (Battle_IsTimeLimitOff()) {
        return -1;
    }
    return BtlSeq_GetClock()->timeLeft;
}

/* Stops both fighters' talk animation and both voice players. */
void BtlSeq_StopTalk(void) {
    BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(0)), 0, -1);
    BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(1)), 0, -1);
    Voice_Stop(0);
    Voice_Stop(1);
}

/* Chooses the two entrance lines: the pair's special dialogue when both have a table entry, else random. */
s32 BtlSeqIntroTalk_Setup(BtlSeqTalkCtx *ctx) {
    BtlSeqObj *obj0 = BtlObj_Get(BattleSide_GetObjId(0));
    u8 *tbl0 = obj0->talkTbl;
    BtlSeqObj *obj1 = BtlObj_Get(BattleSide_GetObjId(1));
    u8 *tbl1 = obj1->talkTbl;

    if (tbl0 != NULL && tbl1 != NULL) {
        if (tbl0[obj1->chara * 4] != 0xFF) {
            if (tbl0[obj1->chara * 4 + 1] == 0) {
                ctx->side[0] = 0;
                ctx->side[1] = 1;
                ctx->chara[0] = obj0->chara;
                ctx->chara[1] = obj1->chara;
                ctx->line[0] = tbl0[obj1->chara * 4] + 6;
                ctx->line[1] = tbl1[obj0->chara * 4] + 6;
            } else {
                ctx->side[1] = 0;
                ctx->side[0] = 1;
                ctx->chara[0] = obj1->chara;
                ctx->chara[1] = obj0->chara;
                ctx->line[0] = tbl1[obj0->chara * 4] + 6;
                ctx->line[1] = tbl0[obj1->chara * 4] + 6;
            }
        } else {
            ctx->side[0] = 0;
            ctx->side[1] = 1;
            ctx->chara[0] = obj0->chara;
            ctx->chara[1] = obj1->chara;
            ctx->line[0] = rand() % 2;
            ctx->line[1] = rand() % 2;
        }
    } else {
        ctx->side[0] = 0;
        ctx->side[1] = 1;
        ctx->chara[0] = obj0->chara;
        ctx->chara[1] = obj1->chara;
        ctx->line[0] = rand() % 2;
        ctx->line[1] = rand() % 2;
    }
    ctx->step = 0;
    func_00267AC8(&ctx->timer, 10.0f, 0.0f, 1.0f);
    return 1;
}

/* State 1 enter: prepares the entrance lines; mode 1 has none and waits for the skip request. */
s32 BtlSeqIntroTalk_Enter(BtlSeqTalkCtx *ctx) {
    if (Battle_GetMode() == 1) {
        BtlFacade_SetCtrl10D(0);
        BtlFacade_SetCtrl10D(1);
        ctx->skip = 0;
        ctx->step = 99;
    } else {
        BtlSeqIntroTalk_Setup(ctx);
    }
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    return 1;
}

/* State 1 preUpdate: at steps 0 and 2 starts a speaker's pose, camera cut, voice line and talk animation. */
s32 BtlSeqIntroTalk_PreUpdate(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
        func_00209EE8(ctx->side[0]);
        func_00267AC8(&ctx->timer, 10.0f, 0.0f, 1.0f);
        DemoCam_PlayObjAnim(ctx->side[0], 0);
        Voice_PlayChara(ctx->side[0], ctx->chara[0], ctx->line[0]);
        BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(ctx->side[0])), 2, ctx->line[0]);
        ctx->step++;
        break;
    case 2:
        func_00209EE8(ctx->side[1]);
        func_00267AC8(&ctx->timer, 10.0f, 0.0f, 1.0f);
        DemoCam_PlayObjAnim(ctx->side[1], 0);
        Voice_PlayChara(ctx->side[1], ctx->chara[1], ctx->line[1]);
        BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(ctx->side[1])), 2, ctx->line[1]);
        ctx->step++;
        break;
    case 3:
        break;
    }
    return 1;
}

/* State 1 update: waits for each line to end (voice stopped or 10 s), or for the skip; then goes to state 2. */
s32 BtlSeqIntroTalk_Update(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
    case 1:
        if (func_00209FC8(ctx->side[0])) {
            if (Voice_IsStopped(ctx->side[0])) {
                func_00244870();
                func_00244830(1, 1.0f);
                ctx->step++;
            } else if (func_00267B00(&ctx->timer)) {
                func_00244870();
                func_00244830(1, 1.0f);
                ctx->step++;
            }
        }
        break;
    case 2:
    case 3:
        if (func_00209FC8(ctx->side[1])) {
            if (Voice_IsStopped(ctx->side[1]) || func_00267B00(&ctx->timer)) {
                func_00244870();
                func_00244830(1, 1.0f);
                ctx->step++;
            }
        }
        break;
    case 4:
        return 2;
    case 99:
        if (ctx->skip) {
            func_00244870();
            func_00244830(1, 1.0f);
            goto done;
        }
        break;
    }
    if (ctx->poll != NULL) {
        if (ctx->poll()) {
            if (ctx->step != 4) {
                func_00244870();
                func_00244830(1, 1.0f);
                if (func_002592D8()) {
                    func_00259360();
                }
            }
done:
            BtlSeq_StopTalk();
            return 2;
        }
    }
    return 1;
}

/* State 1 exit: stops the camera cut and ends both entrance poses (not in mode 1). */
s32 BtlSeqIntroTalk_Exit(BtlSeqTalkCtx *ctx) {
    if (Battle_GetMode() != 1) {
        DemoCam_Stop();
        func_00209F20(ctx->side[0]);
        func_00209F20(ctx->side[1]);
    }
    return 1;
}

/* State 1 extra (mode 1): asks the state to end. */
s32 BtlSeqIntroTalk_Skip(BtlSeqTalkCtx *ctx) {
    s32 mode = Battle_GetMode();

    if (mode == 1) {
        ctx->skip = mode;
    }
    return 1;
}

/* Chooses the speaker, line and announcement of the winner scene. */
s32 BtlSeqWinTalk_Setup(BtlSeqTalkCtx *ctx) {
    s32 winner = BattleResult_GetWinnerSide();
    s32 loser = winner == 0;
    BtlSeqObj *winObj = BtlObj_Get(BattleSide_GetObjId(winner));
    u8 *winTbl = winObj->talkTbl;
    BtlSeqObj *loseObj = BtlObj_Get(BattleSide_GetObjId(loser));
    u8 *loseTbl = loseObj->talkTbl;

    if (BattleResult_IsPlayerWin()) {
        if (winTbl == NULL || loseTbl == NULL || func_00207270(winner) || func_00207270(loser)) {
            ctx->side[0] = winner;
            if (func_00207270(winner)) {
                ctx->chara[0] = 0x56;
            } else {
                ctx->chara[0] = winObj->chara;
            }
            ctx->line[0] = rand() % 2 + 3;
            ctx->step = 0;
        } else {
            if (winTbl[loseObj->chara * 4 + 2] != 0xFF) {
                ctx->side[0] = winner;
                ctx->chara[0] = winObj->chara;
                ctx->line[0] = winTbl[loseObj->chara * 4 + 2] + 0x1E;
                ctx->step = 0;
            } else {
                ctx->side[0] = winner;
                ctx->chara[0] = winObj->chara;
                ctx->line[0] = rand() % 2 + 3;
                ctx->step = 0;
            }
        }
        func_0022AB50(6);
    } else {
        ctx->side[0] = loser;
        if (func_00207270(loser)) {
            ctx->chara[0] = 0x56;
        } else {
            ctx->chara[0] = loseObj->chara;
        }
        ctx->step = 0;
        ctx->line[0] = 0x2A;
        func_0022AB50(7);
    }
    func_00267AC8(&ctx->timer, 10.0f, 0.0f, 1.0f);
    return 1;
}

/* State 5 enter: prepares the winner scene and hides HUD parts; mode 1 can be told to wait for the skip instead. */
s32 BtlSeqWinTalk_Enter(BtlSeqTalkCtx *ctx) {
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    func_00244870();
    func_00244830(1, 1.0f);
    if (Battle_GetMode() == 1) {
        if (*func_00259528() & 0x10) {
            ctx->skip = 0;
            ctx->step = 99;
        } else {
            BtlSeqWinTalk_Setup(ctx);
        }
        func_00218AE8(0);
        func_00218B08(0);
        func_00218B58(0);
        func_00218B80(0);
        func_00218B30(0);
    } else {
        BtlSeqWinTalk_Setup(ctx);
        func_00218AE8(0);
        func_00218B08(0);
        func_00218B58(0);
        func_00218B80(0);
    }
    return 1;
}

/* State 5 preUpdate: step 0 starts the pose, camera cut and voice line; step 2 starts a 1.5 s hold. */
s32 BtlSeqWinTalk_PreUpdate(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
        if (BattleResult_IsPlayerWin()) {
            func_00209F58(ctx->side[0]);
            DemoCam_PlayObjAnim(ctx->side[0], 1);
            DemoCam_SetScaleHeight(1);
        } else {
            func_00209F90(ctx->side[0]);
            DemoCam_PlayObjAnim(ctx->side[0], 2);
        }
        func_00267AC8(&ctx->timer, 10.0f, 0.0f, 1.0f);
        Voice_PlayChara(ctx->side[0], ctx->chara[0], ctx->line[0]);
        BtlObj_SetSubState(BtlObj_Get(BattleSide_GetObjId(ctx->side[0])), 2, ctx->line[0]);
        ctx->step++;
        break;
    case 2:
        func_00267AC8(&ctx->timer, 1.5f, 0.0f, 1.0f);
        ctx->step++;
        break;
    case 3:
        break;
    }
    return 5;
}

/* State 5 update: waits for the line (or 10 s), holds 1.5 s, then goes to state 6; the poll callback skips. */
s32 BtlSeqWinTalk_Update(BtlSeqTalkCtx *ctx) {
    switch (ctx->step) {
    case 0:
    case 1:
        if (func_00209FC8(ctx->side[0])) {
            if (Voice_IsStopped(ctx->side[0]) || func_00267B00(&ctx->timer)) {
                ctx->step = 2;
            }
        }
        break;
    case 2:
    case 3:
        if (func_00267B00(&ctx->timer)) {
            ctx->step = 4;
        }
        break;
    case 4:
        return 6;
    case 99:
        if (ctx->skip) {
            goto done;
        }
        break;
    }
    if (ctx->poll != NULL) {
        if (ctx->poll()) {
done:
            BtlSeq_StopTalk();
            return 6;
        }
    }
    return 5;
}

/* State 5 exit: stops the talk (not in mode 1). */
s32 BtlSeqWinTalk_Exit(BtlSeqTalkCtx *ctx) {
    if (Battle_GetMode() != 1) {
        BtlSeq_StopTalk();
    }
    return 1;
}

/* State 5 extra (mode 1): asks the state to end. */
s32 BtlSeqWinTalk_Skip(BtlSeqTalkCtx *ctx) {
    s32 mode = Battle_GetMode();

    if (mode == 1) {
        ctx->skip = mode;
    }
    return 1;
}

/* State 0 enter: restarts the step counter and holds the fighters. */
s32 BtlSeqStageIntro_Enter(BtlSeqWaitCtx *ctx) {
    ctx->step = 0;
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    return 1;
}

/* State 0 preUpdate: at steps 0, 2 and 4 starts stage camera cut 0, 1 and 2. */
s32 BtlSeqStageIntro_PreUpdate(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
        DemoCam_PlayStageAnim(0);
        ctx->step++;
        break;
    case 2:
        DemoCam_PlayStageAnim(1);
        ctx->step++;
        break;
    case 4:
        DemoCam_PlayStageAnim(2);
        ctx->step++;
        break;
    case 1:
    case 3:
    case 5:
    case 6:
        break;
    }
    return 0;
}

/* State 0 update: steps on when a camera cut ends; goes to state 1 after the third or on the poll callback. */
s32 BtlSeqStageIntro_Update(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
    case 1:
    case 2:
    case 3:
        if (!DemoCam_IsActive()) {
            func_00244870();
            func_00244830(1, 1.0f);
            ctx->step++;
        }
        break;
    case 4:
    case 5:
        if (!DemoCam_IsActive()) {
            ctx->step++;
            return 1;
        }
        break;
    case 6:
        return 1;
    }
    if (ctx->poll != NULL) {
        if (ctx->poll()) {
            return 1;
        }
    }
    return 0;
}

/* State 0 exit: stops a camera cut that is still playing. */
s32 BtlSeqStageIntro_Exit(BtlSeqWaitCtx *ctx) {
    if (DemoCam_IsActive()) {
        DemoCam_Stop();
    }
    func_00244870();
    func_00244830(1, 1.0f);
    return 1;
}

/* True when the battle ended by time up in mode 0: the only case where equal health is a draw. */
s32 BtlSeq_CanDraw(void) {
    if (BattleResult_IsTimeUp() && Battle_GetMode() == 0) {
        return 1;
    }
    return 0;
}

/* Picks the winner from the two sides' health; equal health falls back to a rule flag, a draw, or rand(). */
void BtlSeq_JudgeByHealth(BattleResult *result) {
    if (result->health[0] == result->health[1]) {
        if (BattleResult_IsEvent3CSet(0)) {
            result->winner = BTL_RESULT_WIN_P1;
        } else if (BattleResult_IsEvent3CSet(1)) {
            result->winner = BTL_RESULT_WIN_P2;
        } else if (BtlSeq_CanDraw()) {
            result->winner = BTL_RESULT_DRAW;
        } else if (Battle_GetMode() == 8) {
            result->winner = BTL_RESULT_WIN_P2;
        } else if (rand() & 1) {
            result->winner = BTL_RESULT_WIN_P1;
        } else {
            result->winner = BTL_RESULT_WIN_P2;
        }
    } else if (result->health[1] < result->health[0]) {
        result->winner = BTL_RESULT_WIN_P1;
    } else {
        result->winner = BTL_RESULT_WIN_P2;
    }
}

/* Ticks the clocks and decides whether the battle is over, filling the result block; 1 when it is.
 *
 * Two things were needed to match:
 * - The function has to END with the last success path (`return 1` falling off the end) and every `return 0`
 *   has to be an early return. Then the compiler shares only the final `return 1` between the blocks and keeps
 *   the three "set reason + BtlSeq_JudgeByHealth" blocks separate; with `return 0` as the last statement it
 *   merges those three blocks into one.
 * - The call to BtlSeq_TickClocks() must look like a call to a function the compiler has not seen the body of
 *   (the branch before it is `bnezl` only then). In the original this function was therefore compiled without
 *   BtlSeq_TickClocks() defined above it, i.e. the clock code was most likely a separate source file. Until the
 *   file is split, the call goes through a second declaration of the same symbol. */
extern s32 BtlSeq_TickClocksExt(void) __asm__("BtlSeq_TickClocks");

s32 BtlSeq_CheckBattleEnd(void) {
    BattleResult *result = Battle_GetResult();
    s32 timeUp;

    if (result->winner & 0x1F) {
        return 1;
    }
    if (BtlSeq_GetEndCheckOff()) {
        return 0;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return 0;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return 0;
    }
    if (func_00207090()) {
        return 0;
    }
    timeUp = BtlSeq_TickClocksExt();
    if (Battle_GetMode() == 1) {
        return 0;
    }
    if (timeUp) {
        result->reason = BTL_REASON_TIME_UP;
        BtlSeq_JudgeByHealth(result);
        return 1;
    }
    if (func_0020B8F0(0) && func_0020B8F0(1)) {
        result->reason = BTL_REASON_FLAG7;
        BtlSeq_JudgeByHealth(result);
        return 1;
    }
    if (func_0020B8F0(1)) {
        result->winner = BTL_RESULT_WIN_P1;
        result->reason = BTL_REASON_FLAG7;
        return 1;
    }
    if (func_0020B8F0(0)) {
        result->winner = BTL_RESULT_WIN_P2;
        result->reason = BTL_REASON_FLAG7;
        return 1;
    }
    if (func_0020B878(0) && func_0020B878(1)) {
        result->reason = BTL_REASON_KO;
        BtlSeq_JudgeByHealth(result);
        return 1;
    }
    if (func_0020B878(1)) {
        result->winner = BTL_RESULT_WIN_P1;
        result->reason = BTL_REASON_KO;
        return 1;
    }
    if (func_0020B878(0)) {
        result->winner = BTL_RESULT_WIN_P2;
        result->reason = BTL_REASON_KO;
        return 1;
    }
    if (!BattleReplay_IsActive()) {
        return 0;
    }
    if (!BattleReplay_TestDataFlag()) {
        return 0;
    }
    result->winner = BTL_RESULT_OTHER;
    result->reason = BTL_REASON_BIT18;
    return 1;
}

/* State 3 enter: releases the fighters (clears the demo and ready flags); mode 8 shows the HUD. */
s32 BtlSeqFight_Enter(BtlSeqWaitCtx *ctx) {
    Battle_GetWork()->flags &= ~BATTLE_FLAG_DEMO;
    Battle_GetWork()->flags &= ~BATTLE_FLAG_READY;
    if (Battle_GetMode() == 8) {
        func_00218A58(1);
    }
    return 1;
}

/* State 3 preUpdate: nothing. */
s32 BtlSeqFight_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 3;
}

/* State 3 update: opens / runs the pause menu, then checks for the end of the battle (-> 4, or 6 without a finish scene). */
s32 BtlSeqFight_Update(BtlSeqWaitCtx *ctx) {
    if (Battle_GetMode() != 8) {
        if (ctx->poll != NULL && !(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE_MENU) && ctx->poll()) {
            if (Battle_GetMode() == 7) {
                BattleResult_Set(BTL_RESULT_ABORT, 0);
            } else if (BattleReplay_IsActive() && BattleReplay_IsLoaded() && func_0022FC20() == 1) {
            } else if (Battle_GetMode() == 1 && func_002592D8()) {
                func_00259360();
            } else {
                Battle_GetWork()->flags |= BATTLE_FLAG_PAUSE_MENU | BATTLE_FLAG_PAUSE;
                Adx_PauseSeVoice();
                Snd_SetPause(4, 1);
            }
        }
        if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE_MENU) {
            if (func_00212FF8(func_0022FBD8(), 0) == 0) {
                Battle_GetWork()->flags &= ~(BATTLE_FLAG_PAUSE_MENU | BATTLE_FLAG_PAUSE);
                Snd_SetPause(4, 0);
            }
        }
    }
    if (BtlSeq_CheckBattleEnd()) {
        if (Battle_GetMode() == 7) {
            return 6;
        }
        return BattleResult_IsAborted() ? 6 : 4;
    }
    return 3;
}

/* State 3 exit: nothing. */
s32 BtlSeqFight_Exit(BtlSeqWaitCtx *ctx) {
    return 1;
}

/* State 2 enter: holds the fighters, starts the 0.8 s wait and shows the HUD. */
s32 BtlSeqReady_Enter(BtlSeqWaitCtx *ctx) {
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    ctx->step = 0;
    func_00267AC8(&ctx->timer, 0.8f, 0.0f, 1.0f);
    func_00218A58(1);
    return 1;
}

/* State 2 preUpdate: nothing. */
s32 BtlSeqReady_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 2;
}

/* State 2 update: after 0.8 s shows announcement 0 and releases the fighters; 2 s later goes to state 3. */
s32 BtlSeqReady_Update(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
        if (func_00267B00(&ctx->timer)) {
            func_0022AB50(0);
            Battle_GetWork()->flags &= ~BATTLE_FLAG_DEMO;
            Battle_GetWork()->flags |= BATTLE_FLAG_READY;
            func_00267AC8(&ctx->timer, 2.0f, 0.0f, 1.0f);
            ctx->step++;
        }
        break;
    case 1:
        if (func_00267B00(&ctx->timer)) {
            return 3;
        }
        break;
    }
    return 2;
}

/* State 2 exit: announcement 1. */
s32 BtlSeqReady_Exit(BtlSeqWaitCtx *ctx) {
    func_0022AB50(1);
    return 1;
}

/* Gives the result menu to pad 0 when one pad plays, else to the winning side. */
void BtlSeq_SetResultPad(void) {
    if (func_0022FB90() == 1) {
        func_0022FBB0(0);
    } else {
        func_0022FBB0(BattleResult_GetWinnerSide());
    }
}

/* True in battle mode 0. */
s32 BtlSeq_IsModeZero(void) {
    return Battle_GetMode() == 0;
}

/* State 6 enter: mode 0 with a finished battle opens the result menu (step 0); otherwise starts the 1.2 s fade out (step 1). */
s32 BtlSeqEnd_Enter(BtlSeqWaitCtx *ctx) {
    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    if (BattleResult_IsAborted()) {
        Fade_Start(0, 0, 1.0f);
        func_00267AC8(&ctx->timer, 1.2f, 0.0f, 1.0f);
        ctx->step = 1;
    } else if (BtlSeq_IsModeZero()) {
        ctx->step = 0;
        BtlSeq_SetResultPad();
    } else {
        Fade_Start(0, 0, 1.0f);
        func_00267AC8(&ctx->timer, 1.2f, 0.0f, 1.0f);
        ctx->step = 1;
    }
    return 1;
}

/* State 6 preUpdate: nothing. */
s32 BtlSeqEnd_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 6;
}

/* State 6 update: step 0 runs the result menu until it aborts the battle, step 1 waits for the fade and returns 99. */
s32 BtlSeqEnd_Update(BtlSeqWaitCtx *ctx) {
    switch (ctx->step) {
    case 0:
        func_00212FF8(func_0022FBD8(), 1);
        func_00213220();
        if (BattleResult_IsAborted()) {
            Fade_Start(0, 0, 1.0f);
            func_00267AC8(&ctx->timer, 1.2f, 0.0f, 1.0f);
            ctx->step = 1;
        }
        break;
    case 1:
        if (func_00267B00(&ctx->timer)) {
            return BTL_SEQ_EXIT;
        }
        break;
    }
    return 6;
}

/* State 6 exit: stops the camera cut. */
s32 BtlSeqEnd_Exit(BtlSeqWaitCtx *ctx) {
    if (DemoCam_IsInUse()) {
        DemoCam_Stop();
    }
    DemoCam_SetScaleHeight(0);
    return 1;
}

/* State 4 enter: holds the fighters, starts the 3.5 s wait and shows the announcement for the finish reason. */
s32 BtlSeqFinish_Enter(BtlSeqWaitCtx *ctx) {
    BtlSeqTimer *timer;

    Battle_GetWork()->flags |= BATTLE_FLAG_DEMO;
    timer = &ctx->timer;
    func_00267AC8(timer, 3.5f, 0.0f, 1.0f);
    if (BattleResult_IsKo()) {
        if (BattleResult_IsWinnerEvent59Clear()) {
            func_0022AB50(3);
        } else {
            func_0022AB50(2);
        }
    } else if (BattleResult_IsTimeUp()) {
        func_0022AB50(5);
    } else if (BattleResult_IsReasonBit2()) {
        func_0022AB50(4);
    } else if (BattleResult_IsReasonBit18()) {
        func_00267AC8(timer, 1.1f, 0.0f, 1.0f);
    }
    return 1;
}

/* State 4 preUpdate: nothing. */
s32 BtlSeqFinish_PreUpdate(BtlSeqWaitCtx *ctx) {
    return 4;
}

/* State 4 update: when the wait ends goes to the winner scene (5) if a side won and the mode is not 8, else to 6. */
s32 BtlSeqFinish_Update(BtlSeqWaitCtx *ctx) {
    if (func_00267B00(&ctx->timer)) {
        if (Battle_GetMode() != 8 && BattleResult_HasWinner()) {
            return 5;
        }
        return 6;
    }
    return 4;
}

/* State 4 exit: nothing. */
s32 BtlSeqFinish_Exit(BtlSeqWaitCtx *ctx) {
    return 1;
}
