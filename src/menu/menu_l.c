#include "common.h"
#include "menu/menu_l.h"

/*
 * Bracket, 0x368C18..0x36B3E0: the tournament logic of Dragon World Tour (progress mode 35). Tail of the object that
 * starts at 0x3673F8 (menu_k_d.c: Bracket_SetupDraw, Bracket_Load). Every function takes the bracket work area.
 *
 * The tree: 17 entrants. Entrants 0..15 play a knock-out of 15 matches (matches 0..7, 8..11, 12..13, 14); the
 * winner meets entrant 16, the seeded "boss", in match 15. Tree positions 0..15 are the leaves, 16..23, 24..27,
 * 28..29 the nodes of the first three rounds, 30 the boss, 31 the winner of match 14, 32 the champion.
 */

/*
 * Decides a match between two CPU entrants: a coin toss (Rand_Range(2)), the loser is marked, the winner takes the
 * match's tree position and its slot in the next match. If one of the two is already marked lost no toss is made
 * and the MARKED one moves on (the original's logic; it cannot happen, losers never reach a later match).
 */
void Bracket_PlayCpuMatch(LBracket *b, s32 match) {
    s32 win = 0;

    if (b->match_[match].flags & LTOUR_MATCH_DONE) {
        return;
    }
    if (b->entrant[b->match_[match].ent[0]].flags & LTOUR_ENT_PLAYER) {
        return;
    }
    if (b->entrant[b->match_[match].ent[1]].flags & LTOUR_ENT_PLAYER) {
        return;
    }
    if (b->entrant[b->match_[match].ent[0]].flags & LTOUR_ENT_LOST) {
        win = 0;
    } else if (b->entrant[b->match_[match].ent[1]].flags & LTOUR_ENT_LOST) {
        win = 1;
    } else {
        win = Rand_Range(2);
        b->entrant[b->match_[match].ent[win ^ 1]].flags |= LTOUR_ENT_LOST;
    }
    b->entrant[b->match_[match].ent[win]].pos = b->match_[match].pos;
    if (b->match_[match].flags & LTOUR_MATCH_TO_LEFT) {
        b->match_[b->match_[match].next].ent[0] = b->match_[match].ent[win];
    } else if (b->match_[match].flags & LTOUR_MATCH_TO_RIGHT) {
        b->match_[b->match_[match].next].ent[1] = b->match_[match].ent[win];
    }
    b->match_[match].flags |= LTOUR_MATCH_DONE;
}

/* How far the tree is scrolled (pixels) to show the match with view number `view`. */
s32 Bracket_GetViewX(s32 view) {
    switch (view) {
    case 0:
        return 0;
    case 1:
        return 0x50;
    case 2:
        return 0xC4;
    case 3:
        return 0x138;
    case 4:
        return 0x1AC;
    case 5:
        return 0x200;
    case 6:
        return 0x8A;
    case 7:
        return 0x172;
    case 8:
        return 0x16;
    case 9:
        return 0x1E4;
    case 10:
        return 0x100;
    case 11:
        return 0x32;
    }
    return 0;
}

/* x of tree position `pos` inside the tree movie. */
s32 Bracket_GetPosX(s32 pos) {
    switch (pos) {
    case 0:
        return 0x33;
    case 1:
        return 0x6D;
    case 2:
        return 0xA7;
    case 3:
        return 0xE1;
    case 4:
        return 0x11B;
    case 5:
        return 0x155;
    case 6:
        return 0x18F;
    case 7:
        return 0x1C9;
    case 8:
        return 0x203;
    case 9:
        return 0x23D;
    case 10:
        return 0x277;
    case 11:
        return 0x2B1;
    case 12:
        return 0x2EB;
    case 13:
        return 0x325;
    case 14:
        return 0x35F;
    case 15:
        return 0x399;
    case 16:
        return 0x50;
    case 17:
        return 0xC4;
    case 18:
        return 0x137;
    case 19:
        return 0x1AB;
    case 20:
        return 0x21F;
    case 21:
        return 0x293;
    case 22:
        return 0x306;
    case 23:
        return 0x37A;
    case 24:
        return 0x8A;
    case 25:
        return 0x171;
    case 26:
        return 0x259;
    case 27:
        return 0x340;
    case 28:
        return 0xFD;
    case 29:
        return 0x2CC;
    case 30:
        return 0x54;
    case 31:
        return 0x1E5;
    case 32:
        return 0x11E;
    }
    return 0;
}

