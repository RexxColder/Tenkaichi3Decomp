// The settings window of the PC build: Dear ImGui over the finished picture. F1 opens and closes it.
// Everything it shows lives elsewhere (ui.h): the renderer's settings in gs_gpu.c, the bindings in gs_input.c.
// It runs on the render thread, which owns the window and its events.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "ui.h"

static bool sReady, sOpen;
static int sForceTab = -1;                     // BT3_UI_OPEN=<tab>: open at start on that tab (testing)
static int sCapKind, sCapPlayer, sCapAction;   // waiting for a key (1) or a controller button (2) to bind
static SDL_GPUDevice *sDevice;

/* The stage-name strip (gamedata/stages/names.rgba): the names of stages added from outside the disc, drawn in
   the menu's own style (the port's own texture, one image per name, stacked). Raw RGBA, 12-byte header. */
static SDL_GPUTexture *sNameTex;
static uint32_t sNameW, sNameH, sNameCount;

static uint8_t *read_name_strip(const char *path, size_t *bytes, uint32_t *w, uint32_t *h, uint32_t *count) {
    FILE *fp = fopen(path, "rb");
    uint32_t hdr[3];
    uint8_t *pix;

    if (fp == NULL) {
        return NULL;
    }
    if (fread(hdr, sizeof(hdr), 1, fp) != 1 || hdr[0] == 0 || hdr[1] == 0 || hdr[2] == 0) {
        fclose(fp);
        return NULL;
    }
    *w = hdr[0];
    *h = hdr[1];
    *count = hdr[2];
    *bytes = (size_t)hdr[0] * hdr[1] * hdr[2] * 4;
    pix = (uint8_t *)malloc(*bytes);
    if (pix == NULL || fread(pix, 1, *bytes, fp) != *bytes) {
        free(pix);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    return pix;
}

static void load_name_strip(void) {
    static const char *paths[] = {NULL, "gamedata/stages/names.rgba", "names.rgba"};
    uint8_t *pix = NULL;
    size_t bytes = 0;
    uint32_t i;

    paths[0] = getenv("BT3_STAGE_NAMES");
    for (i = 0; i < sizeof(paths) / sizeof(paths[0]) && pix == NULL; i++) {
        if (paths[i] != NULL) {
            pix = read_name_strip(paths[i], &bytes, &sNameW, &sNameH, &sNameCount);
        }
    }
    if (pix == NULL) {
        return;
    }
    {
        SDL_GPUTextureCreateInfo ci;
        SDL_GPUTransferBufferCreateInfo tbi;
        SDL_GPUTransferBuffer *tb;
        SDL_GPUCommandBuffer *cmd;
        SDL_GPUCopyPass *cp;
        SDL_GPUTextureTransferInfo src;
        SDL_GPUTextureRegion dst;
        void *map;

        SDL_zero(ci);
        ci.type = SDL_GPU_TEXTURETYPE_2D;
        ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        ci.width = sNameW;
        ci.height = sNameH * sNameCount;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        sNameTex = SDL_CreateGPUTexture(sDevice, &ci);
        if (sNameTex == NULL) {
            free(pix);
            return;
        }
        SDL_zero(tbi);
        tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbi.size = (Uint32)bytes;
        tb = SDL_CreateGPUTransferBuffer(sDevice, &tbi);
        map = SDL_MapGPUTransferBuffer(sDevice, tb, false);
        memcpy(map, pix, bytes);
        SDL_UnmapGPUTransferBuffer(sDevice, tb);
        free(pix);
        cmd = SDL_AcquireGPUCommandBuffer(sDevice);
        cp = SDL_BeginGPUCopyPass(cmd);
        SDL_zero(src);
        src.transfer_buffer = tb;
        src.pixels_per_row = sNameW;
        src.rows_per_layer = sNameH * sNameCount;
        SDL_zero(dst);
        dst.texture = sNameTex;
        dst.w = sNameW;
        dst.h = sNameH * sNameCount;
        dst.d = 1;
        SDL_UploadToGPUTexture(cp, &src, &dst, false);
        SDL_EndGPUCopyPass(cp);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(sDevice, tb);
        gUiNameReady = 1;
        fprintf(stderr, "bt3: stage-name overlay: %u names, %ux%u each\n", sNameCount, sNameW, sNameH);
    }
}

static void style() {
    ImGuiStyle &st = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    st.WindowRounding = 10.0f;
    st.FrameRounding = 6.0f;
    st.GrabRounding = 6.0f;
    st.TabRounding = 6.0f;
    st.PopupRounding = 6.0f;
    st.WindowPadding = ImVec2(18.0f, 16.0f);
    st.FramePadding = ImVec2(10.0f, 6.0f);
    st.ItemSpacing = ImVec2(10.0f, 9.0f);
    st.WindowBorderSize = 0.0f;
    st.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    ImVec4 *c = st.Colors;
    const ImVec4 accent(0.96f, 0.55f, 0.13f, 1.0f), accentDim(0.96f, 0.55f, 0.13f, 0.55f);
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.11f, 0.96f);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.12f, 0.17f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.17f, 0.23f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.24f, 0.32f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.29f, 0.38f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.17f, 0.20f, 0.27f, 1.0f);
    c[ImGuiCol_ButtonHovered] = accentDim;
    c[ImGuiCol_ButtonActive] = accent;
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Header] = ImVec4(0.96f, 0.55f, 0.13f, 0.30f);
    c[ImGuiCol_HeaderHovered] = accentDim;
    c[ImGuiCol_HeaderActive] = accent;
    c[ImGuiCol_Tab] = ImVec4(0.13f, 0.15f, 0.20f, 1.0f);
    c[ImGuiCol_TabHovered] = accentDim;
    c[ImGuiCol_TabSelected] = ImVec4(0.96f, 0.55f, 0.13f, 0.80f);
    c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.14f, 0.98f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
}

