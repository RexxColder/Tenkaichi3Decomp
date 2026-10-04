#include "common.h"
#include "menu/menu_r.h"

/*
 * SimTop, 0x387880..0x388618: the first four functions of the entry screen of the "sim" ladder (mode 20, run by
 * SimTop_Run 0x388D70 in the next chunk, src/menu/menu_s.c): three plates, the ranking list, the how-to-play
 * text. Work pointer 0x3B7420 (.data, followed by a padding word and the table gSimTopFaceTex at 0x3B7428). The
 * object's .rodata starts at 0x3B9ED0 with constants of the next chunk (0x3B9ED8 gSimTopPlateVoice, 0x3B9EE8
 * gSimTopIdleVoice), which are in front of this file's first string (0x3B9EF0 "mc_icon_star"), so they are
 * file-scope constants defined above these functions; this file's strings end at 0x3BA050, where the jump table
 * of SimTop_UpdateVoice follows. menu_s.c appends to this file.
 */

/* Shows or hides the star of ranking plate `parent`. */
void SimTop_ShowStar(char *parent, s32 on) {
    MFlashRef ref;
    MFlash *flash = &gSimTop->flash[0];

    Flash_FindLabel(flash, parent, "mc_icon_star", &ref);
    if (on) {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    }
}

/*
 * Loads the screen's section of archive 3 and resets the ladder kept in gProgress: level 0, attack 0, defence 0,
 * health 100 %, points 0, no items, turn 0, one fighter, no DP rule.
 */
