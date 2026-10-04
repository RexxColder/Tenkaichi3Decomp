#ifndef MENU_MENU_Y_H
#define MENU_MENU_Y_H

#include "types.h"
#include "menu/menu_a.h"
#include "sys/save.h"
#include "sys/pad.h"

/*
 * Menu overlay DBZP.BIN, 0x3A3848..0x3A7D98 (placeholder stem "menu_y"). Two pieces, cut at the object boundary:
 *
 *   menu_y.c    0x3A3848..0x3A65E8  Option   tail of the option screen object (mode 62, handler 0x39FAA8; the
 *                                            object starts in the previous chunk, menu_x; work pointer 0x3BC364)
 *   menu_y_b.c  0x3A65E8..0x3A7D98  DcList   head of the Data Center's custom character list (modes 53..56,
 *                                            handler 0x3A9850; the object continues in the next chunk, menu_z)
 *
 * All names are guesses from what the code does. The structures are this chunk's own views.
 */

/* ---- Main executable, beyond what menu_a.h declares ---- */

extern void Sprite_SetScissor(s32 x0, s32 x1, s32 y0, s32 y1);
extern void Flash_ClipSetCallbackA(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetCallbackB(MFlash *flash, MFlashRef *ref, void *fn, void *arg);
extern void Flash_ClipSetOffset(MFlash *flash, MFlashRef *ref, s32 x, s32 y);
extern void Flash_ClipSetScale(MFlash *flash, MFlashRef *ref, f32 x, f32 y);
extern void TextBox_AttachLine(MFlash *flash, MFlashRef *ref, s32 x, s32 y, s32 line, MTextBox *box);
extern void IconWin_SetIcon(s32 icon);
extern void Dialog_Draw(s32 visible);
extern void Dialog_Start(s32 kind);
extern s32 Dialog_Input(s32 allowCancel);
extern void Dialog_SetChoices(s32 on);
extern void Dialog_SetMsgTable(void *table);
extern void Dialog_SetMsg(s32 idx);
extern void Dialog_SetCursor(s32 cursor);
extern s32 Dialog_IsClosed(void);
extern void SndOpt_Apply(void);
extern void Dialog_Init(void *file, void *msgTbl, s32 size);
extern void Dialog_Term(void);

/* Snd_PlaySe returns a value in the original (menu_a.h declares it void). */
#define Snd_PlaySe ((s32 (*)(u32, s32))Snd_PlaySe)

/* Voice_GetStat result when nothing is playing. */
#define Y_VOICE_IDLE 5

/* ---- Option (menu_y.c): the option screen of mode 62 ---- */

/*
 * The work area, as include/menu/menu_x.h declares it (the object's head is in that chunk; this is a copy so
 * that the two chunks do not depend on each other's headers; it is skipped when menu_x.h was included first).
 */
#ifndef MENU_MENU_X_H

#define OPTION_FLASH_NUM 1
#define OPTION_PAD_NUM 2
#define OPTION_KEY_NUM 8

typedef struct Option {
    /* 0x000 */ u32 *pack;          /* this screen's section of archive 10 (compressed) */
    /* 0x004 */ u32 *res;           /* the same unpacked: a pack of 39 sections */
    /* 0x008 */ MFlash flash[OPTION_FLASH_NUM];
    /* 0x034 */ void *bg;           /* section 30: background picture */
    /* 0x038 */ u8 *tex[51];
    /* 0x104 */ s32 cursor;         /* OPT_ITEM_: the row the cursor is on */
    /* 0x108 */ s32 state;          /* OPT_ST_ */
    /* 0x10C */ s32 page;           /* 0 top, 1 screen, 2 sound, 3 controller */
    /* 0x110 */ s32 bgmVolume;      /* the save's volumes when the volume picker was opened / last confirmed */
    /* 0x114 */ s32 seVolume;
    /* 0x118 */ s32 bgmCursor;      /* sound test: index into bgmIds */
    /* 0x11C */ s32 bgmTop;         /* first visible row */
    /* 0x120 */ s32 bgmBottom;      /* one past the last visible row (top + 6) */
    /* 0x124 */ s32 bgmExtra;       /* row that scrolls out */
    /* 0x128 */ s32 value;          /* column of a two-way picker, or volume row (0 music, 1 effects) */
    /* 0x12C */ s32 voiceLine;      /* guide line being said, also the subtitle shown */
    /* 0x130 */ s32 picker;         /* which captions the two-way picker shows; 7 and up = the volume window */
    /* 0x134 */ s32 unk134;
    /* 0x138 */ s32 pad;            /* controller page: the player being edited (0 / 1) */
    /* 0x13C */ s32 fromPage;       /* how the top page was reached again (which rows the page lists) */
    /* 0x140 */ s32 ctrlKind;       /* 0 vibration, 1 key assignment */
    /* 0x144 */ s32 screenX;        /* the save's screen position when the adjust page was opened */
    /* 0x148 */ s32 screenY;
    /* 0x14C */ s32 type;           /* controller type shown (0..2) */
    /* 0x150 */ s32 typePrev;       /* the type scrolling out */
    /* 0x154 */ s32 blink;          /* guide's eyes */
    /* 0x158 */ s32 talk;           /* guide's mouth */
    /* 0x15C */ s32 resetStep;      /* step of Option_UpdateReset */
    /* 0x160 */ s32 keyRow;         /* key page: index into the save's key table of the row under the cursor */
    /* 0x164 */ s32 markX;          /* where Option_SetKeyMark puts the cursor mark and the arrows */
    /* 0x168 */ s32 markY;
    /* 0x16C */ s32 arrowX;
    /* 0x170 */ s32 arrowY;
    /* 0x174 */ s32 keyOld;         /* value of that row when the button went down, -1 = none */
    /* 0x178 */ s32 valueOld;       /* the value when a picker was opened (to tell a change) */
    /* 0x17C */ s32 keyCursor;      /* key page: cursor 0..7 (two columns of four) */
    /* 0x180 */ s32 keyEdit;        /* the key page is open */
    /* 0x184 */ s32 keyHeld;        /* confirm is held on the key page this frame */
    /* 0x188 */ s32 mcBusy;         /* a memory card flow is running: Option_Run skips Option_Input */
    /* 0x18C */ s32 started;
    /* 0x190 */ s32 greeted;        /* the second greeting line was started */
    /* 0x194 */ s32 unk194;
    /* 0x198 */ s32 dirty;          /* a setting was changed: save on leaving */
    /* 0x19C */ s32 *bgmIds;        /* section 37 + 0x10: music ids of the sound test */
    /* 0x1A0 */ s32 bgmCount;
    /* 0x1A4 */ void *msgText;      /* section 33 */
    /* 0x1A8 */ void *dialogMsg;    /* section 38: message table of the "reset?" dialog */
    /* 0x1AC */ void *subtitles;    /* section 36 */
    /* 0x1B0 */ u8 unk1B0[0x3C];    /* handed to MsgWin_Init, which ignores it */
    /* 0x1EC */ s32 key[OPTION_PAD_NUM][OPTION_KEY_NUM]; /* the default key assignment */
} Option; /* 0x22C */

extern Option *gOption;

#endif

/* Option.state values this chunk tests (menu_x.h: OPT_ITEM_ / OPT_ST_). */
#define YOPT_SCREEN 2          /* top page row: screen page */
#define YOPT_SOUND 3
#define YOPT_CTRL 4
#define YOPT_TYPE 7            /* screen page: controller-layout type */
#define YOPT_ADJUST 9
#define YOPT_SCR_RESET 10
#define YOPT_ADJUST_RESET 11
#define YOPT_STEREO 12
#define YOPT_VOLUME 13
#define YOPT_BGM 14
#define YOPT_SND_RESET 16
#define YOPT_KEYS 17
#define YOPT_CTRL_RESET 19
#define YOPT_KEYS_PAD 21
#define YOPT_KEYS_EDIT 22

/* Option_PlateGoto: which clip */
#define OPT_CLIP_ROW 0         /* "mc_menu_plate_%d" of the top page's cursor */
#define OPT_CLIP_ROW_SCREEN 1  /* the same on the screen page (cursor - 6) */
#define OPT_CLIP_ROW_SOUND 2   /* the sound page (cursor - 11) */
#define OPT_CLIP_BOTTOM 3      /* "mc_bottom_plate_1" */
#define OPT_CLIP_PICK 4        /* "mc_select_plate_%d" of the two-way picker's column */
#define OPT_CLIP_VIB_OFF 5     /* the vibration picker's plate for the current setting */
#define OPT_CLIP_KEY_OFF 6
#define OPT_CLIP_BGM 7         /* "mc_bgm_plate_%d" of the sound test's cursor */
#define OPT_CLIP_VOL_BGM 8     /* "mc_vol_plate_bgm_%d" of the saved music volume */
#define OPT_CLIP_VOL_SE 9
#define OPT_CLIP_ROW_CTRL 10   /* the controller page (cursor - 16) */

/* Strings of the object's head (menu_x): one source file, so the compiler shared them. */
extern char D_003BC370[]; /* "fl_on_start" */
extern char D_003BC380[]; /* "fl_off_start" */

void Option_Draw(void);
void Option_PlateGoto(s32 unused, s32 clip, char *label);
s32 Option_SetKeyMark(s32 row);
void Option_UpdateReset(void);
void Option_SetBottomText(MFlash *flash, MFlashRef *ref, char *name, MFlashUv uv);
void Option_SetMenuText(MFlash *flash, MFlashRef *ref, char *name, MFlashUv uv);
void Option_DimPickers(void);
void Option_SetBgmScissor(void);
void Option_ResetScissor(void);
void Option_OnSaved(void);
void Option_Term(void);


/* ---- DcList (menu_y_b.c): the custom character list of the Data Center ---- */

/* The item table of common file 4 (ItemTblEntry of battle/view_b.h; only passed on here). */
typedef struct DcItemTbl DcItemTbl;
extern void ItemSet_GetBonus(u16 *ids, DcItemTbl *table, s32 *out);

/* Character entry of common file 4 (ChrTblEntry of battle/view_b.h; local view). */
typedef struct DcChrEntry {
    /* 0x00 */ s32 unk0[2];
    /* 0x08 */ u16 flags;          /* bit 0: clear = the character's attribute picture 1 */
    /* 0x0A */ u8 unkA[0x32];
} DcChrEntry; /* 0x3C */

typedef struct DcCommonRes {
    /* 0x00 */ void *unk0[3];
    /* 0x0C */ u32 *chrFile;       /* common file 4: word 1 = byte offset of the character table */
} DcCommonRes;

extern DcCommonRes *gCommonRes;

/* One saved custom character: SaveRec of sys/save.h with its first 0x14 bytes resolved. */
typedef struct DcRec {
    /* 0x00 */ u16 item[8];        /* equipped item ids, 1-based, 0 = empty; slot 7 is never used */
    /* 0x10 */ s32 unk10;
    /* 0x14 */ u16 level;
    /* 0x16 */ u16 unk16;
    /* 0x18 */ s32 chara;          /* character id, negative = the record is empty */
} DcRec; /* 0x1C */

#define DC_REC_NUM 14

/* gSaveData as this file sees it (checksum + body, as MSave of menu_c.h): the saved custom characters. */
typedef struct DcSave {
    /* 0x0000 */ s32 sum[2];
    struct {
        /* 0x0008 */ u8 unk8[0x2D40 - 8];
        /* 0x2D40 */ DcRec rec[14];
    } body;
} DcSave;

#define DCSAVE (&((DcSave *)gSaveData)->body)
#define DC_ROWS 3                  /* plates the cursor can be on; a fourth shows the row scrolling in or out */
#define DC_TOP_MAX 11              /* DC_REC_NUM - DC_ROWS */
#define DC_ITEM_MAX 350

/* The list and its cursor. */
typedef struct DcChars {
    /* 0x000 */ s32 top;           /* record on the first plate, 0..11 */
    /* 0x004 */ s32 row;           /* plate the cursor is on, 0..2 */
    /* 0x008 */ s32 extra;         /* record shown on the fourth plate while the list scrolls */
    /* 0x00C */ DcRec rec[DC_REC_NUM]; /* copy of gSaveData->rec */
} DcChars; /* 0x194 */

/* What the details panel shows for the chosen record. */
typedef struct DcStatus {
    /* 0x00 */ s32 bonus[5];       /* ItemSet_GetBonus: [0] item slots used (0..7), [1..4] stat changes -3..3 */
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 attr;           /* picture of "mc_status_attribute" */
    /* 0x1C */ DcRec rec;
} DcStatus; /* 0x38 */

#define DC_FLASH_NUM 2

/* The part of the work that the drawing helpers get a pointer to: the movies and what hangs on them. */
typedef struct DcView {
    /* 0x000 */ MFlash flash[DC_FLASH_NUM]; /* [0] the list ("chara_list_top"), [1] the item list ("chara_list_z_item") */
    /* 0x058 */ u8 unk58[0xE0];
    /* 0x138 */ s32 blink;
    /* 0x13C */ s32 talk;
    /* 0x140 */ MTextBox name[4];  /* character name of each list plate */
    /* 0x370 */ MTextBox form[4];  /* second text line of each list plate */
    /* 0x5A0 */ MTextBox curName;  /* "mc_name_text_l" of the details panel */
    /* 0x62C */ MTextBox curForm;  /* "mc_form_text_l" */
    /* 0x6B8 */ u8 unk6B8[0xC84 - 0x6B8];
} DcView; /* 0xC84 */

typedef struct DcList {
    /* 0x000 */ u8 unk0[0x14];
    /* 0x014 */ DcView view;
    /* 0xC98 */ f32 cloudX;        /* scroll of "mc_compane_3" */
    /* 0xC9C */ s32 running;       /* cleared when the list is left */
    /* 0xCA0 */ s32 unkCA0;
    /* 0xCA4 */ s32 state;         /* DCLIST_ST_ */
    /* 0xCA8 */ s32 menuCursor;    /* details menu: 0 items, 1 password, 2 delete */
    /* 0xCAC */ s32 itemCursor;    /* item list row */
    /* 0xCB0 */ DcChars chars;
    /* 0xE44 */ DcStatus status;
    /* 0xE7C */ s32 unkE7C;        /* 30 at start */
    /* 0xE80 */ DcItemTbl *itemTbl;
    /* 0xE84 */ s32 flags;         /* DCLIST_ */
} DcList;

#define DCLIST_ST_LIST 0           /* the list of 14 records */
#define DCLIST_ST_MENU 1           /* details panel and its three-item menu */
#define DCLIST_ST_ITEMS 2          /* the record's item list */
#define DCLIST_ST_PASSWORD 4       /* the password window */
#define DCLIST_ST_DELETE 5         /* the "delete?" dialog */

#define DCLIST_OPENED 1            /* a record's details were opened */
#define DCLIST_LEAVING 0x10        /* cancel on the list */

/* next chunk (menu_z / menu_za) */
extern void func_003AEA28(DcRec *rec, s32 chara, s32 level);

void DcView_InitBlink(DcView *v);
void DcChars_Load(DcChars *c);
void DcStatus_Calc(DcStatus *s, DcItemTbl *table);
void DcList_Reset(DcList *d);
void DcList_UpdateGuide(DcList *d);
void DcList_ScrollCloud(DcList *d);
void DcList_SetListScissor(void);
void DcList_ResetScissor(void);
void DcView_SetRowCallbacks(DcView *v);
void DcView_HideArrow(DcView *v, s32 up);
void DcView_SetArrows(DcView *v, DcChars *c);
void DcView_SetScrollBar(DcView *v, DcChars *c);
void DcView_LightRow(DcView *v, DcChars *c, s32 on);
s32 DcChars_ClampTop(DcChars *c);
s32 DcChars_ClampRow(DcChars *c);
void DcView_SetRows(DcView *v, DcChars *c);
s32 DcChars_GetCursor(DcChars *c);
DcRec DcChars_GetRec(DcChars *c);
void DcList_PickRec(DcList *d);
void DcList_RowOk(DcList *d);
void DcList_DrawList(DcList *d);
void DcList_InputList(DcList *d);
void DcView_SetMenuText(DcView *v);
void DcView_SetStatus(DcView *v, DcStatus *s);
void DcView_LightMenu(DcView *v, s32 item, s32 on);
void DcList_WrapMenu(s32 *cursor);
void DcView_HideNamePlates(DcView *v);
void DcView_SetNameText(DcView *v, s32 line);
void DcView_MenuOk(DcView *v, s32 item);
s32 DcRec_IsSlotEmpty(u16 *items, s32 slot);
void DcView_LightItem(DcView *v, s32 row, s32 on);
void DcList_InputMenu(DcList *d);

#endif
