// SPDX-License-Identifier: GPL-3.0-or-later
// What the settings menus share, so they look and work the same on every
// desktop build: the Mac and Linux menu (settings_menu.cpp) and the Windows one
// (windows/src/win_settings.cpp). Widgets that wrap in a narrow menu, and a
// controller in them.
#pragma once
#include <imgui.h>
#include <algorithm>
#include <imgui_internal.h> // ClearActiveID, ClosePopupsExceptModals, CalcTypematicRepeatAmount
#include <cmath>
#include <utility>
#include <string>
#include "button_remap.h"

inline int bw_menu_slider_nudge(); // below: a controller's left and right on a slider

// One setting's choices, with no dropdown: see bw_menu_combo's comment.
inline bool bw_menu_combo(const char* label, int* index, const char* const* items, int count) {
    // The same control for every list, however long: the setting's name, the
    // chosen one under it, and a slider with a step for each, which a
    // controller nudges left and right like any other.
    ImGui::PushID(label);
    ImGui::TextWrapped("%s", label);
    // Short enough and every choice is named over its own step, the chosen one
    // bright and the rest quiet; otherwise only the chosen one is written, in
    // the same place whichever it is, so there is one spot to read.
    const int at = std::clamp(*index, 0, count > 0 ? count - 1 : 0);
    const float width = ImGui::GetContentRegionAvail().x;
    const float slot = count > 0 ? width / static_cast<float>(count) : width;
    float widest = 0.f;
    for (int i = 0; i < count; ++i)
        widest = std::max(widest, ImGui::CalcTextSize(items[i]).x);
    if (widest <= slot - ImGui::GetStyle().ItemInnerSpacing.x * 2.f) {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* list = ImGui::GetWindowDrawList();
        const ImU32 chosen = ImGui::GetColorU32(ImGuiCol_Text);
        const ImU32 quiet = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        for (int i = 0; i < count; ++i) {
            const float text = ImGui::CalcTextSize(items[i]).x;
            const float x = origin.x + slot * (static_cast<float>(i) + 0.5f) - text * 0.5f;
            list->AddText(ImVec2(std::clamp(x, origin.x, origin.x + width - text), origin.y),
                          i == at ? chosen : quiet, items[i]);
        }
        ImGui::Dummy(ImVec2(width, ImGui::GetTextLineHeight()));
    } else {
        ImGui::TextUnformatted(items[at]);
    }
    // A chooser, not a scrubber. ImGui keeps the behaviour but draws no handle:
    // ours is drawn below, lit from above, over a track notched once per step.
    const float grab_w = std::max(14.f, slot * 0.9f);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, IM_COL32_BLACK_TRANS);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, IM_COL32_BLACK_TRANS);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, grab_w);
    ImGui::SetNextItemWidth(-1.f);
    bool changed = ImGui::SliderInt("##value", index, 0, count - 1, "", ImGuiSliderFlags_NoInput);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    const ImVec2 lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
    ImDrawList* paint = ImGui::GetWindowDrawList();
    const float round = ImGui::GetStyle().FrameRounding;
    // The track as a groove: a dark edge under its top, a light one on its
    // floor, so it reads as cut into the panel rather than drawn on it.
    paint->AddLine(ImVec2(lo.x + round, lo.y + 1.f), ImVec2(hi.x - round, lo.y + 1.f), IM_COL32(0, 0, 0, 60), 1.f);
    paint->AddLine(ImVec2(lo.x + round, hi.y - 1.f), ImVec2(hi.x - round, hi.y - 1.f), IM_COL32(255, 255, 255, 18),
                   1.f);
    if (count > 1) {
        const ImU32 tint = ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.45f);
        const float inset = grab_w * 0.5f + 2.f;
        for (int i = 0; i < count; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(count - 1);
            const float x = lo.x + inset + (hi.x - lo.x - inset * 2.f) * t;
            paint->AddLine(ImVec2(x, hi.y - 4.f), ImVec2(x, hi.y - 2.f), tint, 1.f);
        }
    }
    // The handle: a shadow beneath it, its face brighter at the top than the
    // bottom, and a thin highlight along its top edge.
    {
        const float pad = 2.f;
        const float sz = std::min(grab_w, hi.x - lo.x - pad * 2.f);
        const float t = count > 1 ? static_cast<float>(at) / static_cast<float>(count - 1) : 0.f;
        const float cx = (lo.x + pad + sz * 0.5f) + (hi.x - pad - sz * 0.5f - (lo.x + pad + sz * 0.5f)) * t;
        const ImVec2 a(cx - sz * 0.5f, lo.y + pad), b(cx + sz * 0.5f, hi.y - pad);
        const float r = (b.y - a.y) * 0.5f;
        const ImVec4 face = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
        const bool held = ImGui::IsItemActive();
        const auto shade = [&](float k, float alpha) {
            return ImGui::ColorConvertFloat4ToU32(
                ImVec4(std::min(1.f, face.x * k), std::min(1.f, face.y * k), std::min(1.f, face.z * k), alpha));
        };
        paint->AddRectFilled(ImVec2(a.x, a.y + 2.f), ImVec2(b.x, b.y + 2.f), IM_COL32(0, 0, 0, 70), r); // shadow
        paint->AddRectFilled(a, b, shade(held ? 1.15f : 0.95f, 1.f), r);                               // face
        paint->AddRectFilled(a, ImVec2(b.x, a.y + (b.y - a.y) * 0.5f), shade(held ? 1.4f : 1.2f, 0.55f), r,
                             ImDrawFlags_RoundCornersTop); // lit from above
        paint->AddLine(ImVec2(a.x + r, a.y + 1.f), ImVec2(b.x - r, a.y + 1.f), IM_COL32(255, 255, 255, 60), 1.f);
    }
    if (const int step = bw_menu_slider_nudge()) {
        const int moved = std::clamp(*index + step, 0, count - 1);
        changed = changed || moved != *index;
        *index = moved;
    }
    ImGui::PopID();
    return changed;
}