int Ui_Init(SDL_Window *window, SDL_GPUDevice *device) {
    static const char *fonts[] = { // a proportional system font if there is one; else the library's built-in font
        "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf",
    };
    ImGui_ImplSDLGPU3_InitInfo info;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL; // no window-layout file next to the game
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    style();
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        FILE *fp = fopen(fonts[i], "rb");
        if (fp != NULL) {
            fclose(fp);
            io.Fonts->AddFontFromFileTTF(fonts[i], 18.0f);
            break;
        }
    }
    if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
        return 0;
    }
    info.Device = device;
    info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if (!ImGui_ImplSDLGPU3_Init(&info)) {
        return 0;
    }
    sDevice = device;
    sReady = true;
    load_name_strip(); /* the stage-name overlay; gUiNameReady stays 0 if there is no strip */
    if (getenv("BT3_UI_OPEN") != NULL) {
        sForceTab = atoi(getenv("BT3_UI_OPEN"));
        sOpen = true;
        gPortOverlayOpen = 1;
    }
    return 1;
}

static void set_open(bool open) {
    sOpen = open;
    sCapKind = 0;
    gPortInputCapture = 0;
    gPortOverlayOpen = open;
}

void Ui_Toggle(void) {
    if (sReady) {
        set_open(!sOpen);
    }
}

static void bind(int value) {
    if (sCapKind == 1) {
        PortInput_Keys(sCapPlayer)[sCapAction] = value;
    } else {
        PortInput_PadButtons(sCapPlayer)[sCapAction] = value;
    }
    PortInput_Save();
}

int Ui_Event(const SDL_Event *ev) {
    if (!sReady || !sOpen) {
        return 0;
    }
    if (sCapKind != 0) { // the next key or button is the binding; Esc leaves it as it was, Delete clears it
        bool done = false;
        if (ev->type == SDL_EVENT_KEY_DOWN) {
            if (ev->key.scancode == SDL_SCANCODE_DELETE || ev->key.scancode == SDL_SCANCODE_BACKSPACE) {
                bind(sCapKind == 1 ? 0 : -1);
            } else if (ev->key.scancode != SDL_SCANCODE_ESCAPE && sCapKind == 1) {
                bind((int)ev->key.scancode);
            }
            done = ev->key.scancode == SDL_SCANCODE_ESCAPE || ev->key.scancode == SDL_SCANCODE_DELETE ||
                   ev->key.scancode == SDL_SCANCODE_BACKSPACE || sCapKind == 1;
        } else if (sCapKind == 2 && ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            bind((int)ev->gbutton.button);
            done = true;
        } else if (sCapKind == 2 && ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && ev->gaxis.value > 20000 &&
                   (ev->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || ev->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
            bind(ev->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? PORT_PAD_LT : PORT_PAD_RT);
            done = true;
        }
        if (done) {
            sCapKind = 0;
            gPortInputCapture = 0;
        }
        if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP) {
            return 1;
        }
    } else if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.scancode == SDL_SCANCODE_ESCAPE) {
        set_open(false);
        return 1;
    }
    ImGui_ImplSDL3_ProcessEvent(ev);
    return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
           ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL;
}

