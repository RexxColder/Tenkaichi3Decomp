#include "common.h"
#include "menu/menu_z.h"

/*
 * Menu overlay DBZP.BIN, 0x3A9850..0x3A9A70: Dc_Main, the handler of progress modes 53..56 (the Data Center,
 * main-menu item 7). No data of its own, so whether it is a file of its own or the last function of the DcList
 * file cannot be told from the layout.
 */

#define DC_BGM 0x10B18

/*
 * Runs the Data Center until it is left: mode 53 the top menu, 54 the password screen, 55 the list of saved
 * custom characters, 56 the replay menu. Returns 1 to leave the overlay (a replay was chosen: the battle starts),
 * 0 to go on with the mode it has set.
 */
/*
 * NOT MATCHING: 19 of 135 instructions, all from one thing: the original keeps the constant 1 that `next` is
 * compared with (mode 56) in a saved register loaded before the loop (li s5,1 after li s2,53), the attempt loads
 * it in place (so it saves one register less and its frame is 16 bytes smaller). The loop pass only moves such a
 * constant out when it has two uses in the loop, so the original source compared with 1 twice in a way that
 * was not found. Control flow, calls and stores are identical.
 */
#if 0
s32 Dc_Main(void) {
    s32 ret = 1;
    s32 done;
    s32 next;

    ZPROG->dcCursor = -1;
    ZPROG->unk698 = 0;
    done = 0;
    if (gMenuArc8 == NULL) {
        gMenuArc8 = File_LoadSync(gProgress->baseFile + 0xB, NULL, 0);
    }
    do {
        switch (gProgress->mode) {
        case 53:
            Bgm_Play(DC_BGM);
            next = DcMenu_Run(1);
            if (next != 0) {
                Adx_StopAll();
                gProgress->mode = next;
            } else {
                ret = 0;
                Adx_StopAll();
                gProgress->mode = 4;
                done = 1;
            }
            break;
        case 54:
            Bgm_Play(DC_BGM);
            DcPass_Run(3);
            Adx_StopAll();
            gProgress->mode = 53;
            break;
        case 55:
            Bgm_Play(DC_BGM);
            DcList_Run(2);
            Adx_StopAll();
            gProgress->mode = 53;
            break;
        case 56:
            Bgm_Play(DC_BGM);
            next = ReplayMenu_Run(4);
            if (next != 0) {
                Adx_StopAll();
                if (next == 1) {
                    ret = 1;
                } else {
                    ret = 0;
                    gProgress->mode = next;
                }
                done = 1;
            } else {
                Adx_StopAll();
                gProgress->mode = 53;
            }
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    if (gMenuArc8 != NULL) {
        Heap_Free(gMenuArc8);
        gMenuArc8 = NULL;
    }
    ZPROG->dcVisits++;
    return ret;
}
#else
INCLUDE_ASM("asm/nonmatchings/menu/menu_z_b", Dc_Main);
#endif
