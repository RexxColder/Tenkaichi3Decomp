#ifndef MENU_MENU_R_H
#define MENU_MENU_R_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (see include/menu/menu_h.h). */
#define Snd_PlaySe Snd_PlaySe_menuA
#include "menu/menu_a.h"
#undef Snd_PlaySe
extern s32 Snd_PlaySe(u32 mask, s32 id);

/*
 * Menu overlay DBZP.BIN, 0x3840E0..0x388618 (placeholder stem "menu_r"): three screens of the mode group 13..30
 * (handler 0x379A58, archive gMenuArc3, main-menu item 1), sub family 20..23. The clip names of its movies call the
 * mode "sim" ("mc_sim_botan", "mc_sim_monita", "fl_syugyo_*" = training, "mc_turn_lamp"): a ladder of seven rounds
 * of ten turns, nine turns of training and events and a fight on the tenth. Four pieces, cut at object boundaries:
 *
 *   menu_r.c    0x3840E0..0x3851B0  SimDay      tail of the board screen (mode 22; the object starts in the
 *                                               previous chunk, src/menu/menu_q_b.c; work pointer 0x3B7384): the
 *                                               guide's closing line, the pad handler, the portrait loader and
 *                                               the frame loop
 *   menu_r_b.c  0x3851B0..0x3851E8  SimEvent    the event dispatcher (table 0x3B7388: 37 scripts that live in
 *                                               the chunks menu_s / menu_t / menu_u)
 *   menu_r_c.c  0x3851E8..0x387880  SimResult   the result screen after a round's fight (mode 23), a whole
 *                                               object (work pointer 0x3B741C)
 *   menu_r_d.c  0x387880..0x388618  SimTop      head of the ladder's entry screen with the ranking list (mode 20;
 *                                               work pointer 0x3B7420); the object goes on in src/menu/menu_s.c
 *
 * All names are guesses from what the code does. The structures are this chunk's own views; where the
 * neighbouring chunks' headers (menu_p.h, menu_q.h, menu_s.h) describe the same thing the field names agree.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern s32 Rand_Libc(void);           /* libc rand() */
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk50(MTextBox *box, s32 value);
extern void TextBox_SetLineOffsets(MTextBox *box, s32 y1, s32 y2, s32 y3, s32 y4, s32 y5);
extern void TextBox_SetSpacing(MTextBox *box, s32 x, s32 y);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void Num_Draw(MFlash *flash, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h, s32 mode);
extern void Num_DrawChild(MFlash *flash, char *parent, char *fmt, s32 first, s32 count, s32 value, s32 w, s32 h,
                          s32 mode, s32 parentFmt);
extern void IconWin_SetIcon(s32 icon);
extern void Voice_StopWithLip(void);
extern void Dialog_SetMsg(s32 idx);
extern s32 Dialog_Input(s32 allowCancel);
extern void Dialog_SetChoices(s32 on);
extern void Dialog_Start(s32 kind);
extern s32 Dialog_IsClosed(void);
extern void Save_AddItem(s32 idx);

/* Voice_GetStat result when nothing is playing. */
#define SIM_VOICE_IDLE 5
/* Voice bank base of this mode group's guides (Voice_PlayWithSubtitle). */
#define SIM_VOICE_BASE 0x8765
#define SIM_FACE_FILE 0x2F9      /* + character id: compressed portrait */
#define SIM_FACE_SIZE 0x16800
#define SIM_BGM_FIRST 0x10B16    /* Bgm_Play id of music 0 */
#define SIM_MONEY_MAX 9999999

/*
 * The state of the ladder, kept between its screens and its fights (gProgress + 0x64C; SimState of
 * include/menu/menu_q.h, same field names). That it is a structure of its own is also what the address arithmetic
 * of SimTop_Init's loop over `item` says (base 0x650, offset 0x14).
 */
#define SIM_STAT_ATK 0
#define SIM_STAT_DEF 1
#define SIM_STAT_HP 2
#define SIM_STAT_POINT 3
#define SIM_STAT_NUM 4
#define SIM_ITEM_NUM 3
#define SIM_TURNS 10            /* turns of a round; the fight is on the last one */
#define SIM_LAST_TURN 0x45      /* 69: the fight of the seventh round */

