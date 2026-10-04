# Menu support code in the main executable (0x25C2A8..0x2600B0)

Source (linked): src/battle/view_a.c .. view_a_e.c, include/battle/view_a.h, names in
config/symbols/view_a.txt (all marked guess). 80 functions, 77 match; `Num_DrawEx`,
`ChrGrid_Build` and `BgmList_ApplyUnlocks` (view_a_e.c) are INCLUDE_ASM with attempts. None of
this is battle code: it is called almost entirely from the menu overlay DBZP.BIN. Suggested
final names: menu/get_win.c, msg_win.c, icon_win.c, chr_view.c, menu_util.c, text_box.c.
view_a_e.c is one object that continues past 0x2600B0: the TextBox head (written as view_a_f.c)
was merged into it at link time, because as two files the powers-of-ten table landed at 0x2F3248
instead of 0x2F3250 (the object's read-only data is 16-byte aligned by TextBox_Init's jump table).

| File | Range | Module |
|---|---|---|
| view_a.c | 0x25C2A8..0x25CFC0 | `GetWin`: reward window ("title_get", money digits, item lines) |
| view_a_b.c | ..0x25D290 | `MsgWin`: message window sliding in from left or right |
| view_a_c.c | ..0x25D468 | `IconWin`: window with an icon |
| view_a_d.c | ..0x25DE68 | `ChrView`: character model viewer |
| view_a_e.c | ..0x2600B0 | `Progress_*`, `FlashAnim_*`, `Num_*`, `ChrGrid_*`, `StgGrid_*`, `BgmList_*`; from 0x25FE00 `TextBox`: head of the text box module |

Verified by matching C:
- **Character viewer**: reuses the battle object, stage, scene and ordering-table modules
  outside a battle (`ChrView_Init / Update / Term`, own frame, not `Battle_Loop`).
  `BtlObj_Create` type 3 is a viewer model, played with model animation 7. Character model
  files are `0x590 + chara * 10 + costume`, +4 for the damaged model; file 0x197 is the
  backdrop (kept at `gCommonRes + 0x24`); common file 4 section 1 holds 0x3C-byte character
  entries (+0x30 viewer camera target height, +0x34 distance). Frame order: passes 1, 3, 2,
  0, 4, 0 through `Gfx_MarkPass`. `OrbitCam_Update(1)` reads pad 0.
- **`gProgress`** is 0x7FC bytes; `Progress_ClearSession` clears +0x30 (0x4C), +0x7C (0x3C4),
  +0x440 (0x1F4), +0x634 (0x58), +0x68C (0x144), +0x7D0, +0x7D4 (0x28). Flag 0x100 freezes
  the menu clip animations.
- **Character grid**: 7 columns, 0x24-byte cells {id, formCount, form[7]}; ids 0..0xA0
  character, 0xA1 custom, 0xA2 locked, 0xA3 random, 0xA4 filler. **Stage grid**: 6 columns,
  0x24 locked, 0x25 filler; `gSaveData->stageBits` bit = unlocked.
- **Random draws**: `FlashAnim_Blink` (`Rand_Range(12)`) and `FlashAnim_Talk`
  (`Rand_Range(3)`) draw from the shared Mersenne Twister whenever a menu portrait animates,
  so menu time before a match changes that generator's state (netplay: reseed at match start).
- Original quirks: `StgGrid_MoveRight` wraps at 7 in a 6-column grid; the three window `Term`
  functions use the pointer before the NULL test; `FlashAnim_ShowNext` and `ShowNext2` are
  identical.

Inferred (from the non-matching attempts or names): `ChrGrid_Build` filters by
`gSaveData->charaBits` and by `gProgress->mode` and lists the 14 saved custom characters;
`bgmBits` gates a 25-entry list; what `GetWin` kinds 0, 1, 7, 8, 9 announce; `TextBox` field
meanings.

Linked layout: each file defines its global pointer `= NULL` (`gGetWin` 0x2FF0D0, `gMsgWin`
0x2FF0D8, `gIconWin` 0x2FF0F0, `gChrView` 0x2FF108, `gProgress` 0x2FF10C), which with the short
label strings reproduces `.sdata` 0x2FF0D0..0x2FF110; `.rodata` 0x2F30E0 (view_a.c), 0x2F3218
(view_a_b.c), 0x2F3250 (view_a_e.c); `.lit4` 0x2FE7A4 (view_a_d.c). `Num_ToDigits` and
`Res_RelocateOffsets` return nothing; the declarations in sys/loading.c and the window files
were corrected.