void SimTop_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;

    gSimTop = Heap_Alloc(sizeof(SimTop), 0x20, 0, 2);
    memset(gSimTop, 0, sizeof(SimTop));
    gSimTop->pack = (u32 *)MPACK_AT(gMenuArc3, section);
    gSimTop->res = Sprite_Unpack(gSimTop->pack, NULL, NULL);

    res = (MTexRes *)MPACK_AT(gSimTop->res, 4);
    Res_RelocateOffsets(&res, res, res);
    gSimTop->bg = res;

    res = (MTexRes *)MPACK_AT(gSimTop->res, 5);
    Res_RelocateOffsets(&res, res, res);
    gSimTop->tex[32] = MTEX(res, 5);
    gSimTop->tex[33] = MTEX(res, 6);
    gSimTop->tex[34] = MTEX(res, 8);

    res = (MTexRes *)MPACK_AT(gSimTop->res, 2);
    Res_RelocateOffsets(&res, res, res);
    gSimTop->tex[20] = NULL;
    gSimTop->tex[22] = NULL;
    gSimTop->tex[23] = NULL;
    gSimTop->tex[24] = NULL;
    gSimTop->tex[25] = NULL;
    gSimTop->tex[26] = NULL;
    gSimTop->tex[31] = NULL;
    gSimTop->tex[0] = MTEX(res, 0);
    gSimTop->tex[1] = MTEX(res, 1);
    gSimTop->tex[11] = MTEX(res, 2);
    gSimTop->tex[18] = MTEX(res, 3);
    gSimTop->tex[16] = MTEX(res, 4);
    gSimTop->tex[15] = MTEX(res, 5);
    gSimTop->tex[28] = MTEX(res, 6);
    gSimTop->tex[27] = MTEX(res, 7);
    gSimTop->tex[30] = MTEX(res, 8);
    gSimTop->tex[19] = MTEX(res, 9);
    gSimTop->tex[21] = MTEX(res, 10);
    gSimTop->tex[13] = MTEX(res, 11);
    gSimTop->tex[14] = MTEX(res, 12);
    gSimTop->tex[3] = MTEX(res, 13);
    gSimTop->tex[2] = MTEX(res, 14);
    gSimTop->tex[7] = MTEX(res, 15);
    gSimTop->tex[10] = MTEX(res, 16);
    gSimTop->tex[9] = MTEX(res, 17);
    gSimTop->tex[8] = MTEX(res, 18);
    gSimTop->tex[12] = MTEX(res, 19);
    gSimTop->tex[17] = MTEX(res, 20);
    gSimTop->tex[4] = MTEX(res, 21);
    gSimTop->tex[5] = MTEX(res, 22);
    gSimTop->tex[6] = MTEX(res, 23);
    gSimTop->tex[29] = MTEX(res, 24);
    gSimTop->tex[35] = MTEX(res, 25);
    gSimTop->tex[36] = MTEX(res, 26);
    gSimTop->tex[37] = MTEX(res, 27);
    gSimTop->tex[38] = MTEX(res, 28);
    gSimTop->tex[39] = MTEX(res, 29);

    gSimTop->faces = (u32 *)MPACK_AT(gSimTop->res, 7);
    for (i = 0; i < SIMTOP_FACE_NUM; i++) {
        res = (MTexRes *)MPACK_AT(gSimTop->faces, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }

    Flash_Create(&gSimTop->flash[0], MPACK_AT(gSimTop->res, 1), gSimTop->tex);
    Flash_Play(&gSimTop->flash[0], 1);

    res = (MTexRes *)MPACK_AT(gSimTop->res, 3);
    Res_RelocateOffsets(&res, res, res);
    IconWin_Init(MPACK_AT(gSimTop->res, 6), res);
    IconWin_Open();

    gSimTop->text = MPACK_AT(gSimTop->res, 8);
    gSimTop->subtitles = MPACK_AT(gSimTop->res, 9);
    TextBox_Init(&gSimTop->box, gSimTop->text, 0);
    TextBox_SetUnk50(&gSimTop->box, 0);
    TextBox_SetSpacing(&gSimTop->box, 0, 4);

    gSimTop->voiceLine = -1;
    gSimTop->top = 0;
    gSimTop->cursor = 0;
    gSimTop->voiceSkip = 0;

    SIM_PROG->run.turn = 0;
    SIM_PROG->run.level = 0;
    SIM_PROG->run.stat[SIM_STAT_ATK] = 0;
    SIM_PROG->run.stat[SIM_STAT_DEF] = 0;
    SIM_PROG->run.stat[SIM_STAT_HP] = 100;
    SIM_PROG->run.stat[SIM_STAT_POINT] = 0;
    for (i = 0; i < SIM_ITEM_NUM; i++) {
        SIM_PROG->run.item[i] = -1;
    }
    SIM_PROG->run.itemHead = 0;
    SIM_PROG->run.wait = 0;
    SIM_PROG->run.off = 0;
    SIM_PROG->teamSize = 1;
    SIM_PROG->dpRule = 0;
}

/* Frees the screen. */
void SimTop_Term(void) {
    s32 i;

    IconWin_Term();
    for (i = 0; i < SIMTOP_FLASH_NUM; i++) {
        Flash_Destroy(&gSimTop->flash[i]);
    }
    if (gSimTop->res != NULL) {
        Heap_Free(gSimTop->res);
        gSimTop->res = NULL;
    }
    if (gSimTop != NULL) {
        Heap_Free(gSimTop);
        gSimTop = NULL;
    }
}