/* y of tree position `pos`: one height per round. */
s32 Bracket_GetPosY(s32 pos) {
    switch (pos) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        return 0x10D;
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
        return 0xDC;
    case 24:
    case 25:
    case 26:
    case 27:
        return 0xC9;
    case 28:
    case 29:
        return 0xA7;
    case 30:
    case 31:
        return 0x89;
    case 32:
        return 0x67;
    }
    return 0;
}

/* Builds the fixed table of the 16 matches. */
void Bracket_InitMatches(LBracket *b) {
    memset(b->match_, 0, sizeof(b->match_));
    b->match_[0].ent[0] = 0;
    b->match_[0].ent[1] = 1;
    b->match_[1].ent[0] = 2;
    b->match_[1].ent[1] = 3;
    b->match_[2].ent[0] = 4;
    b->match_[2].ent[1] = 5;
    b->match_[3].ent[0] = 6;
    b->match_[3].ent[1] = 7;
    b->match_[4].ent[0] = 8;
    b->match_[4].ent[1] = 9;
    b->match_[5].ent[0] = 10;
    b->match_[5].ent[1] = 11;
    b->match_[6].ent[0] = 12;
    b->match_[6].ent[1] = 13;
    b->match_[7].ent[0] = 14;
    b->match_[7].ent[1] = 15;
    b->match_[15].ent[0] = 16;

    b->match_[0].flags = LTOUR_MATCH_TO_LEFT;
    b->match_[0].next = 8;
    b->match_[0].pos = 16;
    b->match_[0].vsAnim = 0;
    b->match_[0].view = 0;
    b->match_[0].winAnim = 0;
    b->match_[1].flags = LTOUR_MATCH_TO_RIGHT;
    b->match_[1].next = 8;
    b->match_[1].pos = 17;
    b->match_[1].vsAnim = 0;
    b->match_[1].view = 0;
    b->match_[1].winAnim = 0;
    b->match_[2].flags = LTOUR_MATCH_TO_LEFT;
    b->match_[2].next = 9;
    b->match_[2].pos = 18;
    b->match_[2].vsAnim = 0;
    b->match_[2].view = 1;
    b->match_[2].winAnim = 0;
    b->match_[3].flags = LTOUR_MATCH_TO_RIGHT;
    b->match_[3].next = 9;
    b->match_[3].pos = 19;
    b->match_[3].vsAnim = 0;
    b->match_[3].view = 2;
    b->match_[3].winAnim = 0;
    b->match_[4].flags = LTOUR_MATCH_TO_LEFT;
    b->match_[4].next = 10;
    b->match_[4].pos = 20;
    b->match_[4].vsAnim = 0;
    b->match_[4].view = 3;
    b->match_[4].winAnim = 0;
    b->match_[5].flags = LTOUR_MATCH_TO_RIGHT;
    b->match_[5].next = 10;
    b->match_[5].pos = 21;
    b->match_[5].vsAnim = 0;
    b->match_[5].view = 4;
    b->match_[5].winAnim = 0;
    b->match_[6].flags = LTOUR_MATCH_TO_LEFT;
    b->match_[6].next = 11;
    b->match_[6].pos = 22;
    b->match_[6].vsAnim = 0;
    b->match_[6].view = 5;
    b->match_[6].winAnim = 0;
    b->match_[7].flags = LTOUR_MATCH_TO_RIGHT | LTOUR_MATCH_ROUND_END;
    b->match_[7].next = 11;
    b->match_[7].pos = 23;
    b->match_[7].vsAnim = 0;
    b->match_[7].view = 5;
    b->match_[7].winAnim = 0;
    b->match_[8].flags = LTOUR_MATCH_TO_LEFT;
    b->match_[8].next = 12;
    b->match_[8].pos = 24;
    b->match_[8].vsAnim = 1;
    b->match_[8].view = 0;
    b->match_[8].winAnim = 1;
    b->match_[9].flags = LTOUR_MATCH_TO_RIGHT;
    b->match_[9].next = 12;
    b->match_[9].pos = 25;
    b->match_[9].vsAnim = 1;
    b->match_[9].view = 6;
    b->match_[9].winAnim = 1;
    b->match_[10].flags = LTOUR_MATCH_TO_LEFT;
    b->match_[10].next = 13;
    b->match_[10].pos = 26;
    b->match_[10].vsAnim = 1;
    b->match_[10].view = 7;
    b->match_[10].winAnim = 1;
    b->match_[11].flags = LTOUR_MATCH_TO_RIGHT | LTOUR_MATCH_ROUND_END;
    b->match_[11].next = 13;
    b->match_[11].pos = 27;
    b->match_[11].vsAnim = 1;
    b->match_[11].view = 5;
    b->match_[11].winAnim = 1;
    b->match_[12].flags = LTOUR_MATCH_TO_LEFT;
    b->match_[12].next = 14;
    b->match_[12].pos = 28;
    b->match_[12].vsAnim = 2;
    b->match_[12].view = 8;
    b->match_[12].winAnim = 2;
    b->match_[13].flags = LTOUR_MATCH_TO_RIGHT | LTOUR_MATCH_ROUND_END;
    b->match_[13].next = 14;
    b->match_[13].pos = 29;
    b->match_[13].vsAnim = 2;
    b->match_[13].view = 9;
    b->match_[13].winAnim = 2;
    b->match_[14].flags = LTOUR_MATCH_TO_RIGHT | LTOUR_MATCH_ROUND_END;
    b->match_[14].next = 15;
    b->match_[14].pos = 31;
    b->match_[14].vsAnim = 3;
    b->match_[14].view = 10;
    b->match_[14].winAnim = 3;
    b->match_[15].flags = LTOUR_MATCH_FINAL | LTOUR_MATCH_ROUND_END;
    b->match_[15].pos = 32;
    b->match_[15].vsAnim = 4;
    b->match_[15].view = 11;
    b->match_[15].winAnim = 4;
}