typedef struct SimRun {
    /* 0x00 (0x64C) */ s32 level;
    /* 0x04 (0x650) */ s32 stat[SIM_STAT_NUM]; /* attack, defence, health in per cent, points / 100 */
    /* 0x14 (0x660) */ s32 unk14;
    /* 0x18 (0x664) */ s32 item[SIM_ITEM_NUM]; /* item ids carried into the fight, -1 = none */
    /* 0x24 (0x670) */ s32 itemHead;
    /* 0x28 (0x674) */ s32 turn;         /* turns played; / 10 = round, % 10 = turn of the round. The mode handler adds 1
                                            after a result screen that says "go on" */
    /* 0x2C (0x678) */ s32 unk2C;
    /* 0x30 (0x67C) */ s32 wait;         /* turns until button 3 of the board is open again (set to 30 when it is used) */
    /* 0x34 (0x680) */ s32 off;          /* bit n: button n of the board is greyed out (the cursor skips it) */
} SimRun; /* 0x38 */

/* gProgress as the sim screens use it (QProgress of include/menu/menu_q.h; menu_m.h's UbProgress has the selects' part). */
typedef struct SimProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ void *unk8[3];
    /* 0x014 */ s32 flags;
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x440];
    /* 0x45C */ s32 chara;      /* the player's character (member record at 0x440, +0x1C; written by the select of mode 21) */
    /* 0x460 */ u8 unk460[0x1E4];
    /* 0x644 */ s32 teamSize;   /* 1: the select of mode 21 is the one-character select */
    /* 0x648 */ s32 dpRule;
    /* 0x64C */ SimRun run;
} SimProgress;

#define SIM_PROG ((SimProgress *)gProgress)

/* gSaveData as these screens use it (include/sys/save.h has the full layout). */
typedef struct SimRankRec {
    /* 0x00 */ s32 chara;
    /* 0x04 */ s32 score;
    /* 0x08 */ u8 cleared;      /* 1 = the ladder was finished: shows "mc_icon_star" */
    /* 0x09 */ u8 unk9[3];
} SimRankRec; /* 0xC */

typedef struct SimSave {
    /* 0x0000 */ u8 unk0[0x210];
    /* 0x0210 */ SimRankRec rank[10];
    /* 0x0288 */ u8 simCleared;   /* the reward item of a finished ladder was given */
    /* 0x0289 */ u8 unk289[0x3028 - 0x289];
    /* 0x3028 */ s32 money;
} SimSave;

#define SIM_SAVE ((SimSave *)gSaveData)
extern void *gSaveData;

/* ---- SimDay (menu_r.c): the part of the previous chunk's work area that its last functions use. A local view:
   include/menu/menu_q.h has the whole layout; the names are the same, except that
   faceTex[0] and faceTex[1] are tex6[13] and tex6[14] there. ---- */

#define SIMDAY_STATE_NUM 13

