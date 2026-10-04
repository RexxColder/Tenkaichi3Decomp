#include "common.h"
#include "menu/menu_h.h"

/*
 * Train: the training menu (progress mode 44), 0x359358..0x35A558: the head of an object that goes on in the
 * next chunk (its Init / Update / Input / Run are there). Three menu levels: the top menu (two items), the
 * class menu (three classes) and the lesson list of a class (three rows visible, up to 15 lessons); one bit per
 * lesson in gSaveData->trainClear[class] says the lesson was cleared.
 */

#define gSaveTrain ((MSaveTrain *)gSaveData)

extern void *gSaveData;

/* Whether the lesson has its "cleared" bit. */
s32 Train_IsCleared(s32 class, s32 lesson) {
    return gSaveTrain->trainClear[class] & (1 << lesson);
}

/* 1 if every lesson of the class is cleared. */
s32 Train_IsClassCleared(s32 class) {
    s32 n;
    s32 i;

    if (class == 0) {
        n = gTrain->count[0];
    } else if (class == 1) {
        n = gTrain->count[1];
    } else if (class == 2) {
        n = gTrain->count[2];
    } else {
        n = 0;
    }
    for (i = 0; i < n; i++) {
        if (!Train_IsCleared(class, i)) {
            return 0;
        }
    }
    return 1;
}

/* 1 if all three classes are cleared. */
s32 Train_IsAllCleared(void) {
    if (Train_IsClassCleared(0) && Train_IsClassCleared(1) && Train_IsClassCleared(2)) {
        return 1;
    }
    return 0;
}

/* Copies the five cursor words (sel[2], saved[3]). */
void Train_CopyCursor(s32 *dst, s32 *src) {
    s32 i;

    for (i = 0; i < 5; i++) {
        dst[i] = src[i];
    }
}

