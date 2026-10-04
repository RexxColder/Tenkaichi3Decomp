#ifndef MENU_MENU_ZA_H
#define MENU_MENU_ZA_H

/* menu_a.h declares Snd_PlaySe as returning nothing; it returns s32 (see include/menu/menu_h.h). */
#define Snd_PlaySe Snd_PlaySe_menuA
#include "menu/menu_a.h"
#include "sys/pad.h"
#undef Snd_PlaySe
extern s32 Snd_PlaySe(u32 mask, s32 id);

/*
 * Menu overlay DBZP.BIN, 0x3AC440..0x3B0E04 (placeholder stem "menu_za"): the last chunk of the overlay, the
 * rest of the Data Center (main-menu item 7, progress modes 53..56).
 *
 *   menu_za.c    0x3AC440..0x3AE648  DcPass     tail of the password entry screen (mode 54); the head of the
 *                                               object is in the previous chunk (stem menu_z, from 0x3AAF30)
 *   menu_za_b.c  0x3AE648..0x3AEAB0  PassWin    the window that shows a character's password (a whole object)
 *   menu_za_c.c  0x3AEAB0..0x3AF290  PassChk    password validity tables and the old-format converter (whole)
 *   menu_za_d.c  0x3AF290..0x3B0C08  ReplayMenu the replay list (mode 56): load a replay / save the last battle
 *   menu_za_e.c  0x3B0C08..0x3B0E04  DcSave     the "save the game" card flow the Data Center runs on leaving
 *
 * Every structure here is this chunk's own view: the neighbours' headers were still changing.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void Flash_ClipSetScale(MFlash *flash, MFlashRef *ref, f32 x, f32 y);
extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetCallbackB(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern void TextBox_Init(MTextBox *box, void *text, u32 preset);
extern void TextBox_SetUnk80(MTextBox *box, s32 value);
extern void TextBox_SetRect(MTextBox *box, s32 x0, s32 x1, s32 y0, s32 y1);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern s32 Dialog_Input(s32 allowCancel);
extern s32 Dialog_IsClosed(void);
extern void Dialog_Draw(s32 visible);
extern void Dialog_Start(s32 kind);
extern void Dialog_SetChoices(s32 on);
extern void Dialog_SetCursor(s32 choice);
extern void Dialog_SetMsg(s32 idx);
extern void Dialog_SetLayout(s32 layout);
extern void Voice_StopWithLip(void);
extern void McFlow_SetModeCb(s32 mode, s32 idx, void *cb, s32 arg);
extern void McFlow_SetSlot(s32 slot);
extern s32 McFlow_PollCard(void);
extern void Progress_ClearTeams(void);
extern double pow(double, double);

/* Voice_GetStat result when nothing is playing. */
#define MVOICE_IDLE 5

/* Voice set of the mode's guide (Bulma). */
#define DC_VOICE_BASE 0x8398

/* The password codec of the main executable (include/sys/misc_a.h; local copy of the two records). */
typedef struct ZaOldPass {
    /* 0x00 */ s32 charId;      /* character id of the PREVIOUS game */
    /* 0x04 */ s32 item[7];     /* item ids of the previous game, 1-based */
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
} ZaOldPass; /* 0x30 */

typedef struct ZaChrPass {
    /* 0x00 */ s32 charId;      /* 0..160 */
    /* 0x04 */ s32 item[8];     /* 1-based item ids, 0 = empty */
    /* 0x24 */ s32 extraSlots;  /* item slots gained on top of the character's own */
    /* 0x28 */ s32 unk28;
} ZaChrPass; /* 0x2C */

extern s32 ChrPass_Encode(ZaChrPass *in);
extern char *ChrPass_GetText(void);

/* ---- DcPass: the password entry screen (mode 54). Head of the object: previous chunk. ---- */

#define DCPASS_FLASH_NUM 2
#define DCPASS_LIST_ROWS 3      /* rows of the "which slot" list that the cursor can be on */
#define DCPASS_REC_NUM 14       /* saved custom characters */
#define DCPASS_TEXT_LAST 0x21   /* index of the last of the 34 password characters */

