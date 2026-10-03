#ifndef SYS_GAME_PAD_H
#define SYS_GAME_PAD_H

#include "types.h"
#include "sys/pad.h"

/* Game-level input layer, src/sys/game_pad.c = 0x2574F0..0x257870 (five functions).
   Before it (..0x2574F0) is matrix/quaternion maths, after it (0x257870..) a list-pool module.

   == The game button word (Pad.gameHeld / gamePressed / gameRepeat, +0x188 / +0x18C / +0x190) ==
   Built once per frame per port by Pad_UpdateGameButtons, called from Pad_Update (pad.c) right after the
   raw read. The mapping is FIXED: no table, no key config, the same for both ports. Each bit is one
   Pad_IsHeld(pad, rawMask) test on Pad.held (PAD_* bits, see pad.h):

     bit  PADG_*              raw input (PAD_*)
      0   PADG_LEFT           PAD_LEFT          d-pad
      1   PADG_RIGHT          PAD_RIGHT
      2   PADG_DOWN           PAD_DOWN
      3   PADG_UP             PAD_UP
      4   PADG_RSTICK_LEFT    PAD_RSTICK_LEFT   right stick past 0.5 on its dominant axis
      5   PADG_RSTICK_RIGHT   PAD_RSTICK_RIGHT
      6   PADG_RSTICK_DOWN    PAD_RSTICK_DOWN
      7   PADG_RSTICK_UP      PAD_RSTICK_UP
      8   PADG_CIRCLE         PAD_CIRCLE
      9   PADG_CROSS          PAD_CROSS
     10   PADG_TRIANGLE       PAD_TRIANGLE
     11   PADG_SQUARE         PAD_SQUARE
     12   PADG_START          PAD_START
     13   PADG_SELECT         PAD_SELECT
     14   PADG_L1             PAD_L1
     15   PADG_L2             PAD_L2
     16   PADG_R1             PAD_R1
     17   PADG_R2             PAD_R2
     18   PADG_L3             PAD_L3
     19   PADG_R3             PAD_R3
     20   (none)              mask 0: never set
     21   PADG_LEFT2          PAD_LEFT          the d-pad a second time
     22   PADG_RIGHT2         PAD_RIGHT
     23   PADG_DOWN2          PAD_DOWN
     24   PADG_UP2            PAD_UP
     25..31                   never set
   The left stick's digital bits (PAD_LSTICK_*) are not mapped. The analog sticks are copied unchanged to
   gameLeft / gameRight (+0x1A4 / +0x1AC).
     gamePressed = gameHeld & ~(gameHeld of the previous frame)
     gameRepeat  = Pad_CalcRepeat(gameHeld, gamePressed, &gameRepeatTimer, &gameRepeatLast, repeatDelay, repeatInterval)
   Pad_Reset (pad.c) zeroes gameHeld/gamePressed/gameRepeat and the timer when the port has no usable pad;
   it does not touch gameLeft/gameRight, which keep their last value until the next successful read.

   == Repeat (Pad_CalcRepeat) ==
   Works on the whole word, not per bit. While held is non-zero and identical to the previous frame's, the
   timer counts down; when it goes below zero it is reloaded with `interval` and the whole held word is
   returned. Any change of held (including to 0, and held == 0 itself) reloads the timer with `delay` and
   returns nothing. A non-zero pressed always overrides the result. So with delay D and interval I a button
   reports on frame 0, then on frame D+1, then every I+1 frames. Pad_Init sets D = 20, I = 1; the same
   delay/interval pair also drives the raw Pad.repeat word. Pad_SetRepeat changes it for both ports
   (callers: func_002BD230 in the main executable, with (20, 1) and later (40, 3); func_00336A90 in the
   menu overlay, twice).

   == Who reads it ==
   There are no reader functions for the game button word: every user reads gPad[n] fields directly
   (lui/lw on gPad + 0x18C etc.). Menus, the overlay and the debug screens use gamePressed and gameRepeat
   (occasionally gameHeld, gameLeft/gameRight); see the report for the list. Pad_GetStatus has no callers,
   Pad_GetLastStatus one (func_002145D8, the pause menu).

   == Battle does NOT use the game button word ==
   Fights read the RAW word and go through their own per-player key config (battle code, not this file):
     func_001D4A00(chr)  samples gPad[chr->padIndex] (via func_001DC2A0): status, lastStatus, raw Pad.held
                         (+0x148) and the left stick (+0x130/+0x134, quantised to a byte, 0x7F = centre)
                         into chr+0x570, then func_001D4370 turns that into a 16-byte record at chr+0x938:
                         'O','P','R','T', player, 0, stickX, stickY, u32 buttons, u32 commands.
     func_001D3BB0(rawHeld, table)  the remap: battle bit i is set when rawHeld & table[i] (bit 15 needs
                         all bits of its mask). table = 31 raw PAD_* masks at chr+0x578.
     func_001D4870(chr)  builds that table from the key config: func_001D47C8(chr, action) looks `action`
                         up in gOption->keyConfig[player][8] (s32, gOption + 0x160C + player * 0x20; a
                         backup copy sits at + 0x164C) and returns the matching entry of D_002EF050 =
                         { CIRCLE, CROSS, SQUARE, TRIANGLE, L1, L2, R1, R2 }. The option menu's default
                         (overlay 0x3A296C) is keyConfig = { 2, 1, 0, 3, 4, 5, 6, 7 }.
     func_001D4A70(chr)  pushes the record through an 8-entry ring (chr+0x8C0..) and derives the per-frame
                         held/prev/pressed/released words at chr+0x73C/0x740/0x744/0x748 (buttons) and
                         chr+0x74C.. (commands).
     readers             func_001D4DD0 (held), func_001D4D98 (pressed), func_001D4DE0 (released),
                         func_001D4DA8, func_001D4E40/4E50/4E60 (command word), func_001D4DF0.. (frame counters).
   The battle button/command word, not gameHeld, is what a fight consumes each frame. */

/* Translates gPad[pad].held into the game button word and derives gamePressed / gameRepeat; copies the sticks. */
void Pad_UpdateGameButtons(s32 pad);
/* Auto-repeat step for one button word; returns the bits to report this frame. */
u32 Pad_CalcRepeat(u32 held, u32 pressed, s32 *timer, u32 *last, s32 delay, s32 interval);
/* Sets repeatDelay / repeatInterval (frames) of both ports. */
void Pad_SetRepeat(s32 delay, s32 interval);
/* gPad[pad].status: 0 = usable pad, PAD_STATUS_NONE = nothing usable. */
s32 Pad_GetStatus(s32 pad);
/* gPad[pad].lastStatus: the last status below 4 seen while the pad was being read. */
s32 Pad_GetLastStatus(s32 pad);

#endif
