#include "common.h"
#include "menu/menu_v.h"

/*
 * Menu overlay DBZP.BIN, 0x395E30..0x396838: EvoZ_Load, the last function of the first source file of the
 * character customising screen (EvoZ). The rest of that file (0x392F10..0x395E30: init, term, draw, update,
 * run) is in the previous chunk; append this function to it.
 */

#define EZ_RES(n) \
    res = (MTexRes *)MPACK_AT(ez->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Unpacks the screen's section of archive 7 and the chip file, builds the three movies, the item details page,
   the password entry and the dialog, and finds the text and the character grid. */
void EvoZ_Load(EvoZ *ez, s32 section) {
    MTexRes *res = NULL;
    s32 i;

    ez->pack = (u32 *)MPACK_AT(gMenuArc7, section);
    ez->res = Sprite_Unpack(ez->pack, NULL, NULL);
    ez->chipFile = File_LoadSync(0x3C0, NULL, 0);
    ez->chipRes = Sprite_Unpack(ez->chipFile, NULL, NULL);
    res = ez->chipRes;
    Res_RelocateOffsets(&res, res, res);
    ez->chipTex = res;

    EZ_RES(3);
    ez->tex0[17] = MTEX(res, 0);
    ez->tex0[20] = MTEX(res, 1);
    ez->tex0[19] = MTEX(res, 2);
    ez->tex0[22] = MTEX(res, 3);
    ez->tex0[25] = MTEX(res, 4);
    EZ_RES(4);
    ez->tex0[18] = MTEX(res, 0);
    ez->tex0[21] = MTEX(res, 1);
    ez->tex0[23] = MTEX(res, 2);
    ez->tex0[26] = MTEX(res, 3);
    EZ_RES(5);
    ez->tex0[1] = MTEX(res, 0);
    ez->tex0[2] = MTEX(res, 1);
    ez->tex0[3] = MTEX(res, 2);
    ez->tex0[5] = MTEX(res, 3);
    EZ_RES(6);
    ez->tex0[4] = MTEX(res, 0);
    ez->tex0[6] = MTEX(res, 1);
    EZ_RES(7);
    ez->tex0[7] = MTEX(res, 0);
    ez->tex0[8] = MTEX(res, 1);
    ez->tex0[9] = MTEX(res, 2);
    ez->tex0[12] = MTEX(res, 3);
    ez->tex0[11] = MTEX(res, 4);
    ez->tex0[13] = MTEX(res, 5);
    EZ_RES(8);
    ez->tex0[27] = MTEX(res, 0);
    ez->tex0[28] = MTEX(res, 1);
    ez->tex0[29] = MTEX(res, 2);
    EZ_RES(9);
    ez->tex0[14] = MTEX(res, 0);
    ez->tex0[15] = MTEX(res, 1);
    ez->tex0[16] = MTEX(res, 2);
    EZ_RES(10);
    ez->tex0[34] = MTEX(res, 0);
    ez->tex0[36] = MTEX(res, 1);
    ez->tex0[37] = MTEX(res, 2);
    ez->tex0[38] = MTEX(res, 3);
    ez->tex0[39] = MTEX(res, 4);
    ez->tex2[3] = MTEX(res, 1);
    ez->tex2[4] = MTEX(res, 2);
    ez->tex2[5] = MTEX(res, 3);
    EZ_RES(11);
    ez->tex0[35] = MTEX(res, 0);
    ez->tex0[40] = MTEX(res, 1);
    ez->tex0[10] = MTEX(res, 2);
    ez->tex2[2] = MTEX(res, 0);
    EZ_RES(12);
    ez->tex0[0] = MTEX(res, 0);
    EZ_RES(16);
    ez->tex0[41] = MTEX(res, 0);
    EZ_RES(17);
    ez->tex0[30] = MTEX(res, 0);
    ez->tex0[31] = MTEX(res, 2);
    Flash_Create(&ez->flash[0], MPACK_AT(ez->res, 13), ez->tex0);
    Flash_Play(&ez->flash[0], 1);
    EZ_RES(18);
    ez->tex1[0] = MTEX(res, 0);
    ez->tex1[2] = MTEX(res, 1);
    EZ_RES(19);
    ez->tex1[1] = MTEX(res, 1);
    ez->tex1[4] = MTEX(res, 2);
    ez->tex1[5] = MTEX(res, 4);
    ez->tex1[6] = MTEX(res, 3);
    EZ_RES(20);
    ez->tex1[7] = MTEX(res, 0);
    EZ_RES(21);
    ez->tex1[3] = MTEX(res, 0);
    EZ_RES(22);
    ez->tex1[10] = MTEX(res, 0);
    ez->tex1[17] = MTEX(res, 1);
    ez->tex0[24] = MTEX(res, 1);
    Flash_Create(&ez->flash[1], MPACK_AT(ez->res, 23), ez->tex1);
    Flash_Play(&ez->flash[1], 1);
    EZ_RES(24);
    ez->tex2[24] = MTEX(res, 0);
    ez->tex2[23] = MTEX(res, 1);
    ez->tex2[25] = MTEX(res, 2);
    EZ_RES(25);
    ez->tex2[19] = MTEX(res, 0);
    ez->tex2[21] = MTEX(res, 1);
    ez->tex2[20] = MTEX(res, 2);
    ez->tex2[22] = MTEX(res, 3);
    ez->tex2[10] = MTEX(res, 4);
    EZ_RES(26);
    ez->tex2[11] = MTEX(res, 0);
    ez->tex2[18] = MTEX(res, 1);
    EZ_RES(27);
    ez->tex2[17] = MTEX(res, 0);
    ez->tex2[16] = MTEX(res, 1);
    EZ_RES(28);
    ez->tex2[12] = MTEX(res, 0);
    ez->tex2[13] = MTEX(res, 0);
    EZ_RES(22);
    ez->tex2[8] = MTEX(res, 1);
    EZ_RES(14);
    ez->tex2[1] = MTEX(res, 0);
    ez->tex2[0] = MTEX(res, 1);
    ez->tex2[6] = MTEX(res, 2);
    EZ_RES(15);
    ez->tex2[9] = MTEX(res, 0);
    EZ_RES(29);
    ez->tex2[15] = MTEX(res, 0);
    EZ_RES(30);
    ez->tex2[7] = MTEX(res, 0);
    Flash_Create(&ez->flash[2], MPACK_AT(ez->res, 31), ez->tex2);
    Flash_Play(&ez->flash[2], 1);

    ItemHelp_Init((u32 *)MPACK_AT(ez->res, 2));
    func_003AE648(MPACK_AT(ez->res, 38));
    ez->dialogText = MPACK_AT(ez->res, 39);
    Dialog_Init(MPACK_AT(ez->res, 32), ez->dialogText, 0);
    Dialog_SetLayout(0);

    ez->items = (VItemEntry *)MPACK_AT(gCommonRes->data[2], 2);
    ez->text[0] = MPACK_AT(ez->res, 34);
    ez->text[1] = MPACK_AT(ez->res, 35);
    ez->text[2] = MPACK_AT(ez->res, 37);
    ez->chipPack = (u32 *)MPACK_AT(ez->res, 36);
    for (i = 0; i < 165; i++) {
        res = (MTexRes *)MPACK_AT(ez->chipPack, i + 1);
        Res_RelocateOffsets(&res, res, res);
    }
    ez->grid = ((VChrGridList *)MPACK_AT(ez->res, 33))->cell;
    ez->gridCount = ez->res[ez->res[33] >> 2];
}