/* The on-screen keyboard's cursor. */
typedef struct DcPassKbd {
    /* 0x00 */ s32 page;        /* which of the keyboard's character sets is shown */
    /* 0x04 */ s32 row;
    /* 0x08 */ s32 col;
} DcPassKbd;

/* The slot list shown after a password was accepted: fourteen slots, four plates on screen. */
typedef struct DcPassList {
    /* 0x00 */ s32 top;         /* first slot shown, 0..11 */
    /* 0x04 */ s32 cursor;      /* row the cursor is on, 0..2 */
    /* 0x08 */ s32 extra;       /* slot shown on the fourth plate while the list scrolls */
    /* 0x0C */ s32 chara[DCPASS_REC_NUM]; /* character saved in each slot, -1 = free */
} DcPassList; /* 0x44 */

/* The password being typed. */
typedef struct DcPassText {
    /* 0x00 */ char text[0x48];
    /* 0x48 */ s32 pos;         /* character the cursor is on, 0..33 */
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ s32 unk50;
    /* 0x54 */ s32 unk54;
    /* 0x58 */ s32 unk58;
} DcPassText; /* 0x5C */

/* The part of the work that the drawing helpers take. */
typedef struct DcPassView {
    /* 0x000 */ MFlash flash[DCPASS_FLASH_NUM]; /* 0 the keyboard (section 17), 1 the new character (section 18) */
    /* 0x058 */ MTexRes *bg;        /* section 10 */
    /* 0x05C */ u8 *tex0[30];
    /* 0x0D4 */ u8 *tex1[37];
    /* 0x168 */ s32 blink;
    /* 0x16C */ s32 talk;
    /* 0x170 */ void *msgText;      /* section 8 */
    /* 0x174 */ void *dialogMsg;    /* section 21 */
    /* 0x178 */ void *subtitles;    /* section 9 */
    /* 0x17C */ u8 unk17C[0x3C];    /* passed to MsgWin_Init, which does not take it */
    /* 0x1B8 */ MTextBox nameBox[DCPASS_LIST_ROWS];
    /* 0x35C */ MTextBox formBox[DCPASS_LIST_ROWS];
    /* 0x500 */ MTextBox nameBoxB;  /* the fourth plate */
    /* 0x58C */ MTextBox formBoxB;
    /* 0x618 */ MTextBox nameBoxL;  /* "mc_name_text_l" */
    /* 0x6A4 */ MTextBox formBoxL;
    /* 0x730 */ MTextBox nameBoxR;  /* "mc_name_text_r" */
    /* 0x7BC */ MTextBox formBoxR;
    /* 0x848 */ void *nameText;     /* section 13: character names */
    /* 0x84C */ void *formText;     /* section 14: form names */
    /* 0x850 */ f32 scroll;         /* backdrop pattern offset */
} DcPassView; /* 0x854 */

/* What the password decodes to (first word: the character). The head of the object owns the layout. */
typedef struct DcPassNew {
    /* 0x00 */ s32 chara;
    /* 0x04 */ u8 unk4[0x34];
} DcPassNew; /* 0x38 */

typedef struct DcPass {
    /* 0x000 */ void *pack;         /* this screen's section of archive 8 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 22 sections */
    /* 0x008 */ void *faceFile;     /* 0x16800 bytes: file 0x2F9 + character, compressed */
    /* 0x00C */ MTexRes *faceRes;   /* 0x20800 bytes: the same unpacked */
    /* 0x010 */ s32 section;
    /* 0x014 */ DcPassView view;
    /* 0x868 */ DcPassText text;
    /* 0x8C4 */ s32 result;
    /* 0x8C8 */ s32 voiceLine;      /* subtitle line shown by the message window, -1 = none */
    /* 0x8CC */ s32 movie;          /* which of the two movies is shown */
    /* 0x8D0 */ u32 state;          /* DCPASS_ST_ */
    /* 0x8D4 */ DcPassKbd kbd;
    /* 0x8E0 */ DcPassList list;
    /* 0x924 */ DcPassNew rec;      /* the character the password describes */
    /* 0x95C */ s32 timer;
    /* 0x960 */ s32 unk960;
    /* 0x964 */ s32 flags;          /* DCPASS_ */
    /* 0x968 */ void *itemTbl;      /* section 12 */
    /* 0x96C */ void *order;        /* section 20 + 0x10: the character select order */
    /* 0x970 */ s32 orderNum;       /* first word of section 20 */
    /* 0x974 */ u32 nextState;      /* state the dialog leads to when it has closed */
    /* 0x978 */ f32 faceAlpha;
} DcPass; /* 0x97C */