typedef struct SimDay {
    /* 0x000 */ u8 unk0[8];
    /* 0x008 */ void *faceFile[2];  /* compressed portraits: the player's character, the round's opponent */
    /* 0x010 */ MTexRes *faceRes[2]; /* the same unpacked */
    /* 0x018 */ u8 unk18[8];
    /* 0x020 */ MFlash flash[7];
    /* 0x154 */ u8 unk154[0x23C];
    /* 0x390 */ u8 *faceTex[2];     /* textures 13 and 14 of the round movie: the opponent's portrait, the player's */
    /* 0x398 */ u8 unk398[0x1B8];
    /* 0x550 */ s32 flags;          /* SIMDAY_ */
    /* 0x554 */ s32 loadState;      /* SIMDAY_LOAD_ */
    /* 0x558 */ s32 cur[SIMDAY_STATE_NUM]; /* cursor of each state */
    /* 0x58C */ s32 timer;          /* frames until the fade out starts */
    /* 0x590 */ s32 state;          /* SIMDAY_ST_ */
    /* 0x594 */ s32 prevState;
    /* 0x598 */ s32 talk[2];        /* [0] the guide's closing line to say (1..5), 0 = none; [1] the last one said */
    /* 0x5A0 */ s32 msgLine;        /* line of the message window, -1 = none */
    /* 0x5A4 */ s32 menuText;       /* 1 = the window shows the item names, 0 = its plates */
    /* 0x5A8 */ s32 bgm;
    /* 0x5AC */ s32 wait;           /* frames the round picture stays (300) */
    /* 0x5B0 */ s32 day;            /* turn of the round, 0..9; 9 = the fight */
    /* 0x5B4 */ u8 unk5B4[0x5A4];
    /* 0xB58 */ s32 enemyChara;     /* the opponent of the fight set up */
    /* 0xB5C */ s32 unkB5C[2];
    /* 0xB64 */ s32 event;          /* script of the turn: index into gSimEvent */
    /* 0xB68 */ u8 unkB68[0x24];
    /* 0xB8C */ s32 unkB8C;         /* answer of the two-row menu: 1 = first row */
} SimDay;

#define SIMDAY_DONE 1
#define SIMDAY_LEAVING 2
#define SIMDAY_STARTED 4
#define SIMDAY_GREETED 8
#define SIMDAY_FACES_READY 0x10
#define SIMDAY_BUSY 0x40
#define SIMDAY_WAIT_KEY 0x80

#define SIMDAY_LOAD_IDLE 0
#define SIMDAY_LOAD_REQUEST 1
#define SIMDAY_LOAD_READ 2
#define SIMDAY_LOAD_UNPACK 3

#define SIMDAY_FL_BOARD 0
#define SIMDAY_FL_MENU 5
#define SIMDAY_FL_ROUND 6

#define SIMDAY_ST_BOARD 0       /* the board: four buttons */
#define SIMDAY_ST_TRAIN 1       /* the three trainings (button 0) */
#define SIMDAY_ST_SELECT 2      /* a script's two-row question */
#define SIMDAY_ST_SCRIPT 3      /* the turn's event script runs (gSimEvent) */
#define SIMDAY_ST_WAIT 4
#define SIMDAY_ST_VERSUS 5      /* the portraits load, then the confirm button starts the fight */
#define SIMDAY_ST_WINDOW 6      /* the three-row window (start button) */
#define SIMDAY_ST_WINDOW_ITEMS 7 /* the window shows the carried items */
#define SIMDAY_ST_CONFIRM 8     /* "quit?" dialog (message 14) */
#define SIMDAY_ST_BACK 9
#define SIMDAY_ST_ROUND 10
#define SIMDAY_ST_ROUND_WAIT 11
#define SIMDAY_ST_QUIT 12

extern SimDay *gSimDay;                      /* 0x3B7384 */
extern s32 (*gSimEvent[37])(SimDay *day);    /* 0x3B7388 */

/* previous chunks (menu_p_d.c, menu_q.c: the score sheet; menu_q_b.c: the head of SimDay) */
extern s32 UbScore_Fill(s32 kind, void *score, s32 *pages);
extern void UbScore_CalcPoints(void *price, void *score);
extern s32 UbScore_CalcRank(s32 kind, void *score);
extern s32 UbScore_Transfer(s32 *from, s32 *to, s32 step, f32 rate);
extern s32 UbScore_CountLine(void *score, s32 isBonus, s32 index, s32 step);
extern s32 UbScore_ConvertStep(void *score, s32 step);
extern void UbScore_SetPage(void *score, s32 page);
extern void UbScore_PlateGoto(MFlash *flash, s32 plate, s32 on);
extern void UbScore_AddRanking(s32 score, s32 chara, s32 cleared);
extern s32 UbScore_GetRewardItem(s32 idx);
extern void SimDay_SetupBattle(void);
extern void SimDay_ListItems(void);
extern s32 SimDay_CountItems(void);
extern void SimDay_Cmd(s32 cmd);
extern void SimDay_PickEvent(s32 unused);   /* defined without a parameter; both callers pass one (0 / 1) */
extern void SimDay_RefreshBoard(void);
extern void SimDay_Init(s32 section);
extern void SimDay_Term(void);
extern void SimDay_Draw(void);
extern void SimDay_Update(void);

