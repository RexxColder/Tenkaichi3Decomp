#include "common.h"
#include "menu/menu_k.h"

/*
 * Bracket, 0x368068..0x368C18: the loader of the bracket screen, first function of the object that goes on in
 * menu_l (the bracket logic). Its read-only data starts at 0x3B6FD0, 16-byte aligned after the previous object's
 * jump table, and repeats "fl_guide_in" of menu_k_c.c.
 */

#define BK_RES(n) \
    res = (MTexRes *)MPACK_AT(b->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads the tournament's archive and backdrop and creates the six movies and the windows. */
void Bracket_Load(Bracket *b) {
    u8 *bgTex[3];
    MTexRes *res = NULL;
    s32 i;

    if (b->pack == NULL) {
        b->pack = File_LoadSync(gProgress->baseFile + TOUR_PROG2->tour + 6, NULL, 0);
    }
    b->res = Sprite_Unpack(b->pack, NULL, NULL);

    /* title plate and banners */
    BK_RES(32);
    b->texTitle[18] = MTEX(res, 0);
    BK_RES(34);
    b->texTitle[20] = MTEX(res, 0);
    b->texTitle[22] = MTEX(res, 1);
    b->texTitle[19] = MTEX(res, 2);
    b->texTitle[21] = MTEX(res, 3);
    BK_RES(35);
    b->texTitle[17] = MTEX(res, 0);
    b->texTitle[15] = MTEX(res, 1);
    BK_RES(37);
    b->texTitle[16] = MTEX(res, 0);
    BK_RES(1);
    b->texTitle[0] = MTEX(res, 0);
    BK_RES(2);
    b->texTitle[28] = MTEX(res, 0);
    b->texTitle[30] = MTEX(res, 1);
    b->texTitle[27] = MTEX(res, 2);
    b->texTitle[29] = MTEX(res, 3);
    b->texTitle[35] = MTEX(res, 4);
    b->texTitle[34] = MTEX(res, 5);
    BK_RES(3);
    b->texTitle[23] = MTEX(res, 0);
    b->texTitle[24] = MTEX(res, 1);
    b->texTitle[26] = MTEX(res, 2);
    b->texTitle[31] = MTEX(res, 3);
    bgTex[0] = MTEX(res, 3);
    bgTex[2] = MTEX(res, 2);
    BK_RES(5);
    b->texTitle[9] = MTEX(res, 0);
    BK_RES(6);
    b->texTitle[37] = MTEX(res, 0);
    b->texTitle[33] = MTEX(res, 1);
    BK_RES(27);
    b->texTitle[36] = MTEX(res, 0);
    b->texTitle[32] = MTEX(res, 1);
    BK_RES(28);
    b->texTitle[2] = MTEX(res, 0);
    b->texTitle[4] = MTEX(res, 1);
    b->texTitle[6] = MTEX(res, 2);
    b->texTitle[8] = MTEX(res, 3);
    b->texTitle[1] = MTEX(res, 4);
    b->texTitle[3] = MTEX(res, 5);
    b->texTitle[5] = MTEX(res, 6);
    b->texTitle[7] = MTEX(res, 7);
    b->texTitle[25] = MTEX(res, 8);
    bgTex[1] = MTEX(res, 8);
    Flash_Create(&b->flash[BRK_FL_TITLE], MPACK_AT(b->res, 4), b->texTitle);
    Flash_Play(&b->flash[BRK_FL_TITLE], 1);
    if (gProgress->flags & MPROG_TOUR_RUNNING) {
        Flash_GotoLabel(&b->flash[BRK_FL_TITLE], "fl_title_in", 1);
    }

    /* backdrop: three of the title movie's textures go into its movie too */
    b->file = File_LoadSync(TOUR_PROG2->tour + 0x3C9, NULL, 0);
    TourBg_Init(b->file, TOUR_PROG2->tour, bgTex);

    /* the guide */
    switch (TOUR_PROG2->tour) {
    case TOUR_BIG:
    case TOUR_CELL:
        BK_RES(43);
        b->texGuide[0] = MTEX(res, 0);
        b->texGuide[2] = MTEX(res, 1);
        b->texGuide[1] = MTEX(res, 3);
        b->guideTex[0] = res;
        BK_RES(44);
        b->guideTex[1] = res;
        Flash_Create(&b->flash[BRK_FL_GUIDE], MPACK_AT(b->res, 29), b->texGuide);
        break;
    case TOUR_WORLD:
    case TOUR_OTHERWORLD:
        BK_RES(43);
        b->texGuide[0] = MTEX(res, 0);
        b->texGuide[1] = MTEX(res, 1);
        Flash_Create(&b->flash[BRK_FL_GUIDE], MPACK_AT(b->res, 29), b->texGuide);
        break;
    case TOUR_YAMCHA:
        BK_RES(43);
        b->texGuide[0] = MTEX(res, 0);
        b->texGuide[2] = MTEX(res, 1);
        b->texGuide[1] = MTEX(res, 3);
        b->guideTex[0] = res;
        BK_RES(44);
        b->guideTex[1] = res;
        BK_RES(45);
        b->texGuide[6] = MTEX(res, 0);
        b->texGuide[8] = MTEX(res, 1);
        b->texGuide[7] = MTEX(res, 3);
        Flash_Create(&b->flash[BRK_FL_GUIDE], MPACK_AT(b->res, 29), b->texGuide);
        break;
    }
    if (!(gProgress->flags & MPROG_TOUR_RUNNING)) {
        Flash_Play(&b->flash[BRK_FL_GUIDE], 1);
        Flash_GotoLabel(&b->flash[BRK_FL_GUIDE], "fl_guide_in", 1);
    }

    /* the tree and the moving chips */
    BK_RES(7);
    b->texTree[25] = MTEX(res, 0);
    BK_RES(8);
    b->texTree[7] = MTEX(res, 0);
    b->texTree[8] = MTEX(res, 1);
    b->texMoveA[2] = MTEX(res, 0);
    b->texMoveA[3] = MTEX(res, 1);
    b->texMoveB[2] = MTEX(res, 0);
    b->texMoveB[3] = MTEX(res, 1);
    BK_RES(9);
    b->texTree[5] = MTEX(res, 0);
    b->texTree[4] = MTEX(res, 1);
    BK_RES(30);
    b->texTree[0] = MTEX(res, 0);
    b->texTree[1] = MTEX(res, 1);
    b->texTree[2] = MTEX(res, 2);
    b->texTree[3] = MTEX(res, 3);
    Flash_Create(&b->flash[BRK_FL_TREE], MPACK_AT(b->res, 10), b->texTree);
    BK_RES(11);
    b->texMoveA[0] = MTEX(res, 0);
    b->texMoveB[0] = MTEX(res, 0);
    b->texMoveB[5] = MTEX(res, 1);
    BK_RES(12);
    b->texMoveA[5] = MTEX(res, 0);
    Flash_Create(&b->flash[BRK_FL_MOVE_A], MPACK_AT(b->res, 13), b->texMoveA);
    Flash_Create(&b->flash[BRK_FL_MOVE_B], MPACK_AT(b->res, 14), b->texMoveB);

    /* the versus panel */
    BK_RES(17);
    b->texVs[2] = MTEX(res, 0);
    BK_RES(18);
    b->texVs[7] = MTEX(res, 0);
    b->texVs[8] = MTEX(res, 2);
    b->texVs[9] = MTEX(res, 1);
    b->texVs[10] = MTEX(res, 3);
    BK_RES(19);
    b->texVs[1] = MTEX(res, 0);
    b->texVs[4] = MTEX(res, 1);
    BK_RES(20);
    b->texVs[3] = MTEX(res, 0);
    BK_RES(15);
    b->texVs[0] = MTEX(res, 0);
    b->texVs[11] = MTEX(res, 1);
    Flash_Create(&b->flash[BRK_FL_VS], MPACK_AT(b->res, 16), b->texVs);
    if (gProgress->flags & MPROG_TOUR_RUNNING) {
        Flash_Play(&b->flash[BRK_FL_VS], 1);
        Flash_GotoLabel(&b->flash[BRK_FL_VS], "fl_in", 1);
    }

    /* windows and text */
    b->msgText = MPACK_AT(b->res, 25);
    b->subtitles = MPACK_AT(b->res, 39);
    MsgWin_Init(MPACK_AT(b->res, 26), b->msgText, 0, 0);
    if (!(gProgress->flags & MPROG_TOUR_RUNNING)) {
        MsgWin_Open();
    }
    Dialog_Init(MPACK_AT(b->res, 36), NULL, 0);
    GetWin_Init(MPACK_AT(b->res, 42), 1);
    b->nameText = MPACK_AT(b->res, 21);
    b->formText = MPACK_AT(b->res, 22);
    b->chips = (u32 *)MPACK_AT(b->res, 23);
    for (i = 0; i < BRK_CELL_MAX; i++) {
        res = (MTexRes *)MPACK_AT(b->chips, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    b->grid = (BrkCell *)(MPACK_AT(b->res, 24) + 0x10);
    b->gridCount = b->res[b->res[24] >> 2];
    b->cpu = MPACK_AT(b->res, 31);
    b->prize = MPACK_AT(b->res, 41);
}