#define DCPASS_ST_TYPE 0        /* typing the password */
#define DCPASS_ST_FAILED 1      /* the guide says the password is wrong */
#define DCPASS_ST_SHOWN 2       /* the new character is shown */
#define DCPASS_ST_LIST 3        /* choosing the slot to store it in */
#define DCPASS_ST_ASK_QUIT 4    /* "give the character up?" */
#define DCPASS_ST_ASK_OVER 5    /* "overwrite this slot?" */
#define DCPASS_ST_STORED 6      /* the guide's line after storing */

#define DCPASS_LEAVE 1
#define DCPASS_LEAVING 2
#define DCPASS_STARTED 4

#define DCPASS_FACE_FILE 0x2F9
#define DCPASS_FACE_SIZE 0x16800

/* What the keyboard's cursor is on (DcPass_GetKey). */
#define DCPASS_KEY_QUIT 0
#define DCPASS_KEY_PAGE 1
#define DCPASS_KEY_OK 2
#define DCPASS_KEY_BACK 3
#define DCPASS_KEY_LEFT 5
#define DCPASS_KEY_RIGHT 6

/* Previous chunk (head of the DcPass object). */
extern u32 func_003AAF50(DcPassKbd *kbd);
extern void func_003AAFA0(DcPassKbd *kbd);
extern void func_003AB000(s32 key, DcPassKbd *kbd);
extern s32 func_003AB038(DcPassKbd *kbd, s32 col);
extern s32 func_003AB088(DcPassKbd *kbd, s32 row);
extern void func_003AB640(DcPassView *view, DcPassKbd *kbd, s32 on);
extern void func_003AB700(DcPassView *view, DcPassKbd *kbd);
extern void func_003AB780(DcPass *pass);
extern void func_003AB7C8(DcPassText *text);
extern void func_003AB800(DcPassKbd *kbd, DcPassText *text);
extern s32 func_003AB848(DcPassText *text);
extern s32 func_003AB8C8(DcPassText *text);
extern void func_003ABCA0(DcPassNew *rec, DcPassList *list);
extern s32 func_003ABD28(DcPass *pass);
extern void func_003ABF00(DcPass *pass);
extern void func_003AC050(DcPass *pass);
extern void func_003AC098(DcPassView *view, DcPassNew *rec);

/* ---- PassWin (menu_za_b.c) ---- */

typedef struct PassWin {
    /* 0x00 */ MFlash flash[1];
    /* 0x2C */ u8 *tex[4];
} PassWin; /* 0x3C */

extern PassWin *gPassWin; /* 0x3BC9B8 */

/* ---- PassChk (menu_za_c.c) ---- */

/* Character entry of common file 4 (ChrTblEntry in battle/view_b.h; local view). */
typedef struct ZaChrEntry {
    /* 0x00 */ u8 unk0[8];
    /* 0x08 */ u16 flags;       /* bit 0 / bit 1: two character classes that some items refuse */
    /* 0x0A */ u8 unkA[4];
    /* 0x0E */ u16 slots;       /* item slots the character has */
    /* 0x10 */ u8 unk10[0x2C];
} ZaChrEntry; /* 0x3C */