/* 1 when no entrant chosen by a player is still in the tournament. */
s32 Bracket_IsPlayerOut(LBracket *b) {
    s32 i;

    for (i = 0; i < LTOUR_ENTRANT_MAX; i++) {
        if ((b->entrant[i].flags & LTOUR_ENT_PLAYER) && !(b->entrant[i].flags & LTOUR_ENT_LOST)) {
            break;
        }
    }
    return i == LTOUR_ENTRANT_MAX;
}

/*
 * Fills the 17 entrants: the players' choices from gProgress first, then random characters of the grid whose cost
 * fits the level (level 0: below 4, level 1: below 8, level 2: 7 or more; given up after 15 misses in a row), no
 * character twice and never the boss. The boss (entrant 16) depends on the tournament.
 */
void Bracket_FillEntrants(LBracket *b) {
    s32 miss = 0;
    s32 boss = -1;
    s32 i;

    switch (LTOUR_PROG->tour) {
    case LTOUR_WORLD:
        boss = Rand_Range(2) ? 0x42 : 0x38;
        break;
    case LTOUR_BIG:
        boss = 0x37;
        break;
    case LTOUR_CELL:
        boss = Rand_Range(2) ? 0x6B : 0x6C;
        break;
    case LTOUR_OTHERWORLD:
        boss = Rand_Range(2) ? 0x3D : 6;
        break;
    case LTOUR_YAMCHA:
        boss = 0x1A;
        break;
    }
    for (i = 0; i < LTOUR_ENTRANT_MAX; i++) {
        LChrCell *cell;
        s32 chara;
        s32 j;

        if (i < LTOUR_PROG->entryNum) {
            memcpy(&b->entrant[i], &LTOUR_PROG->entrant[i], sizeof(LTourEntrant));
        } else {
            for (;;) {
                /* written as integer arithmetic: `&b->grid[n]` adds the two operands the other way round */
                cell = (LChrCell *)(Rand_Range(b->gridCount) * sizeof(LChrCell) + (u32)b->grid);
                if (cell->id <= LCHR_ID_MAX) {
                    chara = cell->id;
                    break;
                }
            }
            if (cell->formCount != 0) {
                chara = cell->form[Rand_Range(cell->formCount)];
            }
            if (LTOUR_PROG->level == 0) {
                if (ChrTbl_GetCost(chara) >= 4 && miss < 15) {
                    miss++;
                    i--;
                    continue;
                }
            } else if (LTOUR_PROG->level == 1) {
                if (ChrTbl_GetCost(chara) >= 8 && miss < 15) {
                    miss++;
                    i--;
                    continue;
                }
            } else {
                if (ChrTbl_GetCost(chara) < 7 && miss < 15) {
                    miss++;
                    i--;
                    continue;
                }
            }
            if (i > 0) {
                for (j = 0; j < i; j++) {
                    if (b->entrant[j].chara == chara) {
                        break;
                    }
                }
                if (j != i) {
                    i--;
                    continue;
                }
            }
            if (boss != -1 && boss == chara) {
                i--;
                continue;
            }
            miss = 0;
            b->entrant[i].chara = chara;
        }
    }
    if (boss != -1) {
        b->entrant[16].pos = 30;
        b->entrant[16].chara = boss;
        b->entrant[16].flags |= LTOUR_ENT_BOSS;
    }
}