void SimDay_UpdateTalk(void);
void SimDay_Input(s32 *result);
void SimDay_ClipGoto(s32 movie, s32 kind, char *label);
void SimDay_UpdateFaceLoad(void);
s32 SimDay_Run(s32 section);
s32 SimEvent_Run(SimDay *day, u32 event);

/* ---- SimResult (menu_r_c.c) ---- */

typedef struct SimScoreLine {
    /* 0x00 */ s32 value;       /* what was measured (a bonus line: the bonus id) */
    /* 0x04 */ s32 remain;      /* points not yet counted into the total */
    /* 0x08 */ s32 points;      /* points the line is worth, in hundreds */
} SimScoreLine; /* 0xC */

/* The score sheet (UbScore of include/menu/menu_p.h / QScore of menu_q.h, same field names; a local view). */
typedef struct SimScore {
    /* 0x000 */ s32 total;       /* here: starts as the run's points (gProgress->run.stat[SIM_STAT_POINT]) */
    /* 0x004 */ s32 count;       /* the total as it is turned into money */
    /* 0x008 */ s32 convert;     /* points turned into money so far */
    /* 0x00C */ s32 unkC;
    /* 0x010 */ SimScoreLine line[4];
    /* 0x040 */ SimScoreLine bonus[48];
    /* 0x280 */ s32 shown[3];    /* bonus indices on the page shown */
    /* 0x28C */ s32 lineCount;
    /* 0x290 */ s32 bonusCount;
    /* 0x294 */ u32 ticks;       /* battle clock at the end */
    /* 0x298 */ s16 hours;
    /* 0x29A */ s16 minutes;
    /* 0x29C */ s16 seconds;
    /* 0x29E */ s16 unk29E[3];
    /* 0x2A4 */ s32 rank;        /* 0 = none, 1..4 */
} SimScore; /* 0x2A8 */

#define SIMRESULT_FLASH_NUM 1
#define SIMRESULT_STATE_NUM 20

typedef struct SimResult {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 3 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 15 sections */
    /* 0x008 */ s32 unk8;
    /* 0x00C */ void *text;         /* section 12 */
    /* 0x010 */ void *subtitles;    /* section 11 */
    /* 0x014 */ void *itemText;     /* section 14 */
    /* 0x018 */ MTextBox box[6];
    /* 0x360 */ MTextBox itemBox;
    /* 0x3EC */ void *bonusTbl;     /* section 13 */
    /* 0x3F0 */ MFlash flash[SIMRESULT_FLASH_NUM];
    /* 0x41C */ void *bg;           /* section 6 */
    /* 0x420 */ u8 *tex[49];
    /* 0x4E4 */ s32 flags;          /* SIMRESULT_ */
    /* 0x4E8 */ s32 pick[SIMRESULT_STATE_NUM]; /* per state: the yes / no cursor (1 = no) */
    /* 0x538 */ s32 timer;
    /* 0x53C */ s32 unk53C[2];
    /* 0x544 */ s32 state;          /* SIMRESULT_ST_ */
    /* 0x548 */ s32 talk;           /* guide line to say (1..21), 0 = none */
    /* 0x54C */ s32 talkLast;
    /* 0x550 */ s32 skip;           /* the confirm button cut the line short */
    /* 0x554 */ s32 voiceLine;
    /* 0x558 */ s32 unk558;
    /* 0x55C */ s32 blink;
    /* 0x560 */ s32 mouth;
    /* 0x564 */ s32 pageMax;        /* last page of the bonus list */
    /* 0x568 */ s32 page;
    /* 0x56C */ s32 outcome;        /* UbScore_Fill: 0 won, 1 lost, 2 aborted */
    /* 0x570 */ s32 cleared;        /* the last turn of the ladder was played */
    /* 0x574 */ s32 gotItem;        /* the reward item is given on this visit */
    /* 0x578 */ u8 pageShown;       /* the page's counters are done: play its closing animation */
    /* 0x579 */ u8 unk579[3];
    /* 0x57C */ SimScore score;
    /* 0x824 */ s32 counting;
    /* 0x828 */ s32 step;           /* counter of the row being counted up / of the money conversion */
} SimResult; /* 0x82C */