/* Item entry of common file 4 (ItemTblEntry in battle/view_b.h; local view). */
typedef struct ZaItemEntry {
    /* 0x00 */ u8 type;         /* 2: only allowed in the eighth place; 3: fills the character's slots up to 7 */
    /* 0x01 */ u8 kind;         /* two items of the same type and kind exclude each other */
    /* 0x02 */ u8 unk2;
    /* 0x03 */ u8 slots;        /* item slots it takes */
    /* 0x04 */ u8 unk4[0x10];
    /* 0x14 */ u32 flags;       /* 1, 4: never in a password; 8 / 0x10: refused by a character class */
    /* 0x18 */ u8 unk18[0x10];
} ZaItemEntry; /* 0x28 */

/* The main executable's table of common files (CommonRes, sys/common.h); data[2] is common file 4. */
typedef struct ZaCommonRes {
    /* 0x00 */ void *unk0;
    /* 0x04 */ u32 *data[3];
} ZaCommonRes;

extern ZaCommonRes *gCommonRes;

/* One item of the previous game: what it gives when converted. */
typedef struct PassOldItem {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 level;        /* summed into the level class */
    /* 0x02 */ u8 defense;      /* summed and compared with attack */
    /* 0x03 */ u8 attack;
} PassOldItem;

/* One character of this game, as the password check sees it. */
typedef struct PassChara {
    /* 0x00 */ u16 mask;        /* bit n - 1: items of group n are allowed */
    /* 0x02 */ u16 valid;       /* non-zero: the character may come from a password */
} PassChara;

/* One item of this game, as the password check sees it. */
typedef struct PassItem {
    /* 0x00 */ u8 flags;        /* bit 0 refused, bit 5 allowed in a password */
    /* 0x01 */ u8 group;        /* 0 = any character */
} PassItem;

/* The five tables of the password pack. */
typedef struct PassChk {
    /* 0x00 */ PassOldItem *oldItem;  /* section 2: per item of the previous game */
    /* 0x04 */ s32 *oldChara;         /* section 1: character id of the previous game -> this game's */
    /* 0x08 */ s32 (*preset)[3][7];   /* section 3: [6 level classes][3 types][7] item ids, 999 = none */
    /* 0x0C */ PassChara *chara;      /* section 5 */
    /* 0x10 */ PassItem *item;        /* section 4 */
} PassChk; /* 0x14 */

extern PassChk *gPassChk; /* 0x2FF290: in the main executable's .sbss */

/* ---- ReplayMenu (menu_za_d.c) ---- */

#define REPLAY_SLOT_NUM 7
#define REPLAY_TEAM_NUM 2
#define REPLAY_MEMBER_NUM 5

/* One replay file as the card scan left it in gProgress (0x2C bytes). */
typedef struct ReplaySlot {
    /* 0x00 */ s32 flags;       /* bit 0: the slot holds a replay */
    /* 0x04 */ s32 chara[REPLAY_TEAM_NUM * REPLAY_MEMBER_NUM]; /* 0xA4 = empty */
} ReplaySlot; /* 0x2C */

/* gProgress as this chunk uses it. */
typedef struct ZaProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 baseFile;
    /* 0x008 */ void *unk8[3];
    /* 0x014 */ s32 flags;         /* ZAPROG_ */
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x624 - 0x1C];
    /* 0x624 */ s32 battleType;    /* 0 = single characters (mode 39), else teams (mode 40) */
    /* 0x628 */ u8 unk628[0x68C - 0x628];
    /* 0x68C */ s32 replayFlags;   /* bit 0: the menu was entered from a battle, to save its replay */
    /* 0x690 */ s32 dcVisits;
    /* 0x694 */ s32 dcCursor;
    /* 0x698 */ s32 replayCursor;  /* slot the replay list was left on */
    /* 0x69C */ ReplaySlot replay[REPLAY_SLOT_NUM];
} ZaProgress;

#define ZAPROG ((ZaProgress *)gProgress)
#define ZAPROG_DIRTY 1          /* the save changed in the Data Center */
#define ZAPROG_FREEZE 0x100
#define ZAPROG_REPLAY_SAVE 1    /* replayFlags */

