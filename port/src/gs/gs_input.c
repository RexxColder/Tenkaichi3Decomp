/*
 * Controller input for the windowed PC build: fills the game's pad buffer (the libpad2 layout the game reads in
 * src/sys/pad.c) from the keyboard and from SDL gamepads.
 *
 * Player 1: the keyboard and the first gamepad together. Player 2: the second gamepad.
 * Keyboard:  arrows = d-pad        W A S D = left stick       Enter = START      Right Shift = SELECT
 *            K = cross   L = circle   J = square   I = triangle
 *            U = L1   O = R1   7 = L2   9 = R2
 * Gamepad: the usual layout (south = cross, east = circle, west = square, north = triangle, triggers = L2 / R2).
 * The pressure bytes of the buffer are left at zero.
 */
#include <SDL3/SDL.h>

int gPortOverlayOpen; /* set by the renderer (gs_gpu.c) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gs_internal.h"

/* bits of the two button bytes together (byte 0 low), as include/sys/pad.h has them; 1 = pressed here */
enum { B_SELECT = 0x0001, B_L3 = 0x0002, B_R3 = 0x0004, B_START = 0x0008, B_UP = 0x0010, B_RIGHT = 0x0020, B_DOWN = 0x0040,
       B_LEFT = 0x0080, B_L2 = 0x0100, B_R2 = 0x0200, B_L1 = 0x0400, B_R1 = 0x0800, B_TRIANGLE = 0x1000, B_CIRCLE = 0x2000,
       B_CROSS = 0x4000, B_SQUARE = 0x8000 };

static SDL_Gamepad *sPads[2];
static int sInit;

static void pads_open(void) {
    SDL_JoystickID *ids;
    int n = 0, i, k = 0;

    if (!sInit) {
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        sInit = 1;
    }
    for (i = 0; i < 2; i++) { /* drop a pad that was unplugged */
        if (sPads[i] != NULL && !SDL_GamepadConnected(sPads[i])) {
            SDL_CloseGamepad(sPads[i]);
            sPads[i] = NULL;
        }
    }
    if (sPads[0] != NULL && sPads[1] != NULL) {
        return;
    }
    ids = SDL_GetGamepads(&n);
    for (i = 0; ids != NULL && i < n && k < 2; i++) {
        if ((sPads[0] != NULL && SDL_GetGamepadID(sPads[0]) == ids[i]) || (sPads[1] != NULL && SDL_GetGamepadID(sPads[1]) == ids[i])) {
            continue;
        }
        for (k = 0; k < 2 && sPads[k] != NULL; k++) {
        }
        if (k < 2) {
            sPads[k] = SDL_OpenGamepad(ids[i]);
        }
    }
    SDL_free(ids);
}

/* BT3_PAD="frame:button,frame:button,...": scripted presses for player 1, each held for 4 frames from that frame
   (frame = the renderer's frame counter). Buttons: start select up down left right cross circle square triangle
   l1 r1 l2 r2. For tests without a person at the keyboard. */
static unsigned script_buttons(void) {
    static const struct { const char *name; unsigned bit; } names[] = {
        {"start", B_START}, {"select", B_SELECT}, {"up", B_UP}, {"down", B_DOWN}, {"left", B_LEFT}, {"right", B_RIGHT},
        {"cross", B_CROSS}, {"circle", B_CIRCLE}, {"square", B_SQUARE}, {"triangle", B_TRIANGLE}, {"l1", B_L1}, {"r1", B_R1},
        {"l2", B_L2}, {"r2", B_R2},
    };
    static struct { unsigned frame, bit; } ev[256];
    static int count = -1;
    unsigned btn = 0;
    int i;

    if (count < 0) {
        const char *p = getenv("BT3_PAD");
        count = 0;
        while (p != NULL && *p != 0 && count < 256) {
            char name[16];
            unsigned frame;
            int used = 0;
            size_t k;
            if (sscanf(p, "%u:%15[a-z0-9]%n", &frame, name, &used) != 2) {
                break;
            }
            for (k = 0; k < sizeof(names) / sizeof(names[0]); k++) {
                if (strcmp(name, names[k].name) == 0) {
                    ev[count].frame = frame;
                    ev[count++].bit = names[k].bit;
                }
            }
            p += used;
            if (*p == ',') { p++; }
        }
    }
    for (i = 0; i < count; i++) {
        if (gGsFrame >= ev[i].frame && gGsFrame < ev[i].frame + 4) {
            btn |= ev[i].bit;
        }
    }
    return btn;
}