static const char *pad_source_name(int src) {
    static const char *names[] = {"A / Cross", "B / Circle", "X / Square", "Y / Triangle", "Back / Select", "Guide", "Start",
                                  "Left stick click", "Right stick click", "Left shoulder", "Right shoulder", "D-pad up",
                                  "D-pad down", "D-pad left", "D-pad right", "Misc 1", "Right paddle 1", "Left paddle 1",
                                  "Right paddle 2", "Left paddle 2", "Touchpad"};
    static char other[24];
    if (src == PORT_PAD_LT) { return "Left trigger"; }
    if (src == PORT_PAD_RT) { return "Right trigger"; }
    if (src < 0) { return "-"; }
    if (src < (int)(sizeof(names) / sizeof(names[0]))) { return names[src]; }
    snprintf(other, sizeof(other), "Button %d", src);
    return other;
}

static bool tab(const char *label, int index) {
    ImGuiTabItemFlags flags = sForceTab == index ? ImGuiTabItemFlags_SetSelected : 0;
    return ImGui::BeginTabItem(label, NULL, flags);
}

static void video_tab(PortVideo &v) {
    static const struct { const char *name; int milli; } aspects[] = {{"4:3 (original)", 1333}, {"16:10", 1600}, {"16:9", 1778}, {"21:9", 2389}, {"32:9", 3556}};
    char label[64];
    int best = 0, count = 0;

    snprintf(label, sizeof(label), "%dx  (%d x %d)", v.scale, 512 * v.scale, 448 * v.scale);
    if (ImGui::BeginCombo("Internal resolution", label)) {
        for (int n = 1; n <= 8; n++) {
            snprintf(label, sizeof(label), "%dx  (%d x %d)", n, 512 * n, 448 * n);
            if (ImGui::Selectable(label, n == v.scale)) { v.scale = n; }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("How finely the game is drawn. 1x is the PlayStation 2's own resolution.\nHigher is sharper and uses more video memory.");
    for (int i = 1; i < (int)(sizeof(aspects) / sizeof(aspects[0])); i++) {
        if (abs(aspects[i].milli - v.aspectMilli) < abs(aspects[best].milli - v.aspectMilli)) { best = i; }
    }
    if (ImGui::BeginCombo("Aspect ratio", aspects[best].name)) {
        for (int i = 0; i < (int)(sizeof(aspects) / sizeof(aspects[0])); i++) {
            if (ImGui::Selectable(aspects[i].name, i == best)) { v.aspectMilli = aspects[i].milli; }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Wider than 4:3 shows more of the fight to the sides. Menus are stretched.");
    bool full = v.fullscreen != 0;
    if (ImGui::Checkbox("Full screen", &full)) { v.fullscreen = full; }
    ImGui::SameLine();
    ImGui::TextDisabled("(F11)");
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    if (v.display > count) { v.display = 0; }
    snprintf(label, sizeof(label), "%s", v.display == 0 ? "Chosen by the desktop" : SDL_GetDisplayName(displays[v.display - 1]));
    if (ImGui::BeginCombo("Display", label)) {
        if (ImGui::Selectable("Chosen by the desktop", v.display == 0)) { v.display = 0; }
        for (int i = 0; i < count; i++) {
            snprintf(label, sizeof(label), "%d: %s", i + 1, SDL_GetDisplayName(displays[i]));
            if (ImGui::Selectable(label, v.display == i + 1)) { v.display = i + 1; }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Takes effect the next time the game starts.");
    SDL_free(displays);
    ImGui::Spacing();
    if (v.texPackCount > 0) {
        bool on = v.texPack != 0;
        snprintf(label, sizeof(label), "Texture pack (%d textures)", v.texPackCount);
        if (ImGui::Checkbox(label, &on)) { v.texPack = on; }
        ImGui::SetItemTooltip("Replacement textures from the folder \"textures\" next to the game.");
    } else {
        ImGui::BeginDisabled();
        bool off = false;
        ImGui::Checkbox("Texture pack (none found)", &off);
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Put a pack's files (PCSX2 naming, .dds or .png) into a folder \"textures\" next to the game.");
    }
}

static void effects_tab(PortVideo &v) {
    static const struct { const char *name, *tip; } fx[5] = {
        {"Outline", "The black line around the fighters."},
        {"See-through tint", "The tint that shows a fighter behind scenery."},
        {"Depth tint", "The haze that colours distant things."},
        {"Glare and glow", "The bloom around bright things and the sky's glare."},
        {"Distance blur", "The soft focus on distant scenery."},
    };
    for (int i = 0; i < 5; i++) {
        bool on = !((v.fxOff >> i) & 1);
        if (ImGui::Checkbox(fx[i].name, &on)) { v.fxOff = (v.fxOff & ~(1 << i)) | (on ? 0 : 1 << i); }
        ImGui::SetItemTooltip("%s", fx[i].tip);
    }
    ImGui::Spacing();
    ImGui::SliderInt("Glow strength", &v.glow, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("100%% is what the PlayStation 2 draws; the default here is 60%%.");
}

static void audio_tab(PortVideo &v) {
    ImGui::SliderInt("Music", &v.music, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt("Sound effects and voices", &v.effects, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("Takes effect with the next sound that starts.");
}

// Cheats. "Unlock everything" is the game's own leftover debug function (Save_UnlockAll, which nothing in the game
// calls): the request is set here and carried out on the game's side at the next vertical blank (headless.c).
extern "C" {
extern volatile int gPortUnlockAll, gPortUnlockDone;
}

static void cheats_tab(void) {
    ImGui::TextWrapped("Unlock everything: all characters, stages, music and items, and the largest amount of Zenni. "
                       "This is a debug function the game's developers left in.");
    ImGui::Spacing();
    ImGui::TextWrapped("It also empties your records list, and it becomes permanent the next time the game saves. "
                       "Use it after your save has been loaded (from the main menu on).");
    ImGui::Spacing();
    if (ImGui::Button("Unlock everything...")) {
        ImGui::OpenPopup("Unlock everything?");
    }
    if (gPortUnlockDone) {
        ImGui::SameLine();
        ImGui::TextUnformatted("Done.");
    }
    if (ImGui::BeginPopupModal("Unlock everything?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Your records list will be emptied. This cannot be undone once the game saves.");
        ImGui::Spacing();
        if (ImGui::Button("Unlock", ImVec2(140.0f, 0.0f))) {
            gPortUnlockAll = 1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(140.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void controls_tab(void) {
    static int player;
    char label[96];
    int count = 0;

    ImGui::RadioButton("Player 1", &player, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Player 2", &player, 1);
    int *slot = PortInput_PadSlot(player), *keys = PortInput_Keys(player), *pads = PortInput_PadButtons(player);
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (*slot < 0) {
        snprintf(label, sizeof(label), "None");
    } else if (*slot < count) {
        snprintf(label, sizeof(label), "%d: %s", *slot + 1, SDL_GetGamepadNameForID(ids[*slot]));
    } else {
        snprintf(label, sizeof(label), "Controller %d (not connected)", *slot + 1);
    }
    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::BeginCombo("Controller", label)) {
        if (ImGui::Selectable("None", *slot < 0)) { *slot = -1; PortInput_Save(); }
        for (int i = 0; i < (count > 4 ? count : 4); i++) {
            if (i < count) {
                snprintf(label, sizeof(label), "%d: %s", i + 1, SDL_GetGamepadNameForID(ids[i]));
            } else {
                snprintf(label, sizeof(label), "Controller %d (not connected)", i + 1);
            }
            if (ImGui::Selectable(label, *slot == i)) { *slot = i; PortInput_Save(); }
        }
        ImGui::EndCombo();
    }
    SDL_free(ids);
    ImGui::SameLine();
    if (ImGui::Button("Reset to defaults")) {
        PortInput_ResetDefaults(player);
        PortInput_Save();
    }
    ImGui::TextDisabled("Click a binding, then press the new key or button. Esc keeps it, Delete clears it.");
    if (ImGui::BeginTable("bindings", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV,
                          ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Action");
        ImGui::TableSetupColumn("Keyboard");
        ImGui::TableSetupColumn("Controller");
        ImGui::TableHeadersRow();
        for (int a = 0; a < PORT_ACTIONS; a++) {
            bool waitKey = sCapKind == 1 && sCapPlayer == player && sCapAction == a;
            bool waitPad = sCapKind == 2 && sCapPlayer == player && sCapAction == a;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(PortInput_ActionLabel(a));
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##k%d", waitKey ? "press a key..." : keys[a] > 0 ? SDL_GetScancodeName((SDL_Scancode)keys[a]) : "-", a);
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                sCapKind = 1; sCapPlayer = player; sCapAction = a; gPortInputCapture = 1;
            }
            ImGui::TableNextColumn();
            if (a < PORT_PAD_BUTTONS) {
                snprintf(label, sizeof(label), "%s##p%d", waitPad ? "press a button..." : pad_source_name(pads[a]), a);
                if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                    sCapKind = 2; sCapPlayer = player; sCapAction = a; gPortInputCapture = 1;
                }
            } else {
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled(a < 20 ? "Left stick" : "Right stick");
            }
        }
        ImGui::EndTable();
    }
}

static void build(void) {
    PortVideo v, was;
    ImGuiIO &io = ImGui::GetIO();
    bool open = true;

    GsGpu_GetSettings(&v);
    was = v;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(640.0f, 560.0f), ImGuiCond_Appearing);
    if (ImGui::Begin("Settings", &open, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::BeginTabBar("tabs")) {
            if (tab("Video", 0)) { video_tab(v); ImGui::EndTabItem(); }
            if (tab("Effects", 1)) { effects_tab(v); ImGui::EndTabItem(); }
            if (tab("Audio", 2)) { audio_tab(v); ImGui::EndTabItem(); }
            if (tab("Controls", 3)) { controls_tab(); ImGui::EndTabItem(); }
            if (tab("Cheats", 4)) { cheats_tab(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        if (ImGui::GetFrameCount() > 3) {
            sForceTab = -1;
        }
    }
    ImGui::End();
    if (memcmp(&v, &was, sizeof(v)) != 0) {
        GsGpu_SetSettings(&v);
    }
    if (!open) {
        set_open(false);
    }
}

void Ui_DrawAgain(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) {
    ImDrawData *dd = sReady ? ImGui::GetDrawData() : NULL;
    SDL_GPUColorTargetInfo ti;
    SDL_GPURenderPass *pass;

    if (dd == NULL || dd->CmdListsCount == 0 || dd->DisplaySize.x <= 0.0f || dd->DisplaySize.y <= 0.0f) {
        return;
    }
    ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd);
    SDL_zero(ti);
    ti.texture = target;
    ti.load_op = SDL_GPU_LOADOP_LOAD;
    ti.store_op = SDL_GPU_STOREOP_STORE;
    pass = SDL_BeginGPURenderPass(cmd, &ti, 1, NULL);
    ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, pass);
    SDL_EndGPURenderPass(pass);
}

void Ui_Draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) {
    int overlay = gUiNameReady != 0 && gUiNameIdx >= 0 && gUiNameIdx < (int)sNameCount;

    if (!sReady || (!sOpen && !overlay)) {
        return;
    }
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (overlay && gUiPresentW > 0 && gUiPresentH > 0) {
        /* The name's rectangle is in the game's 512x448 pixels: map it through the picture's rectangle in the
           window (the letterbox gs_gpu.c just blitted into). */
        float sx = (float)gUiPresentW / 512.0f;
        float sy = (float)gUiPresentH / 448.0f;
        float x = (float)gUiPresentX + (float)gUiNameX * sx;
        float y = (float)gUiPresentY + (float)gUiNameY * sy;
        float w = (float)gUiNameW * sx;
        float h = (float)gUiNameH * sy;
        ImVec2 uv0(0.0f, (float)gUiNameIdx / (float)sNameCount);
        ImVec2 uv1(1.0f, (float)(gUiNameIdx + 1) / (float)sNameCount);
        ImGui::GetForegroundDrawList()->AddImage(ImTextureRef((ImTextureID)(intptr_t)sNameTex),
                                                 ImVec2(x, y), ImVec2(x + w, y + h), uv0, uv1);
    }
    if (sOpen) {
        build();
    }
    ImGui::Render();
    Ui_DrawAgain(cmd, target);
}
