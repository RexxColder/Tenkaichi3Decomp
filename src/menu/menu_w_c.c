#include "common.h"
#include "menu/menu_w.h"

/*
 * Menu overlay DBZP.BIN, 0x39EB08..0x39EFC0: head of the EvoTop object, the top menu of Evolution Z (progress
 * mode 48). Unlike the other screens its functions take the work structure as an argument. The object goes on
 * in the next chunk (its run function is 0x39F9D0). Its read-only data starts at 0x3BC008 (the Shop object's
 * ends with the jump table at 0x3BBDA0..0x3BC004); "fl_on_start" / "fl_off_start" exist in both, so they are two
 * source files.
 *
 * This file looks clips up through small inline helpers that own their MFlashRef. That shows in three ways:
 * the strings of EvoTop_ClipOnOff ("fl_on_start" / "fl_off_start") are emitted in front of the first string of
 * the function that uses it; each inlined copy has its own 16-byte stack slot behind the caller's locals; and
 * the slot's address is formed again at every use instead of being kept in a register. (A 128-bit scalar in
 * place of the MFlashRef gives the same code but not the same string order.)
 */

extern void func_003B0C20(void *dialogPack); /* chunk menu_za: the "save on leaving" helper's init */
/* MsgWin_Init takes three arguments (menu_a.h declares a fourth, unused one as s32); a pointer is passed here. */
extern void MsgWin_Init4(void *pack, void *text, s32 side, void *arg) __asm__("MsgWin_Init");

/* Starts a line of the guide's voice with its subtitle. */
void EvoTop_PlayVoice(EvoTop *menu, s32 line) {
    Voice_PlayWithSubtitle(menu->subtitles, EVOTOP_VOICE_BASE, line);
    menu->voiceLine = line;
}

/* Wraps a cursor: below min gives max, above max gives min. */
s32 EvoTop_Wrap(s32 value, s32 min, s32 max) {
    if (value < min) {
        value = max;
    } else if (value > max) {
        value = min;
    }
    return value;
}

/* Plays the "on" or the "off" animation of a clip of the movie's root. */
static inline void EvoTop_ClipOnOff(MFlash *flash, char *name, s32 on) {
    MFlashRef ref;

    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
    } else {
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
    }
}

/* Lights (on != 0) or dims the plate the cursor is on. */
void EvoTop_SetPlate(EvoTop *menu, s32 on) {
    char name[256];
    MFlash *flash = &menu->flash[0];

    sprintf(name, "mc_menu_plate_%d", menu->cursor + 1);
    EvoTop_ClipOnOff(flash, name, on);
}

/* Sets the texture rectangle of a clip. */
static inline void EvoTop_ClipSetUv(MFlash *flash, char *parent, char *name, MFlashUv *uv) {
    MFlashRef ref;

    Flash_FindLabel(flash, parent, name, &ref);
    Flash_ClipSetUv(flash, &ref, uv);
}

/* Gives each of the three plates its strip of the caption texture. */
void EvoTop_SetPlateText(EvoTop *menu) {
    MFlashUv uv;
    char name[256];
    MFlash *flash = &menu->flash[0];
    s32 i;

    for (i = 0; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i * 0x20;
        uv.x1 = 0x200;
        uv.y1 = uv.y0 + 0x20;
        sprintf(name, "mc_menu_plate_%d", i + 1);
        EvoTop_ClipSetUv(flash, name, "mc_menu_text_on", &uv);
        EvoTop_ClipSetUv(flash, name, "mc_menu_text_off", &uv);
    }
}

/* Advances the movie. */
void EvoTop_Advance(EvoTop *menu) {
    s32 i;

    for (i = 0; i < EVOTOP_FLASH_NUM; i++) {
        Flash_Advance(&menu->flash[i]);
    }
}

#define EVO_RES(n) \
    res = (MTexRes *)MPACK_AT(menu->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Unpacks the menu's section of archive 7 and builds its movie, windows and sounds. */
void EvoTop_Init(EvoTop *menu, s32 section) {
    MTexRes *res = NULL;
    MFlash *flash = &menu->flash[0];

    menu->pack = MPACK_AT(gMenuArc7, section);
    menu->res = Sprite_Unpack(menu->pack, NULL, NULL);

    EVO_RES(4);
    menu->tex[0] = MTEX(res, 0);
    menu->tex[1] = MTEX(res, 1);
    menu->tex[2] = MTEX(res, 2);
    menu->tex[3] = MTEX(res, 3);
    menu->tex[7] = MTEX(res, 4);
    menu->tex[9] = MTEX(res, 5);
    menu->tex[10] = MTEX(res, 6);
    menu->tex[11] = MTEX(res, 7);
    EVO_RES(2);
    menu->tex[4] = MTEX(res, 0);
    menu->tex[5] = MTEX(res, 1);
    menu->tex[6] = MTEX(res, 3);
    EVO_RES(5);
    menu->tex[8] = MTEX(res, 0);
    menu->tex[12] = MTEX(res, 1);
    EVO_RES(1);
    menu->bg = res;

    EVO_RES(3);
    IconWin_Init(MPACK_AT(menu->res, 9), res);
    IconWin_Open();

    menu->msgText = MPACK_AT(menu->res, 7);
    MsgWin_Init4(MPACK_AT(menu->res, 8), menu->msgText, 1, menu->unk7C);
    MsgWin_Open();

    menu->subtitles = MPACK_AT(menu->res, 10);
    Flash_Create(flash, MPACK_AT(menu->res, 6), menu->tex);
    Flash_Play(flash, 1);

    func_003B0C20(MPACK_AT(menu->res, 11));

    menu->blink = Rand_Range(32);
    menu->cursor = EVO_PROGRESS_CURSOR;
    menu->voiceLine = -1;
    StreamSe_PlayDefault(0, 0x10BE0);
}