/* Sets up the movie's clips (ranking rows, portraits, the description) and draws it. */
void SimTop_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[0x40];
    MFlash *flash;
    s32 i;
    s32 x;
    s32 n;
    s32 score;
    s32 star;
    s32 chara;
    MTexRes *face;

    Sprite_DrawPicture(gSimTop->bg, 0, 0, 0x80);
    flash = &gSimTop->flash[0];
    Flash_FindLabel(flash, "mc_guide_18go", "mc_guide_18go_eye", &ref);
    FlashAnim_Blink(flash, &ref, &gSimTop->blink, 0);
    Flash_FindLabel(flash, "mc_guide_18go", "mc_guide_18go_mouth", &ref);
    FlashAnim_Talk(flash, &ref, &gSimTop->mouth, 0);

    x = gSimTop->iconFrame << 6;
    uv.x0 = x;
    uv.y0 = 0;
    uv.x1 = x + 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_icon_play1", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    gSimTop->iconFrame ^= 1;

    for (i = 1; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i << 5;
        uv.x1 = 0x200;
        uv.y1 = (i << 5) + 0x20;
        sprintf(name, "mc_sim_plate%02d", i + 1);
        Flash_FindLabel(flash, name, "mc_friend_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_friend_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }

    uv.x0 = 0;
    uv.y0 = 0x20;
    uv.x1 = 0x20;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_menu_yajirusi_up", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gSimTop->top == 0) {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    }

    uv.x0 = 0x20;
    uv.y0 = 0x20;
    uv.x1 = 0x40;
    uv.y1 = 0x40;
    Flash_FindLabel(flash, NULL, "mc_menu_yajirusi_down", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (gSimTop->top == 5) {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    }

    for (i = 0; i < 6; i++) {
        sprintf(name, "mc_senreki_plate%02d", i);
        if (i != 5) {
            n = gSimTop->top + i + 1;
        } else {
            n = gSimTop->cursor + 1;
        }
        uv.x0 = (n % 4) << 6;
        uv.y0 = (n / 4) << 6;
        uv.x1 = ((n % 4) << 6) + 0x40;
        uv.y1 = ((n / 4) << 6) + 0x40;
        Flash_FindLabel(flash, name, "mc_jyuni_suji", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_jyuni_suji_ef", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);

        if (i != 5) {
            chara = SIM_SAVE->rank[gSimTop->top + i].chara;
            face = (MTexRes *)MPACK_AT(gSimTop->faces, chara + 1);
            gSimTop->tex[gSimTopFaceTex[i]] = face->tex;
        } else {
            chara = SIM_SAVE->rank[gSimTop->cursor].chara;
            face = (MTexRes *)MPACK_AT(gSimTop->faces, chara + 1);
            gSimTop->tex[SIMTOP_TEX_CURSOR_FACE] = face->tex;
        }
        if (i != 5) {
            score = SIM_SAVE->rank[gSimTop->top + i].score;
        } else {
            score = SIM_SAVE->rank[gSimTop->cursor].score;
        }
        Num_DrawChild(flash, name, "mc_pt_suji%02d", 0, 9, score * 100, 0x20, 0x20, 0, 0);
        if (i != 5) {
            star = SIM_SAVE->rank[gSimTop->top + i].cleared;
        } else {
            star = SIM_SAVE->rank[gSimTop->cursor].cleared;
        }
        SimTop_ShowStar(name, star);

        uv.x0 = 0;
        uv.y0 = 0;
        uv.x1 = 0x20;
        uv.y1 = 0x20;
        Flash_FindLabel(flash, name, "mc_icon_star", &ref);
        FlashAnim_Sheet(flash, &ref, &gSimTop->starTimer, &gSimTop->starFrame, &uv, 2, 2, 0x80);
    }

    Flash_FindLabel(flash, NULL, "mc_setumeikari", &ref);
    TextBox_AttachLine(flash, &ref, 0, 0, gSimTop->page, &gSimTop->box);
    Flash_FindLabel(flash, NULL, "mc_episode_next_2", &ref);
    if (gSimTop->page >= 10) {
        Flash_ClipSetFlags(flash, &ref, 2, 0);
    } else {
        Flash_ClipSetFlags(flash, &ref, 2, 1);
    }
    Flash_FindLabel(flash, NULL, "mc_episode_next", &ref);
    Flash_ClipSetFlags(flash, &ref, 2, 0);

    for (i = 0; i < SIMTOP_FLASH_NUM; i++) {
        Flash_Draw(&gSimTop->flash[i]);
    }
    IconWin_Draw();
}