#define SIMRESULT_DONE 1
#define SIMRESULT_LEAVING 2
#define SIMRESULT_STARTED 4
#define SIMRESULT_GREETED 8

#define SIMRESULT_ST_INTRO 0
#define SIMRESULT_ST_COUNT 1     /* the score rows count up */
#define SIMRESULT_ST_PAGES 2     /* browse the bonus pages, confirm to go on */
#define SIMRESULT_ST_OVER 3      /* "game over" plate */
#define SIMRESULT_ST_CONTINUE 4  /* "continue?" yes / no */
#define SIMRESULT_ST_TRAIN 5
#define SIMRESULT_ST_CLEAR 6
#define SIMRESULT_ST_MONEY 7     /* the score turns into money */
#define SIMRESULT_ST_ITEM 8      /* the reward item */
#define SIMRESULT_ST_END 9

extern SimResult *gSimResult; /* 0x3B741C */

void SimResult_YesNoGoto(char *label);
void SimResult_Init(s32 section);
void SimResult_Term(void);
void SimResult_Draw(void);
void SimResult_Update(void);
void SimResult_UpdateTalk(void);
void SimResult_Input(s32 *result);
s32 SimResult_Run(s32 section);

/* ---- SimTop (menu_r_d.c): head of the object; include/menu/menu_s.h has a view with the same field names ---- */

#define SIMTOP_FLASH_NUM 1
#define SIMTOP_TEX_CURSOR_FACE 26 /* SimTop.tex: the portrait of the record under the cursor */
#define SIMTOP_FACE_NUM 165      /* texture lists in the portrait pack: one per character-grid id */

typedef struct SimTop {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 3 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 10 sections */
    /* 0x008 */ u32 *faces;         /* section 7: a pack of 165 texture lists, one per character */
    /* 0x00C */ void *text;         /* section 8 */
    /* 0x010 */ void *subtitles;    /* section 9 */
    /* 0x014 */ MTextBox box;
    /* 0x0A0 */ MFlash flash[SIMTOP_FLASH_NUM];
    /* 0x0CC */ void *bg;           /* section 4 */
    /* 0x0D0 */ u8 *tex[40];        /* [gSimTopFaceTex[n]] = the portrait of ranking row n; [26] that of the cursor row */
    /* 0x170 */ s32 flags;          /* the fields from here to voiceLast are used by the tail of the object (menu_s.c) */
    /* 0x174 */ s32 cur[6];
    /* 0x18C */ s32 timer;
    /* 0x190 */ s32 level;
    /* 0x194 */ s32 voiceReq;
    /* 0x198 */ s32 voiceLast;
    /* 0x19C */ s32 voiceSkip;
    /* 0x1A0 */ s32 voiceLine;      /* subtitle line, -1 = none */
    /* 0x1A4 */ s32 blink;
    /* 0x1A8 */ s32 mouth;
    /* 0x1AC */ s32 idle;
    /* 0x1B0 */ s32 starTimer;
    /* 0x1B4 */ s32 starFrame;
    /* 0x1B8 */ s32 top;            /* first ranking row shown (0..5) */
    /* 0x1BC */ s32 cursor;         /* ranking row shown on the sixth plate (the one that scrolls in) */
    /* 0x1C0 */ s32 page;           /* text line of the description: the page of the "how to play" text (0..10) */
    /* 0x1C4 */ s32 iconFrame;      /* flips every frame */
} SimTop; /* 0x1C8 */

extern SimTop *gSimTop;       /* 0x3B7420 */
extern s32 gSimTopFaceTex[5]; /* 0x3B7428 */

void SimTop_ShowStar(char *parent, s32 on);
void SimTop_Init(s32 section);
void SimTop_Term(void);
void SimTop_Draw(void);

#endif
