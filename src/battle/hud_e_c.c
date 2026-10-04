#include "common.h"
#include "battle/hud_e.h"
#include "battle/battle.h"
#include "sys/heap.h"

/*
 * Battle HUD, timer part, 0x22EC08-0x22F998: the clock at the top of the screen (or mode 3's counter).
 * Two nodes over six sprites of sheet 0. Hud_PreUpdate hands over the value every frame (HudTimer_SetTime /
 * HudTimer_SetCount); the node update lays the digits out. The pulse and the slide are frozen by the pause flag.
 */

#define HUDT_PAUSED() (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)

/* Sprite / node library at 0x224B50.. (neighbouring ranges). */
extern void func_00224B90(HudESprite *spr, s32 show);
extern void func_00224BD0(HudESprite *spr, s32 x0, s32 x1, s32 y0, s32 y1); /* screen rectangle */
extern void func_00224BE8(HudESprite *spr, s32 u0, s32 u1, s32 v0, s32 v1); /* texel rectangle */
extern void func_00224C00(HudESprite *spr, s32 tex, s32 sub);               /* texture entry and sub entry */
extern void func_00224CA0(HudESprite *spr, s32 dx, s32 dy);                 /* moves the rectangle */
extern void func_00224D40(HudESprite *spr, f32 sx, f32 sy);                 /* scales the rectangle */
extern void func_00224DB8(HudESprite *spr);                                 /* centres the rectangle */
extern void func_00224E20(HudESprite *spr, HudERes *res, s32 tex, s32 sub); /* sprite of a sheet's texture */
extern void func_00226488(HudESprite *spr, HudERes *res, s32 additive);     /* HudSprite_Draw */
extern void func_002264C8(HudENode *node, s32 x, s32 y);                    /* HudNode_SetPos */
extern void *memset(void *dst, s32 c, u32 n);

HudTimer *gHudTimer = NULL;

/* Root node update: steps the slide and moves the part up by 224 * slide. */
void HudTimer_UpdateRoot(HudENode *node) {
    if (!HUDT_PAUSED()) {
        Ramp_Step(&gHudTimer->slide);
    }
    func_002264C8(node, gHudTimer->slide.value * 0.0f, gHudTimer->slide.value * -224.0f);
}

/* Texel rectangle of digit d: 32x32 cells, four per row. */
#define HUDT_DIGIT_UV(spr, d) func_00224BE8(spr, ((d) % 4) * 32, ((d) % 4) * 32 + 32, ((d) / 4) * 32, ((d) / 4) * 32 + 32)

/* Clock node update: lays out up to three digits 11 pixels apart, centred (a negative value shows the two halves
   of the "infinite" picture). A clock is drawn with sub texture 1 below 10 seconds and 2 below 4, where each
   change of the value also starts a 0.5 s pulse of the digits' size (1 + pulse, about the point 16 below the
   baseline). */
