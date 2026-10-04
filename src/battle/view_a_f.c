#include "common.h"
#include "battle/view_a.h"

/*
 * TextBox, 0x25FE00..0x2600B0: the first functions of the text box module (a text file plus the style its
 * lines are drawn in), which continues after 0x2600B0. See battle/view_a.h.
 *
 * This is not an object boundary: the powers of ten of Num_ToDigits (0x2F3250) and the jump table of
 * TextBox_Init (0x2F3270) lie in one 16-byte aligned read-only block, so view_a_e.c and this file belong to
 * the same original object, which goes on past the end of this range.
 */

extern void *memset(void *dst, s32 c, u32 n);

/* text box module, after 0x2600B0 (not decompiled) */
extern void func_002600B0(TextBox *box, s32 a);
extern void func_00260118(TextBox *box, s32 a, s32 b, s32 c, s32 d, s32 e);

/* Clears a text box, binds it to a text file and applies one of seven style presets. */
void TextBox_Init(TextBox *box, void *text, u32 preset) {
    memset(box, 0, sizeof(TextBox));
    box->text = text;
    switch (preset) {
    case 1:
        TextBox_SetUnk50(box, 2);
        TextBox_SetUnkC(box, 0x100, 0);
        func_002600B0(box, 0xE1);
        break;
    case 2:
        TextBox_SetUnk50(box, 0);
        TextBox_SetUnkC(box, 0, 0);
        func_002600B0(box, 0xE1);
        break;
    case 3:
        TextBox_SetUnk50(box, 2);
        TextBox_SetUnkC(box, 0x100, 0);
        TextBox_SetColor(box, 0xFFFF0080);
        func_002600B0(box, 0xD4);
        break;
    case 4:
        TextBox_SetUnk50(box, 0);
        TextBox_SetUnkC(box, 0, 0);
        TextBox_SetColor(box, 0xFFFF0080);
        func_002600B0(box, 0xD4);
        break;
    case 5:
        TextBox_SetUnk50(box, 0);
        TextBox_SetUnk80(box, 1);
        TextBox_SetUnkC(box, 0, 0);
        func_00260118(box, 0x20, 0x14, 0xA, 0, 0);
        break;
    case 6:
        TextBox_SetUnk50(box, 0);
        func_002600B0(box, 0x160);
        break;
    case 0:
        break;
    }
}

void TextBox_SetUnk50(TextBox *box, s32 value) {
    box->unk50 = value;
}

void TextBox_SetUnk80(TextBox *box, s32 value) {
    box->unk80 = value;
}

void TextBox_SetUnkC(TextBox *box, s32 a, s32 b) {
    box->unkC = a;
    box->unk10 = b;
}

/* Gives the box four values (a rectangle, by the look of it) and marks them valid. */
void TextBox_SetRect(TextBox *box, s32 a, s32 b, s32 c, s32 d) {
    box->rect[3] = d;
    box->rect[0] = a;
    box->rect[1] = c;
    box->rect[2] = b;
    box->flags |= TEXTBOX_FLAG_RECT;
}

/* Sets the text colour from 0xRRGGBBAA. */
void TextBox_SetColor(TextBox *box, u32 rgba) {
    box->flags |= TEXTBOX_FLAG_COLOR;
    box->color[0] = rgba >> 24;
    box->color[1] = rgba >> 16;
    box->color[2] = rgba >> 8;
    box->color[3] = rgba;
}

/* Sets the second colour from 0xRRGGBBAA. */
void TextBox_SetColor2(TextBox *box, u32 rgba) {
    box->flags |= TEXTBOX_FLAG_COLOR2;
    box->color2[0] = rgba >> 24;
    box->color2[1] = rgba >> 16;
    box->color2[2] = rgba >> 8;
    box->color2[3] = rgba;
}
