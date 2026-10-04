#include "common.h"
#include "menu/menu_q.h"

/*
 * UbScore, tail (0x37F430..0x37F850): the score sheet module of the result screens of the mode group 13..30.
 * The object starts at 0x37EE18 (src/menu/menu_p_d.c): its read-only data begins with the table below
 * (0x3B8DD8, in front of gUbScoreTimeSteps at 0x3B8DF0) and ends with the strings of UbScore_PlateGoto
 * (0x3B8FA0..0x3B9008). UbScore_GetRewardItem only matches with the table defined in the file (the load sits in
 * the delay slot of the return), so it is defined here until the two halves are merged.
 */

/* Item ids handed out as rewards by the result screens. */
const s32 gUbRewardItems[6] = { 0x7E, 0x57, 0x6B, 0x6A, 0x7F, 0x80 };

/* Pays out up to `step` points of the total as money, at a tenth; returns 1 when the total is used up. */
s32 UbScore_ConvertStep(QScore *score, s32 step) {
    return UbScore_Transfer(&score->total, &score->convert, step, 0.1f);
}

/* Shows bonus page `page` (1-based): its three lines, as far as the sheet has them. */
void UbScore_SetPage(QScore *score, s32 page) {
    s32 i;
    s32 first;

    if (page > 0) {
        first = page * 3 - 3;
        for (i = 0; i < 3 && first + i < score->bonusCount; i++) {
            score->shown[i] = first + i;
        }
    }
}

/* Lights or dims plate `plate` of the money conversion and its arrow. */
void UbScore_PlateGoto(MFlash *flash, s32 plate, s32 on) {
    MFlashRef ref;
    char name[64];
    char *plateFmt = "mc_pt_convert_plate%d";
    char *arrowFmt = "mc_blue_yajirusi%02d";

    if (on) {
        sprintf(name, plateFmt, plate);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
        sprintf(name, arrowFmt, plate);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_on_loop");
    } else {
        sprintf(name, plateFmt, plate);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
        sprintf(name, arrowFmt, plate);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_stop");
    }
}

/* Enters a result into the save's ranking (ten entries, best first); a score below the tenth is dropped. */
void UbScore_AddRanking(s32 score, s32 chara, s32 cleared) {
    QSaveRank *rank = QSAVE->rank;
    s32 i;
    s32 j;

    for (i = 0; i < Q_RANK_NUM; i++) {
        if (rank[i].score < score) {
            for (j = Q_RANK_NUM - 2; j >= i; j--) {
                rank[j + 1] = rank[j];
            }
            rank[i].score = score;
            rank[i].chara = chara;
            rank[i].cleared = cleared != 0;
            return;
        }
    }
}

/* Keeps a mission's best total with its rank and battle time; returns 1 for a new record. */
s32 UbScore_SaveMissionBest(s32 mission, s32 total, QScore *score) {
    s32 record = 0;

    if (QSAVE->mission[mission].total < total) {
        QSAVE->mission[mission].total = total;
        record = 1;
        QSAVE->mission[mission].rank = score->rank;
        QSAVE->mission[mission].hours = score->hours;
        QSAVE->mission[mission].minutes = score->minutes;
        QSAVE->mission[mission].seconds = score->seconds;
    }
    return record;
}

/* The same for a course of modes 17..19, which also keeps the sheet's fourth line. */
s32 UbScore_SaveBestB(s32 course, s32 total, QScore *score) {
    s32 record = 0;

    if (QSAVE->bestB[course].total < total) {
        QSAVE->bestB[course].total = total;
        record = 1;
        QSAVE->bestB[course].rank = score->rank;
        QSAVE->bestB[course].hours = score->hours;
        QSAVE->bestB[course].minutes = score->minutes;
        QSAVE->bestB[course].seconds = score->seconds;
        QSAVE->bestB[course].value = score->line[3].value;
    }
    return record;
}

/* The same for a course of modes 24..30. */
s32 UbScore_SaveBestC(s32 course, s32 total, QScore *score) {
    s32 record = 0;

    if (QSAVE->bestC[course].total < total) {
        QSAVE->bestC[course].total = total;
        record = 1;
        QSAVE->bestC[course].rank = score->rank;
        QSAVE->bestC[course].hours = score->hours;
        QSAVE->bestC[course].minutes = score->minutes;
        QSAVE->bestC[course].seconds = score->seconds;
    }
    return record;
}

/* Reward item `idx` of the result screens. */
s32 UbScore_GetRewardItem(s32 idx) {
    return gUbRewardItems[idx];
}