void HudTimer_UpdateDigits(void) {
    s32 x = 0;
    Ramp *pulse = &gHudTimer->pulse;
    s32 value = gHudTimer->value;
    HudESprite *spr;
    s32 d;
    s32 w;

    if (value >= 1000) {
        value = 999;
    }
    if (value < 0) {
        spr = &gHudTimer->spr[1];
        func_00224B90(spr, 1);
        func_00224BD0(spr, 0, 32, -32, 0);
        func_00224BE8(spr, 96, 128, 64, 96);
        func_00224CA0(spr, 1, 0);
        spr = &gHudTimer->spr[2];
        func_00224B90(spr, 1);
        func_00224BD0(spr, -32, 0, -32, 0);
        func_00224BE8(spr, 64, 96, 64, 96);
        func_00224CA0(spr, 1, 0);
        func_00224B90(&gHudTimer->spr[3], 0);
    } else {
        spr = &gHudTimer->spr[3];
        {
            s32 hundred = 100;

            d = value / hundred;
        }
        if (d == 0) {
            func_00224B90(spr, 0);
        } else {
            func_00224B90(spr, 1);
            func_00224BD0(spr, 0, 32, -32, 0);
            HUDT_DIGIT_UV(spr, d);
            x = 11;
        }
        spr = &gHudTimer->spr[2];
        {
            s32 hundred = 100;
            s32 ten = 10;

            d = value % hundred / ten;
        }
        if (x == 0 && d == 0) {
            func_00224B90(spr, 0);
        } else {
            func_00224B90(spr, 1);
            func_00224BD0(spr, x, x + 32, -32, 0);
            HUDT_DIGIT_UV(spr, d);
            x += 11;
        }
        spr = &gHudTimer->spr[1];
        {
            s32 ten = 10;

            d = value % ten;
        }
        func_00224B90(spr, 1);
        func_00224BD0(spr, x, x + 32, -32, 0);
        HUDT_DIGIT_UV(spr, d);
        func_00224CA0(&gHudTimer->spr[1], w = -(x + 32) / 2, 0);
        func_00224CA0(&gHudTimer->spr[2], w, 0);
        func_00224CA0(&gHudTimer->spr[3], w, 0);
    }
    if (gHudTimer->isClock) {
        if ((u32)value < 4) {
            s32 ofs[2] = { 0, 16 };
            f32 scale;

            if (value != gHudTimer->prev) {
                Ramp_Start(pulse, 0.5f, 1.0f, 0.0f);
            }
            scale = pulse->value + 1.0f;
            spr = &gHudTimer->spr[1];
            func_00224C00(spr, 0, 2);
            func_00224CA0(spr, ofs[0], ofs[1]);
            func_00224D40(spr, scale, scale);
            func_00224CA0(spr, -ofs[0], -ofs[1]);
            spr = &gHudTimer->spr[2];
            func_00224C00(spr, 0, 2);
            func_00224CA0(spr, ofs[0], ofs[1]);
            func_00224D40(spr, scale, scale);
            func_00224CA0(spr, -ofs[0], -ofs[1]);
            spr = &gHudTimer->spr[3];
            func_00224C00(spr, 0, 2);
            func_00224CA0(spr, ofs[0], ofs[1]);
            func_00224D40(spr, scale, scale);
            func_00224CA0(spr, -ofs[0], -ofs[1]);
            if (!HUDT_PAUSED()) {
                Ramp_Step(pulse);
            }
        } else if ((u32)value < 10) {
            func_00224C00(&gHudTimer->spr[1], 0, 1);
            func_00224C00(&gHudTimer->spr[2], 0, 1);
            func_00224C00(&gHudTimer->spr[3], 0, 1);
        } else {
            func_00224C00(&gHudTimer->spr[1], 0, 0);
            func_00224C00(&gHudTimer->spr[2], 0, 0);
            func_00224C00(&gHudTimer->spr[3], 0, 0);
        }
    } else {
        func_00224C00(&gHudTimer->spr[1], 0, 0);
        func_00224C00(&gHudTimer->spr[2], 0, 0);
        func_00224C00(&gHudTimer->spr[3], 0, 0);
    }
    func_00224CA0(&gHudTimer->spr[1], -2, 4);
    func_00224CA0(&gHudTimer->spr[2], -2, 4);
    func_00224CA0(&gHudTimer->spr[3], -2, 4);
    func_00224B90(&gHudTimer->spr[5], gHudTimer->mark);
}

/* Draw callback: every sprite of the node. */
void HudTimer_Draw(HudENode *node) {
    u32 i;

    for (i = 0; i < node->sprCount; i++) {
        func_00226488(node->sprList[i], gHudTimer->res, 0);
    }
}

/* Slides the part off the screen over `seconds`. */
void HudTimer_SlideOut(f32 seconds) {
    Ramp_Start(&gHudTimer->slide, seconds, 0.0f, 1.0f);
}

/* Slides it back. */
void HudTimer_SlideIn(f32 seconds) {
    Ramp_Start(&gHudTimer->slide, seconds, 1.0f, 0.0f);
}