/* Sends the clip named by `fmt` and n + 1 to "fl_on_start" or "fl_off_start". */
static inline void Train_ClipGoto(char *fmt, s32 n, s32 on) {
    MFlashRef ref;
    char name[256];
    MFlash *flash = &gTrain->flash[0];

    sprintf(name, fmt, n + 1);
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Lights or dims plate `plate` of the top / class menu. */
void Train_PlateGoto(s32 plate, s32 on) {
    Train_ClipGoto("mc_menu_plate_%d", plate, on);
}

/* Lights or dims row `plate` of the lesson list. */
void Train_Plate2Goto(s32 plate, s32 on) {
    Train_ClipGoto("mc_menu_plate_2_%d", plate, on);
}

/* Lights or dims the plate under the cursor of the current menu level. */
void Train_CursorGoto(s32 on) {
    switch (gTrain->level) {
    case 0:
    case 1:
        Train_PlateGoto(gTrain->sel[gTrain->level], on);
        break;
    case 2:
        Train_Plate2Goto(gTrain->saved[gTrain->sel[1]], on);
        break;
    case 3:
    case 4:
    case 5:
        break;
    }
}

/* Shows the "cleared" icon of the current lesson: the animated one on levels 2 and 5, the still one on 3 and 4. */
void Train_DrawClearIcon(void) {
    MFlashRef ref;
    MFlash *flash = &gTrain->flash[0];

    if (gTrain->level == 3 || gTrain->level == 4) {
        if (Train_IsCleared(gTrain->sel[1], gTrain->row + gTrain->top)) {
            Flash_FindLabel(flash, NULL, "mc_icon_training_clear_in", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, "mc_menu_plate_2_5", "mc_icon_training_clear", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 1);
        } else {
            Flash_FindLabel(flash, NULL, "mc_icon_training_clear_in", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
            Flash_FindLabel(flash, "mc_menu_plate_2_5", "mc_icon_training_clear", &ref);
            Flash_ClipSetFlags(flash, &ref, 2, 0);
        }
    } else if (gTrain->level == 5 || gTrain->level == 2) {
        Flash_FindLabel(flash, NULL, "mc_icon_training_clear_in", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 1);
        Flash_FindLabel(flash, "mc_menu_plate_2_5", "mc_icon_training_clear", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
}

/* Gives the clip `name` of `parent` a texture rectangle. */
static inline void Train_SetUv(MFlash *flash, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Sets the text strips of the six menu plates (texture 0 on the top menu, 2 on the class menu). */
void Train_DrawPlates(void) {
    MFlashRef ref;
    char name[256];
    MFlashUv uv;
    MFlash *flash = &gTrain->flash[0];
    s32 tex;
    s32 i;

    switch (gTrain->level) {
    case 0:
        tex = 0;
        break;
    case 1:
        tex = gTrain->level * 2; /* a plain `tex = 2` compiles the test to `xori` instead of `li / xor` */
        break;
    default:
        tex = 0;
        break;
    }
    for (i = 0; i < 6; i++) {
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetTex(flash, &ref, tex);
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetTex(flash, &ref, tex);
        uv.x0 = 0;
        uv.x1 = 0x200;
        uv.y0 = i * 0x20;
        uv.y1 = i * 0x20 + 0x20;
        Train_SetUv(flash, name, "mc_menu_text_on", &uv);
        Train_SetUv(flash, name, "mc_menu_text_off", &uv);
        Flash_FindLabel(flash, name, "mc_icon_wii_style", &ref);
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
}

/* Attaches the lesson names to the five list plates and shows the "cleared" icon of each. */
void Train_DrawList(void) {
    MFlashRef ref;
    char name[256];
    MFlash *flash = &gTrain->flash[0];
    s32 i;
    s32 lesson;
    s32 line;

    for (i = 0; i < 5; i++) {
        sprintf(name, "mc_menu_plate_2_%d", i + 1);
        if (i == 3) {
            lesson = gTrain->extra;
        } else if (i == 4) {
            lesson = gTrain->extra2;
        } else {
            lesson = gTrain->top + i;
        }
        line = gTrain->nameLine[gTrain->sel[1]][lesson];
        Flash_FindLabel(flash, name, "mc_dammy_text", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, line, &gTrain->box[i]);
        Flash_FindLabel(flash, name, "mc_icon_training_clear", &ref);
        if (i != 4) {
            if (Train_IsCleared(gTrain->sel[1], lesson)) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
    }
}

/* Clip callback: scissor to the lesson list. */
void Train_ScissorOn(void) {
    Sprite_SetScissor(0, 0x200, 0x4F, 0x12F);
}

/* Clip callback: scissor back to the full screen. */
void Train_ScissorOff(void) {
    Sprite_SetScissor(0, 0x1FF, 0, 0x1BF);
}

/* Reads the position of the two "mc_ss" clips and makes them draw inside the list scissor. */
void Train_SetClip(void) {
    MFlashRef ref;

    Flash_FindLabel(&gTrain->flash[0], NULL, "mc_ss_speace", &ref);
    Flash_ClipGetPos(&gTrain->flash[0], &ref, &gTrain->clipX, &gTrain->clipY);
    Flash_ClipSetCallbackA(&gTrain->flash[0], &ref, Train_ScissorOn, NULL);
    Flash_ClipSetCallbackB(&gTrain->flash[0], &ref, Train_ScissorOff, NULL);
    Flash_FindLabel(&gTrain->flash[0], NULL, "mc_ss_dammy", &ref);
    Flash_ClipGetPos(&gTrain->flash[0], &ref, &gTrain->clipX, &gTrain->clipY);
    Flash_ClipSetCallbackA(&gTrain->flash[0], &ref, Train_ScissorOn, NULL);
    Flash_ClipSetCallbackB(&gTrain->flash[0], &ref, Train_ScissorOff, NULL);
}

/* Hides the up (or down) arrow of the lesson list. */
void Train_HideArrow(s32 up) {
    MFlashRef ref;
    char parent[256];
    char name[256];

    if (up) {
        strcpy(parent, "mc_yajirusi_up");
        strcpy(name, "mc_yajirusi_icon_up");
    } else {
        strcpy(parent, "mc_yajirusi_down");
        strcpy(name, "mc_yajirusi_icon_down");
    }
    Flash_FindLabel(&gTrain->flash[0], parent, name, &ref);
    Flash_ClipSetFlags(&gTrain->flash[0], &ref, 2, 0);
}

/* Sets the arrow icons' texture rectangles and hides the arrows that lead nowhere. */
void Train_DrawArrows(void) {
    MFlashUv uv;

    uv.x0 = 0;
    uv.x1 = 0x20;
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    Train_SetUv(&gTrain->flash[0], "mc_yajirusi_up", "mc_yajirusi_icon_up", &uv);
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    uv.y0 = 0x20;
    uv.y1 = 0x40;
    Train_SetUv(&gTrain->flash[0], "mc_yajirusi_down", "mc_yajirusi_icon_down", &uv);
    uv.x0 = 0x20;
    uv.x1 = 0x40;
    uv.y0 = 0;
    uv.y1 = 0x20;
    Train_SetUv(&gTrain->flash[0], "mc_yajirusi_right", "mc_yajirusi_icon_right", &uv);
    if (gTrain->top == 0) {
        Train_HideArrow(1);
    } else if (gTrain->top == gTrain->count[gTrain->sel[1]] - TRAIN_ROWS) {
        Train_HideArrow(0);
    }
    {
        MFlashRef ref;

        Flash_FindLabel(&gTrain->flash[0], NULL, "mc_ss_next_2", &ref);
        if (gTrain->unk440 >= gTrain->unk1BD8 - 1) {
            Flash_ClipSetFlags(&gTrain->flash[0], &ref, 2, 0);
        } else {
            Flash_ClipSetFlags(&gTrain->flash[0], &ref, 2, 1);
        }
        Flash_FindLabel(&gTrain->flash[0], NULL, "mc_ss_next", &ref);
        Flash_ClipSetFlags(&gTrain->flash[0], &ref, 2, 0);
    }
}

/* Clamps *v to lo..hi; 1 if it had to. */
static inline s32 Train_Clamp(s32 *v, s32 lo, s32 hi) {
    if (*v < lo) {
        *v = lo;
        return 1;
    }
    if (*v > hi) {
        *v = hi;
        return 1;
    }
    return 0;
}

/* 1 if v is not in lo..hi. */
static inline s32 Train_IsOutside(s32 *v, s32 lo, s32 hi) {
    if (*v < lo) {
        return 1;
    }
    if (*v > hi) {
        return 1;
    }
    return 0;
}

/* Wraps *v around lo..hi. */
static inline void Train_Wrap(s32 *v, s32 lo, s32 hi) {
    if (*v < lo) {
        *v = hi;
    } else if (*v > hi) {
        *v = lo;
    }
}

/* Says the current lesson's line and remembers it. */
#define TRAIN_SAY(line) \
    if (gTrain->level == 0) { \
        Voice_PlayWithSubtitle(gTrain->subtitlesA, TRAIN_VOICE_BASE_A, line); \
    } else { \
        Voice_PlayWithSubtitle(gTrain->subtitlesB, TRAIN_VOICE_BASE_B, line); \
    } \
    gTrain->voiceLine = line

/* Moves the lesson cursor by dir (-1 / 1), scrolling the list at its edges; 0 if it is at the end. */
s32 Train_MoveRow(s32 dir) {
    char name[256];
    s32 line;
    s32 lesson;
    s32 hi;

    hi = gTrain->count[gTrain->sel[1]] - 1;
    lesson = gTrain->row + gTrain->top + dir;
    if (Train_IsOutside(&lesson, 0, hi)) {
        return 0;
    }
    Train_Plate2Goto(gTrain->row, 0);
    gTrain->row += dir;
    if (Train_Clamp(&gTrain->row, 0, TRAIN_ROWS - 1)) {
        gTrain->top += dir;
        if (!Train_Clamp(&gTrain->top, 0, gTrain->count[gTrain->sel[1]] - TRAIN_ROWS)) {
            switch (dir) {
            case 1:
                Flash_GotoLabel(&gTrain->flash[0], "fl_class_menu_up", 1);
                gTrain->extra = gTrain->top - 1;
                break;
            case -1:
                Flash_GotoLabel(&gTrain->flash[0], "fl_class_menu_down", 1);
                gTrain->extra = gTrain->top + 3;
                break;
            }
        }
    }
    sprintf(name, "mc_menu_plate_2_%d", gTrain->row + 1);
    Train_Plate2Goto(gTrain->row, 1);
    Snd_PlaySe(1, 0);
    line = gTrain->voiceTbl[gTrain->sel[1]][gTrain->row + gTrain->top];
    TRAIN_SAY(line);
    return 1;
}

/* Moves the lesson cursor to the next lesson without the scroll animation or the sound. */
void Train_NextRow(void) {
    char name[256];
    s32 line;

    Train_Plate2Goto(gTrain->row, 0);
    gTrain->row++;
    if (Train_Clamp(&gTrain->row, 0, TRAIN_ROWS - 1)) {
        gTrain->top++;
    }
    if (!Train_Clamp(&gTrain->top, 0, gTrain->count[gTrain->sel[1]] - TRAIN_ROWS)) {
        gTrain->extra = gTrain->top - 1;
    }
    sprintf(name, "mc_menu_plate_2_%d", gTrain->row + 1);
    Train_Plate2Goto(gTrain->row, 1);
    line = gTrain->voiceTbl[gTrain->sel[1]][gTrain->row + gTrain->top];
    TRAIN_SAY(line);
}

/* Up (dir 0) or down (dir 1) on the current menu level. */
void Train_MoveCursor(s32 dir) {
    s32 d = 0;
    s32 line;

    switch (dir) {
    case 0:
        d = -1;
        break;
    case 1:
        d = 1;
        break;
    }
    switch (gTrain->level) {
    case 0:
        Train_CursorGoto(0);
        gTrain->sel[gTrain->level] += d;
        Train_Wrap(&gTrain->sel[gTrain->level], 0, 1);
        Train_CursorGoto(1);
        Snd_PlaySe(1, 0);
        line = gTrain->sel[0] + 2;
        TRAIN_SAY(line);
        break;
    case 1:
        Train_CursorGoto(0);
        gTrain->sel[gTrain->level] += d;
        Train_Wrap(&gTrain->sel[gTrain->level], 0, 2);
        Train_CursorGoto(1);
        Snd_PlaySe(1, 0);
        line = gTrain->sel[1] + 1;
        TRAIN_SAY(line);
        break;
    case 2:
        Train_MoveRow(d);
        break;
    case 3:
    case 4:
    case 5:
        break;
    }
}
