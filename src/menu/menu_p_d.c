#include "common.h"
#include "menu/menu_p.h"

/*
 * UbScore, 0x37EE18..0x37F430: head of the score sheet module of the result screens of the mode group 13..30
 * (the object continues in the next chunk: src/menu/menu_q.c, 0x37F430..0x37F850, must be appended to this file).
 * Its .rodata starts at 0x3B8DD8 with the reward item table (six words), then the time steps below (0x3B8DF0),
 * the initialiser of UbScore_CalcRank (0x3B8F80) and, from 0x3B8FA0, the strings of the tail.
 */

/*
 * Item ids handed out as rewards (read by UbScore_GetRewardItem, next chunk). It is the first thing in the object's
 * read-only data, so it is defined here; src/menu/menu_q.c carries a second definition until the two are merged.
 */
const s32 gUbRewardItems[6] = { 0x7E, 0x57, 0x6B, 0x6A, 0x7F, 0x80 };

/* Line 2 is priced by step: the index of the first entry that is not below the value. 999, 1999, ... 99999. */
const s32 gUbScoreTimeSteps[100] = {
    999,   1999,  2999,  3999,  4999,  5999,  6999,  7999,  8999,  9999,  10999, 11999, 12999, 13999, 14999,
    15999, 16999, 17999, 18999, 19999, 20999, 21999, 22999, 23999, 24999, 25999, 26999, 27999, 28999, 29999,
    30999, 31999, 32999, 33999, 34999, 35999, 36999, 37999, 38999, 39999, 40999, 41999, 42999, 43999, 44999,
    45999, 46999, 47999, 48999, 49999, 50999, 51999, 52999, 53999, 54999, 55999, 56999, 57999, 58999, 59999,
    60999, 61999, 62999, 63999, 64999, 65999, 66999, 67999, 68999, 69999, 70999, 71999, 72999, 73999, 74999,
    75999, 76999, 77999, 78999, 79999, 80999, 81999, 82999, 83999, 84999, 85999, 86999, 87999, 88999, 89999,
    90999, 91999, 92999, 93999, 94999, 95999, 96999, 97999, 98999, 99989,
};

/*
 * Fills the sheet from the result of the battle just fought. Returns 0 when side 0 won, 1 when it lost, 2 when the
 * battle was aborted (the sheet is then left empty); *pages = pages of bonus plates.
 */
s32 UbScore_Fill(s32 kind, UbScore *score, s32 *pages) {
    s32 outcome = 2;
    PBattleResult *result = BattleResult_GetPtr();
    s32 i;
    s32 n;
    s32 q;
    s32 m;

    if (result->aborted == 0) {
        if (result->winner & 1) {
            outcome = 0;
        } else {
            outcome = 1;
        }
    }
    memset(score, 0, sizeof(UbScore));
    *pages = 0;
    if (outcome != 2) {
        if (0.0f < result->health[0] && result->health[0] < 1.0f) {
            score->line[0].value = 1;
        } else {
            score->line[0].value = result->health[0];
        }
        score->line[1].value = result->unk24[0];
        score->line[2].value = result->unk1C[0];
        if (kind == 2) {
            score->line[3].value = result->frames;
            score->lineCount = 4;
        } else {
            score->line[3].value = 0;
            score->lineCount = 3;
        }
        if (score->line[1].value > 100) {
            score->line[1].value = 100;
        }
        if (score->line[2].value > 99990) {
            score->line[2].value = 99990;
        }
        for (i = 0; i < UBSCORE_BONUS_MAX; i++) {
            if ((result->eventSummary >> i) & 1) {
                score->bonus[score->bonusCount++].value = i;
            }
        }
        n = UBSCORE_PAGE;
        m = UBSCORE_PAGE;
        q = score->bonusCount / n;
        if (score->bonusCount % m) {
            q++;
        }
        *pages = q;
        score->clock = result->clock;
        if (score->clock.hours >= 100) {
            score->clock.hours = 99;
        }
    }
    return outcome;
}

/* Prices every line: lines 0, 1 and 3 by their value, line 2 by its step, the bonuses by their id. */
void UbScore_CalcPoints(UbScorePrice *price, UbScore *score) {
    s32 i;
    s32 j;

    for (i = 0; i < UBSCORE_LINE_MAX; i++) {
        if (i != 2) {
            score->line[i].remain = price[score->line[i].value].line[i];
        } else {
            for (j = 0; j < 100; j++) {
                if (!(gUbScoreTimeSteps[j] < score->line[i].value)) {
                    score->line[i].remain = price[j].line[2];
                    break;
                }
                if (j == 100) {
                    /* never reached */
                    score->line[i].remain = price[100].line[2];
                }
            }
        }
        score->line[i].points = score->line[i].remain;
    }
    for (i = 0; i < score->bonusCount; i++) {
        score->bonus[i].remain = price[score->bonus[i].value].bonus;
        score->bonus[i].points = score->bonus[i].remain;
    }
}

/* Adds the sheet up and ranks the total (1 = best ... 4; the limits depend on the kind of sheet). */
s32 UbScore_CalcRank(s32 kind, UbScore *score) {
    s32 limit[2][4] = { { 1599, 2199, 2799, 9999 }, { 2099, 2499, 3199, 9999 } };
    s32 row;
    s32 total;
    s32 i;

    if (kind == 2 || kind == 4) {
        row = 1;
    } else {
        row = 0;
    }
    total = 0;
    score->rank = 0;
    for (i = 0; i < score->lineCount; i++) {
        total += score->line[i].remain;
    }
    for (i = 0; i < score->bonusCount; i++) {
        total += score->bonus[i].remain;
    }
    for (i = 0; i < 4; i++) {
        if (!(limit[row][i] < total)) {
            score->rank = i + 1;
            break;
        }
    }
    return total;
}

/*
 * Moves up to `step` points from *from to *to (scaled by `rate` on the way), with the counting sound. Returns 1
 * when *from is empty.
 */
s32 UbScore_Transfer(s32 *from, s32 *to, s32 step, f32 rate) {
    if (*from <= 0) {
        return 1;
    }
    Snd_PlaySe(2, 5);
    if (*from >= step) {
        *to += (s32)(step * rate);
        *from -= step;
    } else {
        *to += (s32)(*from * rate);
        *from = 0;
        return 1;
    }
    return 0;
}

/* Counts one step of a line (isBonus 0) or of a bonus (1) into the total. Returns 1 when the line is empty. */
s32 UbScore_CountLine(UbScore *score, s32 isBonus, s32 index, s32 step) {
    UbScoreLine *line = NULL;
    s32 done;

    switch (isBonus) {
    case 0:
        line = &score->line[index];
        break;
    case 1:
        line = &score->bonus[index];
        break;
    }
    done = UbScore_Transfer(&line->remain, &score->total, step, 1.0f);
    if (P_PROG->ubCursor == 0) {
        if (score->total > 0xFFFF) {
            score->total = 0xFFFF;
        }
    }
    return done;
}