/* Shuffles the 16 first-round entrants, puts a player to the left of a CPU in each pair, numbers the leaves. */
void Bracket_ShuffleEntrants(LBracket *b) {
    LTourEntrant tmp;
    s32 i;

    for (i = 0; i < 16; i++) {
        s32 r = Rand_Range(16);

        tmp = b->entrant[r];
        b->entrant[r] = b->entrant[i];
        b->entrant[i] = tmp;
    }
    for (i = 0; i < 16; i += 2) {
        if (!(b->entrant[i].flags & LTOUR_ENT_PLAYER) && (b->entrant[i + 1].flags & LTOUR_ENT_PLAYER)) {
            tmp = b->entrant[i + 1];
            b->entrant[i + 1] = b->entrant[i];
            b->entrant[i] = tmp;
        }
    }
    for (i = 0; i < 16; i++) {
        b->entrant[i].pos = i;
    }
}

/* Gives the seventeen chips of the tree movie the entrants' small pictures. */
void Bracket_SetChipTex(LBracket *b) {
    s32 tbl[LTOUR_ENTRANT_MAX] = {6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    s32 i;

    for (i = 0; i < LTOUR_ENTRANT_MAX; i++) {
        b->texTree[tbl[i]] = MTEX((MTexRes *)MPACK_AT(b->chips, b->entrant[i].chara + 1), 0);
    }
}

/* Plays the CPU-only matches in order until the current round's last match is passed. */
void Bracket_PlayCpuMatches(LBracket *b) {
    s32 round = 0;
    s32 i;

    for (i = 0; i < LTOUR_MATCH_MAX; i++) {
        if ((b->entrant[b->match_[i].ent[0]].flags & LTOUR_ENT_PLAYER) ||
            (b->entrant[b->match_[i].ent[1]].flags & LTOUR_ENT_PLAYER)) {
            if (b->match_[i].flags & LTOUR_MATCH_ROUND_END) {
                round++;
                if (b->round < round) {
                    return;
                }
            }
        } else {
            Bracket_PlayCpuMatch(b, i);
            if (b->match_[i].flags & LTOUR_MATCH_ROUND_END) {
                round++;
                if (b->round < round) {
                    return;
                }
            }
        }
    }
}

/* Steps to the next match with a player in it; a round's last match starts the "round over" speech. */
void Bracket_NextMatch(LBracket *b) {
    do {
        if (b->match_[b->match].flags & LTOUR_MATCH_ROUND_END) {
            gProgress->flags &= ~MPROG_FLAG20;
            b->round++;
            if (b->round < LTOUR_ROUND_NUM) {
                b->seq = LBRK_SEQ_ROUND_END;
            }
        }
        b->match++;
        if (b->entrant[b->match_[b->match].ent[0]].flags & LTOUR_ENT_PLAYER) {
            break;
        }
    } while (!(b->entrant[b->match_[b->match].ent[1]].flags & LTOUR_ENT_PLAYER));
}

/*
 * Takes over the result of the battle. The pad side is battle side 0: the left entrant when it is a player, else
 * the right one. The loser is marked only while Bracket_IsPlayerOut looks (the mark is taken back at once; the
 * result sequence sets it for good), the winner keeps its remaining health.
 */
void Bracket_ApplyResult(LBracket *b) {
    s32 flags = BattleResult_GetFlags();
    LBattleResult *res = BattleResult_GetPtr();

    if (flags & 1) {
        if (b->entrant[b->match_[b->match].ent[0]].flags & LTOUR_ENT_PLAYER) {
            b->entrant[b->match_[b->match].ent[1]].flags |= LTOUR_ENT_LOST;
            b->winSide = 0;
            b->flags |= LBRK_RESULT;
            if (Bracket_IsPlayerOut(b)) {
                b->flags |= LBRK_PLAYER_OUT;
            }
            b->entrant[b->match_[b->match].ent[0]].health = res->health[0];
            b->entrant[b->match_[b->match].ent[1]].flags ^= LTOUR_ENT_LOST;
        } else {
            b->entrant[b->match_[b->match].ent[0]].flags |= LTOUR_ENT_LOST;
            b->winSide = 1;
            b->flags |= LBRK_RESULT;
            if (Bracket_IsPlayerOut(b)) {
                b->flags |= LBRK_PLAYER_OUT;
            }
            b->entrant[b->match_[b->match].ent[1]].health = res->health[0];
            b->entrant[b->match_[b->match].ent[0]].flags ^= LTOUR_ENT_LOST;
        }
    } else if (flags & 2) {
        if (b->entrant[b->match_[b->match].ent[0]].flags & LTOUR_ENT_PLAYER) {
            b->entrant[b->match_[b->match].ent[0]].flags |= LTOUR_ENT_LOST;
            b->winSide = 1;
            b->flags |= LBRK_RESULT;
            if (Bracket_IsPlayerOut(b)) {
                b->flags |= LBRK_PLAYER_OUT;
            }
            b->entrant[b->match_[b->match].ent[1]].health = res->health[1];
            b->entrant[b->match_[b->match].ent[0]].flags ^= LTOUR_ENT_LOST;
        } else {
            b->entrant[b->match_[b->match].ent[1]].flags |= LTOUR_ENT_LOST;
            b->winSide = 0;
            b->flags |= LBRK_RESULT;
            if (Bracket_IsPlayerOut(b)) {
                b->flags |= LBRK_PLAYER_OUT;
            }
            b->entrant[b->match_[b->match].ent[0]].health = res->health[1];
            b->entrant[b->match_[b->match].ent[1]].flags ^= LTOUR_ENT_LOST;
        }
    }
    b->entrant[b->match_[b->match].ent[0]].flags &= ~LTOUR_ENT_HIDDEN;
    b->entrant[b->match_[b->match].ent[1]].flags &= ~LTOUR_ENT_HIDDEN;
    b->match_[b->match].flags |= LTOUR_MATCH_DONE;
}

/* Starts the movie of the current match's two chips meeting, and starts loading their large pictures. */
void Bracket_StartVs(LBracket *b) {
    char name[64];
    MFlash *flash = &b->flash[1];
    MTexRes *res;

    b->loadState = LBRK_LOAD_REQUEST;
    b->flags |= LBRK_MOVE_A;
    res = (MTexRes *)MPACK_AT(b->chips, b->entrant[b->match_[b->match].ent[0]].chara + 1);
    b->texMoveA[1] = MTEX(res, 0);
    res = (MTexRes *)MPACK_AT(b->chips, b->entrant[b->match_[b->match].ent[1]].chara + 1);
    b->texMoveA[4] = MTEX(res, 0);
    b->scroll = -(f32)Bracket_GetViewX(b->match_[b->match].view);
    Flash_SetOffset(&b->flash[0], b->scroll, 0);
    b->entrant[b->match_[b->match].ent[0]].flags |= LTOUR_ENT_HIDDEN;
    b->entrant[b->match_[b->match].ent[1]].flags |= LTOUR_ENT_HIDDEN;
    Flash_Play(flash, 1);
    sprintf(name, "fr_vs_action_%d", b->match_[b->match].vsAnim + 1);
    Flash_GotoLabel(flash, name, 1);
}

/* Starts the movie of the winner's chip moving up to the next node. */
void Bracket_StartWin(LBracket *b) {
    char name[64];
    MFlash *flash = &b->flash[2];
    MTexRes *res;

    b->flags |= LBRK_MOVE_B;
    res = (MTexRes *)MPACK_AT(b->chips, b->entrant[b->match_[b->match].ent[0]].chara + 1);
    b->texMoveB[1] = MTEX(res, 0);
    res = (MTexRes *)MPACK_AT(b->chips, b->entrant[b->match_[b->match].ent[1]].chara + 1);
    b->texMoveB[4] = MTEX(res, 0);
    b->scroll = -(f32)Bracket_GetViewX(b->match_[b->match].view);
    Flash_SetOffset(&b->flash[0], b->scroll, 0);
    b->entrant[b->match_[b->match].ent[0]].flags |= LTOUR_ENT_HIDDEN;
    b->entrant[b->match_[b->match].ent[1]].flags |= LTOUR_ENT_HIDDEN;
    Flash_Play(flash, 1);
    sprintf(name, b->winSide != 0 ? "fr_win_action_%dR" : "fr_win_action_%dL", b->match_[b->match].winAnim + 1);
    Flash_GotoLabel(flash, name, 1);
}

/* One step of the background loader of the two fighters' large pictures (files 0x2F9 + character). */
void Bracket_UpdateImages(LBracket *b) {
    s32 chara[2];
    MTexRes *res;
    s32 *p = chara; /* through a pointer: `memset(chara, 0, 8)` is expanded in line, the original calls memset */

    memset(p, 0, 8);
    switch (b->loadState) {
    case LBRK_LOAD_REQUEST:
        b->texVs[6] = NULL;
        b->texVs[5] = NULL;
        chara[0] = b->entrant[b->match_[b->match].ent[0]].chara;
        chara[1] = b->entrant[b->match_[b->match].ent[1]].chara;
        File_CancelRequests();
        File_Request(chara[0] + 0x2F9, b->imageFile[0], 0x16800);
        File_Request(chara[1] + 0x2F9, b->imageFile[1], 0x16800);
        b->loadState = LBRK_LOAD_READ;
        break;
    case LBRK_LOAD_READ:
        if (File_UpdateRequests()) {
            b->loadState = LBRK_LOAD_UNPACK;
        }
        break;
    case LBRK_LOAD_UNPACK:
        Sprite_Unpack(b->imageFile[0], b->imageRes[0], NULL);
        Sprite_Unpack(b->imageFile[1], b->imageRes[1], NULL);
        res = b->imageRes[0];
        Res_RelocateOffsets(&res, res, res);
        b->texVs[6] = MTEX(res, 0);
        res = b->imageRes[1];
        Res_RelocateOffsets(&res, res, res);
        b->texVs[5] = MTEX(res, 0);
        b->flags |= LBRK_IMAGES;
        b->loadState = 0;
        break;
    }
}

/* Loads the two fighters' large pictures at once (when the screen comes back from a battle). */
void Bracket_LoadImages(LBracket *b) {
    s32 chara[2];
    MTexRes *res;
    s32 *p = chara; /* through a pointer: `memset(chara, 0, 8)` is expanded in line, the original calls memset */

    memset(p, 0, 8);
    chara[0] = b->entrant[b->match_[b->match].ent[0]].chara;
    chara[1] = b->entrant[b->match_[b->match].ent[1]].chara;
    File_LoadSync(chara[0] + 0x2F9, b->imageFile[0], 0x16800);
    File_LoadSync(chara[1] + 0x2F9, b->imageFile[1], 0x16800);
    Sprite_Unpack(b->imageFile[0], b->imageRes[0], NULL);
    Sprite_Unpack(b->imageFile[1], b->imageRes[1], NULL);
    res = b->imageRes[0];
    Res_RelocateOffsets(&res, res, res);
    b->texVs[6] = MTEX(res, 0);
    res = b->imageRes[1];
    Res_RelocateOffsets(&res, res, res);
    b->texVs[5] = MTEX(res, 0);
}

/* Draws a random stage among the unlocked ones of a list: 8 stages for the Yamcha Game, 23 for the others. */
s32 Bracket_PickStage(s32 yamcha) {
    s32 list[36];
    s32 count = 0;
    u32 i;

    if (yamcha) {
        s32 tbl[8] = {2, 3, 7, 8, 13, 16, 18, 34};

        for (i = 0; i < 8; i++) {
            if ((LSAVE->stageBits >> tbl[i]) & 1) {
                list[count] = tbl[i];
                count++;
            }
        }
        return list[Rand_Range(count)];
    } else {
        s32 tbl[23] = {0, 1, 5, 9, 10, 11, 12, 14, 15, 17, 19, 20, 21, 22, 23, 24, 25, 26, 29, 30, 31, 32, 33};

        for (i = 0; i < 23; i++) {
            if ((LSAVE->stageBits >> tbl[i]) & 1) {
                list[count] = tbl[i];
                count++;
            }
        }
        return list[Rand_Range(count)];
    }
}

/*
 * The battle hand-off: sets up the current match. A player is always battle side 0 on pad 0; a second player is
 * side 1 on pad 1 (split screen), a CPU is side 1 with the level and items of the table [level][round]. In the
 * Cell Games the players' health carries over from the last battle plus 20, capped at 100.
 */
void Bracket_SetupBattle(LBracket *b) {
    s32 chara[2];
    s32 costume[2];
    LItemSet item[2];
    f32 health[2];
    s32 stage = 0;
    s32 bgm = 0;
    s32 announcer = 0;
    s32 screenMode;
    s32 i;

    Battle_ClearWork();
    if (b->entrant[b->match_[b->match].ent[0]].flags & LTOUR_ENT_PLAYER) {
        if (b->entrant[b->match_[b->match].ent[1]].flags & LTOUR_ENT_PLAYER) {
            BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, 0);
            BattleSetup_SetSide(1, 0, 1, 1, 1, 1, 0, 0);
            b->flags |= LBRK_PVP;
            chara[0] = b->entrant[b->match_[b->match].ent[0]].chara;
            chara[1] = b->entrant[b->match_[b->match].ent[1]].chara;
            costume[0] = b->entrant[b->match_[b->match].ent[0]].costume;
            costume[1] = b->entrant[b->match_[b->match].ent[1]].costume;
            item[0] = b->entrant[b->match_[b->match].ent[0]].item;
            item[1] = b->entrant[b->match_[b->match].ent[1]].item;
            screenMode = 1;
            if (LTOUR_PROG->tour == LTOUR_CELL) {
                health[0] = b->round != 0 ? b->entrant[b->match_[b->match].ent[0]].health + 20.0f : 100.0f;
                health[1] = b->round != 0 ? b->entrant[b->match_[b->match].ent[1]].health + 20.0f : 100.0f;
                if (health[0] > 100.0f) {
                    health[0] = 100.0f;
                }
                if (health[1] > 100.0f) {
                    health[1] = 100.0f;
                }
            } else {
                health[0] = 100.0f;
                health[1] = 100.0f;
            }
        } else {
            BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, 0);
            BattleSetup_SetSide(1, 2, 1, 1, 1, 1, 0, 0);
            b->flags |= LBRK_LEFT_PAD;
            screenMode = 0;
            chara[0] = b->entrant[b->match_[b->match].ent[0]].chara;
            chara[1] = b->entrant[b->match_[b->match].ent[1]].chara;
            costume[0] = b->entrant[b->match_[b->match].ent[0]].costume;
            costume[1] = b->entrant[b->match_[b->match].ent[1]].costume;
            item[0] = b->entrant[b->match_[b->match].ent[0]].item;
            item[1] = b->cpu[LTOUR_PROG->level * LTOUR_ROUND_NUM + b->round].item;
            for (i = 0; i < 8; i++) {
                item[1].id[i]++;
            }
            if (LTOUR_PROG->tour == LTOUR_CELL) {
                health[0] = b->round != 0 ? b->entrant[b->match_[b->match].ent[0]].health + 20.0f : 100.0f;
                health[1] = 100.0f;
                if (health[0] > 100.0f) {
                    health[0] = 100.0f;
                }
            } else {
                health[0] = 100.0f;
                health[1] = 100.0f;
            }
        }
    } else {
        BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, 0);
        BattleSetup_SetSide(1, 2, 1, 1, 1, 1, 0, 0);
        b->flags |= LBRK_RIGHT_PAD;
        screenMode = 0;
        chara[0] = b->entrant[b->match_[b->match].ent[1]].chara;
        chara[1] = b->entrant[b->match_[b->match].ent[0]].chara;
        costume[0] = b->entrant[b->match_[b->match].ent[1]].costume;
        costume[1] = b->entrant[b->match_[b->match].ent[0]].costume;
        item[0] = b->entrant[b->match_[b->match].ent[1]].item;
        item[1] = b->cpu[LTOUR_PROG->level * LTOUR_ROUND_NUM + b->round].item;
        for (i = 0; i < 8; i++) {
            item[1].id[i]++;
        }
        if (LTOUR_PROG->tour == LTOUR_CELL) {
            health[0] = b->round != 0 ? b->entrant[b->match_[b->match].ent[1]].health + 20.0f : 100.0f;
            health[1] = 100.0f;
            if (health[0] > 100.0f) {
                health[0] = 100.0f;
            }
        } else {
            health[0] = 100.0f;
            health[1] = 100.0f;
        }
    }
    switch (LTOUR_PROG->tour) {
    case LTOUR_WORLD:
        stage = b->round == 4 ? 0x1B : 4;
        announcer = 4;
        bgm = 0x12;
        break;
    case LTOUR_BIG:
        announcer = 5;
        stage = Bracket_PickStage(0);
        bgm = Rand_Range(9) + 8;
        break;
    case LTOUR_CELL:
        stage = b->round == 4 ? 6 : 0x1C;
        announcer = 6;
        bgm = Rand_Range(9) + 8;
        break;
    case LTOUR_OTHERWORLD:
        announcer = 2;
        stage = Bracket_PickStage(0);
        bgm = Rand_Range(9) + 8;
        break;
    case LTOUR_YAMCHA:
        announcer = 3;
        stage = Bracket_PickStage(1);
        bgm = Rand_Range(9) + 8;
        break;
    }
    BattleSetup_SetRule(screenMode, 4, bgm, 0, announcer, stage, 1);
    for (i = 0; i < 2; i++) {
        BattleSetup_SetMember(i, 0, chara[i], costume[i], 0,
                              b->cpu[LTOUR_PROG->level * LTOUR_ROUND_NUM + b->round].cpuLevel, health[i], item[i].id);
    }
    BattleSetup_Finish();
}