/* Builds the part: the work, six sprites and two nodes (root, and the clock at (256, 37) with all six sprites). */
void HudTimer_Init(HudENode **out, HudERes *res) {
    HudESprite *spr;
    HudENode *node;

    gHudTimer = Heap_Alloc(sizeof(HudTimer), 0x20, 0, 2);
    memset(gHudTimer, 0, sizeof(HudTimer));
    gHudTimer->spr = Heap_Alloc(6 * sizeof(HudESprite), 0x20, 0, 2);
    memset(gHudTimer->spr, 0, 6 * sizeof(HudESprite));
    gHudTimer->node = Heap_Alloc(2 * sizeof(HudENode), 0x20, 0, 2);
    memset(gHudTimer->node, 0, 2 * sizeof(HudENode));
    gHudTimer->res = res;

    spr = &gHudTimer->spr[0];
    func_00224E20(spr, res, 3, 0);
    func_00224BD0(spr, 0, 64, 0, 30);
    func_00224BE8(spr, 0, 64, 0, 30);
    func_00224DB8(spr);
    func_00224CA0(spr, 0, -7);

    spr = &gHudTimer->spr[1];
    func_00224E20(spr, res, 0, 0);
    func_00224BD0(spr, -10, 22, -16, 16);
    func_00224BE8(spr, 0, 32, 0, 32);

    spr = &gHudTimer->spr[5];
    func_00224E20(spr, res, 5, 0);
    func_00224DB8(spr);
    func_00224CA0(spr, 0, 16);

    spr = &gHudTimer->spr[2];
    func_00224E20(spr, res, 0, 0);
    func_00224BD0(spr, -22, 10, -16, 16);
    func_00224BE8(spr, 0, 32, 0, 32);

    spr = &gHudTimer->spr[3];
    func_00224E20(spr, res, 0, 0);
    func_00224BD0(spr, -22, 10, -16, 16);
    func_00224BE8(spr, 0, 32, 0, 32);

    spr = &gHudTimer->spr[4];
    func_00224E20(spr, res, 4, 0);
    func_00224DB8(spr);
    func_00224B90(spr, 0);
    func_00224CA0(spr, 0, 26);

    node = &gHudTimer->node[1];
    node->x = 256;
    node->y = 37;
    node->sprCount = 6;
    node->sprList = Heap_Alloc(node->sprCount * sizeof(HudESprite *), 0x20, 0, 2);
    memset(node->sprList, 0, node->sprCount * sizeof(HudESprite *));
    node->sprList[0] = &gHudTimer->spr[0];
    node->sprList[1] = &gHudTimer->spr[1];
    node->sprList[2] = &gHudTimer->spr[2];
    node->sprList[3] = &gHudTimer->spr[3];
    node->sprList[4] = &gHudTimer->spr[4];
    node->sprList[5] = &gHudTimer->spr[5];
    node->update = HudTimer_UpdateDigits;
    node->draw = HudTimer_Draw;

    node = &gHudTimer->node[0];
    node->x = 0;
    node->y = 0;
    node->childCount = 1;
    node->childList = Heap_Alloc(node->childCount * sizeof(HudENode *), 0x20, 0, 2);
    memset(node->childList, 0, node->childCount * sizeof(HudENode *));
    node->childList[0] = &gHudTimer->node[1];
    node->update = HudTimer_UpdateRoot;
    node->draw = NULL;

    *out = &gHudTimer->node[0];
}

/* Frees the part. */
void HudTimer_Term(void) {
    s32 i;

    if (gHudTimer->spr != NULL) {
        Heap_Free(gHudTimer->spr);
    }
    for (i = 0; i < 2; i++) {
        if (gHudTimer->node[i].sprList != NULL) {
            Heap_Free(gHudTimer->node[i].sprList);
        }
        if (gHudTimer->node[i].childList != NULL) {
            Heap_Free(gHudTimer->node[i].childList);
        }
    }
    if (gHudTimer->node != NULL) {
        Heap_Free(gHudTimer->node);
    }
    if (gHudTimer != NULL) {
        Heap_Free(gHudTimer);
    }
}

/* Sets the seconds left and shows the clock's frame (64x30 at the top of texture 3); hides the counter's label. */
void HudTimer_SetTime(s32 seconds) {
    HudESprite *spr;

    gHudTimer->prev = gHudTimer->value;
    gHudTimer->value = seconds;
    gHudTimer->isClock = 1;
    spr = &gHudTimer->spr[0];
    func_00224BD0(spr, 0, 64, 0, 30);
    func_00224BE8(spr, 0, 64, 0, 30);
    func_00224DB8(spr);
    func_00224CA0(spr, 0, -7);
    func_00224B90(&gHudTimer->spr[4], 0);
}

/* Sets a count shown in place of the clock (mode 3: opponents left): the other frame (64x32 at v = 32) and the
   label. */
void HudTimer_SetCount(s32 count) {
    HudESprite *spr;

    gHudTimer->prev = count;
    gHudTimer->value = count;
    gHudTimer->isClock = 0;
    spr = &gHudTimer->spr[0];
    func_00224BD0(spr, 0, 64, 0, 32);
    func_00224BE8(spr, 0, 64, 32, 64);
    func_00224DB8(spr);
    func_00224CA0(spr, 0, -6);
    func_00224B90(&gHudTimer->spr[4], 1);
}

/* Shows sprite 5 from now on. Called by the stage (stg_a_b.c). */
void HudTimer_ShowMark(void) {
    gHudTimer->mark = 1;
}

/* The same; no caller. */
void HudTimer_ShowMark2(void) {
    gHudTimer->mark = 1;
}

/* Round reset: value 0, mark off, both ramps cleared. */
void HudTimer_Reset(void) {
    gHudTimer->value = 0;
    gHudTimer->prev = 0;
    gHudTimer->mark = 0;
    memset(&gHudTimer->pulse, 0, sizeof(Ramp));
    memset(&gHudTimer->slide, 0, sizeof(Ramp));
}