// A checkbox whose label wraps when the window is too narrow for it; clicking
// the wrapped label toggles it too.
inline bool bw_menu_checkbox(const char* label, bool* value) {
    const ImGuiStyle& style = ImGui::GetStyle();
    if (ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(label).x <=
        ImGui::GetContentRegionAvail().x)
        return ImGui::Checkbox(label, value);
    ImGui::PushID(label);
    bool changed = ImGui::Checkbox("##box", value);
    ImGui::SameLine(0.f, style.ItemInnerSpacing.x);
    ImGui::TextWrapped("%s", label);
    if (ImGui::IsItemClicked()) {
        *value = !*value;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

// --- a controller in the menu ----------------------------------------------
//
// ImGui's own controller navigation is off in the menus (NavEnableGamepad):
// it scrolls the window with the left stick, which nothing outside ImGui can
// switch off, and it chooses and cancels with fixed face buttons. Instead
// bw_menu_controller reads the controller from SDL once a frame and hands ImGui
// keys: the D-pad and the left stick are the arrow keys, held while they're
// held so ImGui repeats them as it does a held key, and the game's A is Space,
// which chooses. The game's B, the bumpers and the right stick come back to
// the menu, which acts on them itself.

struct BwMenuPad {
    int tab_step = 0;   // -1 for LB, 1 for RB, pressed this frame
    bool back = false;  // the game's B, pressed this frame
    float scroll = 0.f; // the right stick, -1 (up) to 1 (down); 0 inside its dead zone
};

// Call it once a frame while the menu is open, before its window. The game's A
// and B are where the player put them: a custom layout (Controls › Controller
// buttons; "" is the controller's own), else Swap A and B, as the game reads them.
inline BwMenuPad bw_menu_controller(const std::string& button_map, bool swap_ab) {
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    BwButtonMap map;
    const bool custom = bw_button_map_parse(button_map, &map);
    unsigned a = map.native[0], b = map.native[1];
    if (!custom && swap_ab)
        std::swap(a, b);
    bool held[SDL_GAMEPAD_BUTTON_COUNT] = {};
    float lx = 0.f, ly = 0.f, ry = 0.f;
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int i = 0; ids != nullptr && i < count; ++i) {
        SDL_Gamepad* pad = SDL_GetGamepadFromID(ids[i]);
        if (pad == nullptr)
            continue;
        for (int k = 0; k < SDL_GAMEPAD_BUTTON_COUNT; ++k)
            held[k] = held[k] || SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(k));
        const auto stronger = [pad](float& to, SDL_GamepadAxis axis) {
            const float v = SDL_GetGamepadAxis(pad, axis) / 32767.f;
            if (std::fabs(v) > std::fabs(to))
                to = v;
        };
        stronger(lx, SDL_GAMEPAD_AXIS_LEFTX);
        stronger(ly, SDL_GAMEPAD_AXIS_LEFTY);
        stronger(ry, SDL_GAMEPAD_AXIS_RIGHTY);
    }
    SDL_free(ids);
    const auto button = [&held](unsigned k) { return k < SDL_GAMEPAD_BUTTON_COUNT && held[k]; };
    // The stick is one direction at a time, its stronger axis, past half way.
    const bool stick = std::fabs(lx) >= 0.5f || std::fabs(ly) >= 0.5f;
    const bool across = std::fabs(lx) > std::fabs(ly);
    // Presses are a physical button going down, so a button already held when
    // the menu opens, or when the player remaps it in Controls (picking the
    // game's B with the button that then becomes B), does nothing until it is
    // pressed again.
    static bool was_held[SDL_GAMEPAD_BUTTON_COUNT];
    static int last_frame = -10;
    const bool reopened = ImGui::GetFrameCount() != last_frame + 1;
    last_frame = ImGui::GetFrameCount();
    const auto pressed = [&](unsigned k) {
        return !reopened && k < SDL_GAMEPAD_BUTTON_COUNT && held[k] && !was_held[k];
    };
    // The keys ImGui gets: the arrows while the D-pad or the stick is held, and
    // Space from a press of the game's A until it's let go.
    static const ImGuiKey kKey[] = {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow,
                                    ImGuiKey_DownArrow, ImGuiKey_Space};
    static bool key_down[5];
    const bool now[5] = {
        button(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || (stick && across && lx < 0.f),
        button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || (stick && across && lx > 0.f),
        button(SDL_GAMEPAD_BUTTON_DPAD_UP) || (stick && !across && ly < 0.f),
        button(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || (stick && !across && ly > 0.f),
        button(a) && (key_down[4] || pressed(a)),
    };
    // Opened again (or the first time): a direction already held counts once
    // it's let go and pushed again, as A does.
    static bool wait_release[4];
    for (int i = 0; i < 5; ++i) {
        if (i < 4) {
            if (reopened)
                wait_release[i] = now[i];
            else if (!now[i])
                wait_release[i] = false;
        }
        const bool down = !reopened && now[i] && !(i < 4 && wait_release[i]);
        if (down != key_down[i])
            io.AddKeyEvent(kKey[i], down);
        key_down[i] = down;
    }
    BwMenuPad out;
    out.back = pressed(b);
    if (pressed(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
        out.tab_step = -1;
    else if (pressed(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
        out.tab_step = 1;
    for (int k = 0; k < SDL_GAMEPAD_BUTTON_COUNT; ++k)
        was_held[k] = held[k];
    if (std::fabs(ry) > 0.2f)
        out.scroll = ry;
    return out;
}

// A selected slider moves with left and right (the arrow keys, which the
// D-pad and the left stick hold: bw_menu_controller) without A to grab it
// first: -1, 1, or 0 for no press, repeating while held. Call it right after
// the slider; it fills its row, so left and right have nothing else to do there.
inline int bw_menu_slider_nudge() {
    if (!ImGui::IsItemFocused() || ImGui::IsItemActive())
        return 0;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        return -1;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        return 1;
    return 0;
}

// The right stick (BwMenuPad::scroll) scrolls the current window, a tab's body:
// the game isn't listening while the menu is open.
inline void bw_menu_stick_scroll(float scroll) {
    if (scroll != 0.f)
        ImGui::SetScrollY(ImGui::GetScrollY() + scroll * 900.f * ImGui::GetIO().DeltaTime);
}

// The game's B (BwMenuPad::back) ends an edit or closes an open dropdown, and
// otherwise closes the menu. Call it inside the menu's window; true: close.
inline bool bw_menu_back(bool pressed) {
    if (!pressed)
        return false;
    if (ImGui::IsAnyItemActive()) {
        ImGui::ClearActiveID();
        return false;
    }
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
        ImGui::ClosePopupsExceptModals();
        return false;
    }
    return true;
}

// The footer both menus end with: the hint, the next-launch note when there is
// one, and the buttons, on one row or, when narrow, two.
enum BwMenuButton { BW_MENU_NONE, BW_MENU_RESUME, BW_MENU_SAVE_STATE, BW_MENU_LOAD_STATE, BW_MENU_QUIT };
inline const char* const kBwMenuButtons[] = {"Resume", "Save state (F5)", "Load latest state (F8)", "Quit the game"};

// The footer's height, for the tab body above it (BeginChild's -height).
inline float bw_menu_footer_height(bool restart_note) {
    const ImGuiStyle& style = ImGui::GetStyle();
    float buttons = -style.ItemSpacing.x;
    for (const char* label : kBwMenuButtons)
        buttons += ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.f + style.ItemSpacing.x;
    const int rows = buttons > ImGui::GetContentRegionAvail().x ? 2 : 1;
    return style.ItemSpacing.y * 2.f + ImGui::GetTextLineHeightWithSpacing() +
           (restart_note ? ImGui::GetFrameHeightWithSpacing() : 0.f) + ImGui::GetFrameHeightWithSpacing() * rows;
}

// The hint and the next-launch note. The caller puts its Restart now button,
// if it has one, on the same line right after it (ImGui::SameLine()).
inline void bw_menu_footer_note(bool restart_note) {
    ImGui::Separator();
    ImGui::TextDisabled("A chooses, B closes, LB and RB change tabs, the right stick scrolls.");
    if (restart_note) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(1.f, 0.8f, 0.3f, 1.f), "Some changes take effect at the next launch.");
    }
}

// The buttons; returns the one pressed. Resume's tooltip names where the
// preferences are saved, when given.
inline BwMenuButton bw_menu_footer_buttons(const char* settings_path) {
    BwMenuButton pressed = BW_MENU_NONE;
    for (int i = 0; i < 4; ++i) {
        if (i > 0) {
            const float width = ImGui::CalcTextSize(kBwMenuButtons[i]).x + ImGui::GetStyle().FramePadding.x * 2.f;
            const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            if (right - ImGui::GetItemRectMax().x > width + ImGui::GetStyle().ItemSpacing.x)
                ImGui::SameLine();
        }
        if (ImGui::Button(kBwMenuButtons[i]))
            pressed = static_cast<BwMenuButton>(i + 1);
        if (i == 0 && settings_path != nullptr && settings_path[0] != '\0' && ImGui::IsItemHovered())
            ImGui::SetTooltip("Preferences are saved when you resume:\n%s", settings_path);
    }
    return pressed;
}