/*
 * Pays out the prizes of the tournament (table [tournament].prize[kind][level]) and lists them for the reward
 * window. `second` = the player was the runner-up: only the second-place money. A dragon ball may come on top.
 */
void Bracket_GivePrizes(LBracket *b, s32 second) {
    s32 value;
    s32 i;

    memset(b->reward, 0, sizeof(b->reward));
    b->rewardCount = 0;
    for (i = 0; i < LPRIZE_NUM; i++) {
        value = b->prize[LTOUR_PROG->tour].prize[i][LTOUR_PROG->level];

        if (value < 0) {
            continue;
        }
        if (second == 0) {
            switch (i) {
            case LPRIZE_MONEY_WIN:
                b->reward[b->rewardCount].kind = 6;
                b->reward[b->rewardCount].value = value;
                b->rewardCount++;
                Save_AddMoney(value);
                break;
            case LPRIZE_ITEM:
                if (!(LSAVE->item[value] & SAVE_ITEM_OWNED)) {
                    b->reward[b->rewardCount].kind = 2;
                    b->reward[b->rewardCount].value = value;
                    b->rewardCount++;
                    Save_AddItem(value);
                }
                break;
            case LPRIZE_CHARA_A:
            case LPRIZE_CHARA_B:
                if (!(s32)((LSAVE->charaBits[value / 64] >> (value % 64)) & 1)) {
                    b->reward[b->rewardCount].kind = 0;
                    b->reward[b->rewardCount].value = value;
                    b->rewardCount++;
                    LSAVE->charaBits[value / 64] |= 1L << (value % 64);
                }
                break;
            case LPRIZE_STAGE:
                if (!(s32)((LSAVE->stageBits >> value) & 1)) {
                    b->reward[b->rewardCount].kind = 1;
                    b->reward[b->rewardCount].value = value;
                    b->rewardCount++;
                    LSAVE->stageBits |= 1L << value;
                }
                break;
            case LPRIZE_MONEY_SECOND:
            case 6:
                break;
            }
        } else if (i == LPRIZE_MONEY_SECOND) {
            b->reward[b->rewardCount].kind = 6;
            b->reward[b->rewardCount].value = value;
            b->rewardCount++;
            Save_AddMoney(value);
        }
    }
    value = Bracket_DrawDragonBall(LTOUR_PROG->tour, LTOUR_PROG->level, second);
    if (value >= 0) {
        b->reward[b->rewardCount].kind = 9;
        b->reward[b->rewardCount].value = value;
        b->rewardCount++;
        LSAVE->unlockFlags |= 1 << value;
    }
}

/*
 * Draws a dragon ball: -1 when all seven are owned or when Rand_Range(100) misses the chance (percent, by
 * tournament, place and level), else a random one of the missing balls.
 */
s32 Bracket_DrawDragonBall(s32 tour, s32 level, s32 second) {
    s32 list[8];
    s32 count = 0;
    s32 chance[LTOUR_NUM][2][3] = {
        {{20, 30, 40}, {5, 10, 15}}, {{20, 30, 40}, {5, 10, 15}}, {{20, 30, 40}, {5, 10, 15}},
        {{20, 30, 40}, {5, 10, 15}}, {{40, 50, 60}, {15, 20, 25}},
    };
    s32 i;

    if ((LSAVE->unlockFlags & 0x7F) == 0x7F) {
        return -1;
    }
    if ((s32)Rand_Range(100) >= chance[tour][second][level]) {
        return -1;
    }
    for (i = 0; i < 7; i++) {
        if (!(LSAVE->unlockFlags & (s32)(1U << i))) {
            list[count] = i;
            count++;
        }
    }
    return list[Rand_Range(count)];
}