static unsigned char axis(SDL_Gamepad *g, SDL_GamepadAxis a) {
    return (unsigned char)((SDL_GetGamepadAxis(g, a) + 32768) >> 8);
}

/* Fills `data` (18 bytes) for controller port `socket`. Returns 0 when there is no window (the caller keeps the
   idle pad). */
int Port_PadRead(int socket, unsigned char *data) {
    static const struct { SDL_GamepadButton b; unsigned bit; } map[] = {
        {SDL_GAMEPAD_BUTTON_BACK, B_SELECT}, {SDL_GAMEPAD_BUTTON_START, B_START}, {SDL_GAMEPAD_BUTTON_LEFT_STICK, B_L3},
        {SDL_GAMEPAD_BUTTON_RIGHT_STICK, B_R3}, {SDL_GAMEPAD_BUTTON_DPAD_UP, B_UP}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, B_RIGHT},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, B_DOWN}, {SDL_GAMEPAD_BUTTON_DPAD_LEFT, B_LEFT}, {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, B_L1},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, B_R1}, {SDL_GAMEPAD_BUTTON_NORTH, B_TRIANGLE}, {SDL_GAMEPAD_BUTTON_EAST, B_CIRCLE},
        {SDL_GAMEPAD_BUTTON_SOUTH, B_CROSS}, {SDL_GAMEPAD_BUTTON_WEST, B_SQUARE},
    };
    static const struct { SDL_Scancode k; unsigned bit; } keys[] = {
        {SDL_SCANCODE_RSHIFT, B_SELECT}, {SDL_SCANCODE_RETURN, B_START}, {SDL_SCANCODE_UP, B_UP}, {SDL_SCANCODE_RIGHT, B_RIGHT},
        {SDL_SCANCODE_DOWN, B_DOWN}, {SDL_SCANCODE_LEFT, B_LEFT}, {SDL_SCANCODE_7, B_L2}, {SDL_SCANCODE_9, B_R2}, {SDL_SCANCODE_U, B_L1},
        {SDL_SCANCODE_O, B_R1}, {SDL_SCANCODE_I, B_TRIANGLE}, {SDL_SCANCODE_L, B_CIRCLE}, {SDL_SCANCODE_K, B_CROSS},
        {SDL_SCANCODE_J, B_SQUARE},
    };
    unsigned btn = 0, i;
    unsigned char rx = 0x80, ry = 0x80, lx = 0x80, ly = 0x80;
    SDL_Gamepad *g;

    if (!GsGpu_Enabled() || socket < 0 || socket > 1) {
        return 0;
    }
    pads_open();
    g = sPads[socket];
    if (g != NULL) {
        for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
            if (SDL_GetGamepadButton(g, map[i].b)) {
                btn |= map[i].bit;
            }
        }
        if (SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 12000) { btn |= B_L2; }
        if (SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 12000) { btn |= B_R2; }
        lx = axis(g, SDL_GAMEPAD_AXIS_LEFTX); ly = axis(g, SDL_GAMEPAD_AXIS_LEFTY);
        rx = axis(g, SDL_GAMEPAD_AXIS_RIGHTX); ry = axis(g, SDL_GAMEPAD_AXIS_RIGHTY);
    }
    if (socket == 0 && !gPortOverlayOpen) { /* the settings overlay takes the keyboard while it is open */
        const bool *ks = SDL_GetKeyboardState(NULL);
        for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            if (ks[keys[i].k]) {
                btn |= keys[i].bit;
            }
        }
        if (ks[SDL_SCANCODE_A]) { lx = 0x00; }
        if (ks[SDL_SCANCODE_D]) { lx = 0xFF; }
        if (ks[SDL_SCANCODE_W]) { ly = 0x00; }
        if (ks[SDL_SCANCODE_S]) { ly = 0xFF; }
    }
    if (socket == 0) {
        btn |= script_buttons();
    }
    memset(data, 0, 18);
    data[0] = (unsigned char)(~btn & 0xFF);       /* active low */
    data[1] = (unsigned char)(~(btn >> 8) & 0xFF);
    data[2] = rx; data[3] = ry; data[4] = lx; data[5] = ly;
    return 18;
}