typedef struct ReplayMenu {
    /* 0x000 */ void *pack;         /* this screen's section of archive 8 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 10 sections */
    /* 0x008 */ u32 *chips;         /* section 8: a pack of 165 character chip texture lists */
    /* 0x00C */ MFlash flash[1];
    /* 0x038 */ MTexRes *bg;        /* section 6 */
    /* 0x03C */ u8 *tex[20];
    /* 0x08C */ u8 *chip[REPLAY_TEAM_NUM][REPLAY_MEMBER_NUM]; /* textures 20..29: the chips of the slot shown */
    /* 0x0B4 */ u8 *texB[9];
    /* 0x0D8 */ void *font;         /* section 9 */
    /* 0x0DC */ s32 unkDC;
    /* 0x0E0 */ s32 result;         /* REPLAY_RESULT_ */
    /* 0x0E4 */ s32 unkE4;
    /* 0x0E8 */ s32 busy;           /* REPLAY_BUSY_: which card flow is running */
    /* 0x0EC */ s32 unkEC;
    /* 0x0F0 */ s32 cursor;         /* slot shown, 0..6 */
    /* 0x0F4 */ s32 timer;          /* frames from leaving to the end */
    /* 0x0F8 */ s32 flags;          /* REPLAY_ */
    /* 0x0FC */ s32 unkFC;
    /* 0x100 */ s32 mcState;        /* McFlow_Update result: non-zero while a card flow runs */
} ReplayMenu; /* 0x104 */

#define REPLAY_LEAVE 1
#define REPLAY_LEAVING 2
#define REPLAY_STARTED 4
#define REPLAY_SCANNED 8        /* the card was read once */
#define REPLAY_PLAYABLE 0x10    /* the slot shown holds a replay */

#define REPLAY_BUSY_NONE 0
#define REPLAY_BUSY_SCAN 1
#define REPLAY_BUSY_FILE 2      /* loading or saving a replay */
#define REPLAY_BUSY_ASK 3       /* "leave without saving the replay?" */
#define REPLAY_BUSY_REMOVED 4   /* the card went away */

#define REPLAY_RESULT_BACK 0    /* back to the Data Center menu */
#define REPLAY_RESULT_PLAY 1    /* a replay was loaded: leave the overlay and run the battle */
#define REPLAY_RESULT_SINGLE 39 /* after saving: the progress mode to go back to (character select) */
#define REPLAY_RESULT_TEAM 40   /* (team select) */

extern ReplayMenu *gReplayMenu; /* 0x3BC9F8 */

/* ---- DcSave (menu_za_e.c) ---- */

typedef struct DcSave {
    /* 0x00 */ s32 flags;       /* DCSAVE_ */
    /* 0x04 */ s32 state;       /* McFlow_Update result */
} DcSave; /* 8 */

#define DCSAVE_DONE 1           /* the card flow ended (saved or not) */
#define DCSAVE_STARTED 4

extern DcSave *gDcSave; /* 0x3BC9FC */

s32 DcPass_Run(s32 section);
void PassWin_Init(u32 *pack);
void PassWin_Term(void);
void PassWin_Draw(void);
void PassWin_Open(u16 *items, s32 chara);
void PassWin_OpenEx(u16 *items, s32 chara, s32 extraSlots);
void PassWin_Close(void);
void PassChk_Init(u32 *pack);
void PassChk_Term(void);
ZaChrPass PassChk_ConvertOld(ZaOldPass *old);
s32 PassChk_IsOldValid(ZaOldPass *old);
s32 PassChk_IsValid(ZaChrPass *pass);
s32 ReplayMenu_Run(s32 section);
void DcSave_Init(void *pack);
void DcSave_Term(void);
void DcSave_Start(void);
void DcSave_Update(void);
s32 DcSave_GetState(void);
s32 DcSave_IsDone(void);
s32 DcSave_IsStarted(void);

#endif
