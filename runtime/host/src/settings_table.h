// SPDX-License-Identifier: GPL-3.0-or-later
// Every desktop setting once: the host's environment name (what the game
// reads, and the settings file's key), the name the Windows settings file used
// before the menus were one, its kind, its default, its range, its label and
// tab, and whether it takes effect at the next launch. The order is the
// settings file's. The menus draw their plain switches and sliders from it;
// settings with their own controls (Smooth Motion's rate, Jump & Sprint, the
// texture pack, the remaps, Better Wind Waker's options) only take their
// names and defaults from it. No ImGui here, so the host tests can read it.
#pragma once
#include <cstring>

namespace bw_settings {

enum class Kind { Bool, Int, Float, Choice, Text };
// How a tab draws the setting. Switch, Slider and Dropdown are drawn by the
// shared draw_tab; Custom names a drawing function of its own (Smooth Motion's
// rate, the texture pack, the remaps, Better Wind Waker's options, the
// keyboard and controller tables); None is not drawn on its own at all.
enum class Control { None, Switch, Slider, Dropdown, Custom };
enum class Tab { Display, Gameplay, Mods, Controls, SoundFiles };

struct Setting {
    const char* key;      // environment variable and settings file key
    const char* windows;  // the Windows settings file's old key, nullptr if none
    Kind kind;
    const char* fallback; // the value when the variable is unset ("" for none)
    float low, high;      // Int and Float: the menu's range
    const char* label;    // nullptr: drawn as part of another setting's control
    Tab tab;
    bool next_launch;     // takes effect when BlueWake starts again
    Control control;      // how the shared draw_tab draws it; table order is display order
    const char* note;     // the quiet line under it, nullptr for none
    const char* needs;    // a Bool setting that must be on, nullptr for always on
    const char* group;    // a heading drawn before it, nullptr to carry on under the last
    const char* shown;    // Slider: how its value reads ("%.0f seconds"), nullptr for "%.2f"
    const char* stored;   // Slider: how it is written to the file, nullptr for "%.3f"
};

// clang-format off
inline constexpr Setting kSettings[] = {
    {"DOL_AURORA_TEXTURE_PACK", "texture_pack", Kind::Text, "", 0, 0, "HD textures", Tab::Mods, true, Control::Custom, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_ASPECT", "aspect", Kind::Choice, "4:3", 0, 0, "Widescreen", Tab::Mods, true, Control::Custom, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_HD_LOOK", "hd_look", Kind::Bool, "0", 0, 0, "Wind Waker HD look   (F2)", Tab::Display, false, Control::Switch, "The HD release's look, rebuilt piece by piece. So far: islands on the horizon from further away, soft shading, sun and cloud shadows and edge light outdoors, torch light, a soft distance and bloom. Off by default.", nullptr, nullptr, nullptr, nullptr},
    // Custom: the button that goes fullscreen now is drawn above the switch.
    {"DOL_AURORA_FULLSCREEN", "fullscreen", Kind::Bool, "0", 0, 0, "Start in fullscreen", Tab::Display, true, Control::Custom, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"DOL_AURORA_RENDER_SCALE", "render_scale", Kind::Int, "0", 0, 4, "Render resolution", Tab::Display, false, Control::Dropdown, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"DOL_AURORA_FRAME_INTERP", "smooth_motion", Kind::Bool, "0", 0, 0, "Smooth Motion (experimental)   (F10)", Tab::Display, false, Control::Custom, "The game runs at 30; display changes and overloads can lower the presentation rate.", nullptr, nullptr, nullptr, nullptr},
    {"DOL_AURORA_FRAME_INTERP_STEPS", "smooth_motion_fps", Kind::Choice, "1", 0, 0, nullptr, Tab::Display, false, Control::None, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"DOL_AURORA_SHOW_FPS", "show_fps", Kind::Bool, "0", 0, 0, "Show the frame rate   (F9)", Tab::Display, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"DOL_AURORA_FPS_POSITION", "fps_position", Kind::Choice, "", 0, 0, "FPS position", Tab::Display, false, Control::Dropdown, nullptr, "DOL_AURORA_SHOW_FPS", nullptr, nullptr, nullptr},
    {"DOL_AURORA_FORCE_ANISO", "anisotropy", Kind::Int, "1", 1, 16, "Texture filtering", Tab::Display, false, Control::Dropdown, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_MODS", "betterww", Kind::Text, "", 0, 0, "Better Wind Waker", Tab::Mods, true, Control::Custom, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_OPTIONS", nullptr, Kind::Text, "", 0, 0, nullptr, Tab::Mods, true, Control::None, nullptr, nullptr, nullptr, nullptr, nullptr}, // Windows: option.NAME
    {"BLUEWAKE_FADE_FRAMES", "fast_transitions", Kind::Int, "", 0, 0, nullptr, Tab::Gameplay, false, Control::None, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_FAST_FORWARD", "fast_transitions", Kind::Bool, "1", 0, 0, "Fast transitions (short fades, no waiting in the black)", Tab::Gameplay, false, Control::Switch, nullptr, nullptr, "Doors and exits", nullptr, nullptr},
    {"BLUEWAKE_QUICK_DOORS", "quick_doors", Kind::Bool, "1", 0, 0, "Quick doors (no walk-in or door closing behind Link)", Tab::Gameplay, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_JUMP_BUTTON", "movement_extras", Kind::Bool, "0", 0, 0, "Jump & Sprint (Space or the left bumper jumps; Shift or the left stick click sprints)", Tab::Gameplay, false, Control::Custom, nullptr, nullptr, "Jump & Sprint", nullptr, nullptr},
    {"BLUEWAKE_PAD_SWAP_AB", "controller_swap_ab", Kind::Bool, "0", 0, 0, "Swap A and B", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_PAD_SWAP_XY", "controller_swap_xy", Kind::Bool, "0", 0, 0, "Swap X and Y", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_PAD_SWAP_LR", "controller_swap_lr", Kind::Bool, "0", 0, 0, "Swap L and R (the triggers)", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_DPAD_MENUS", "dpad_menus", Kind::Bool, "0", 0, 0, "D-pad moves the cursor in Items and Quest Status", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_BUTTON_MAP", "button_map", Kind::Text, "", 0, 0, nullptr, Tab::Controls, false, Control::None, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_SPRINT_SPEED", "sprint_speed", Kind::Float, "1", 0, 0, nullptr, Tab::Gameplay, false, Control::None, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_MOUSE_CAMERA", "mouse_camera", Kind::Bool, "1", 0, 0, "Mouse camera (click the game to use it)", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_MOUSE_SENSITIVITY", "mouse_sensitivity", Kind::Float, "1.0", 0.2f, 3.f, "Mouse sensitivity", Tab::Controls, false, Control::Slider, nullptr, "BLUEWAKE_MOUSE_CAMERA", nullptr, "%.2fx", "%.2f"},
    {"BLUEWAKE_MOUSE_INVERT_Y", "mouse_invert_y", Kind::Bool, "0", 0, 0, "Invert the mouse's up and down", Tab::Controls, false, Control::Switch, nullptr, "BLUEWAKE_MOUSE_CAMERA", nullptr, nullptr, nullptr},
    {"BLUEWAKE_MOUSE_BUTTONS", "mouse_buttons", Kind::Text, "", 0, 0, nullptr, Tab::Controls, false, Control::None, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_KEY_MAP", "key_map", Kind::Text, "", 0, 0, nullptr, Tab::Controls, false, Control::None, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_STICK_CAMERA", "stick_camera", Kind::Bool, "1", 0, 0, "Direct right-stick camera and aiming", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_STICK_CAMERA_SPEED", "stick_camera_speed", Kind::Float, "360", 120.f, 720.f, "Right-stick turn speed", Tab::Controls, false, Control::Slider, nullptr, "BLUEWAKE_STICK_CAMERA", nullptr, "%.0f degrees a second", "%.0f"},
    // The inverts are not greyed out with the direct camera off: an invert the
    // player ticked works either way (Windows' behaviour, the agreed one).
    {"BLUEWAKE_STICK_CAMERA_INVERT_X", "controller_invert_x", Kind::Bool, "0", 0, 0, "Invert the right stick's left and right", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_STICK_CAMERA_INVERT_Y", "controller_invert_y", Kind::Bool, "0", 0, 0, "Invert the right stick's up and down", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_STICK_AIM_SPEED", "stick_aim_speed", Kind::Float, "180", 60.f, 480.f, "Right-stick aim speed (first person, items)", Tab::Controls, false, Control::Slider, nullptr, "BLUEWAKE_STICK_CAMERA", nullptr, "%.0f degrees a second", "%.0f"},
    {"BLUEWAKE_STICK_ZOOM", "stick_zoom", Kind::Bool, "0", 0, 0, "Hold the right-stick click and move it up/down to zoom", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_STICK_ZOOM_SPEED", "stick_zoom_speed", Kind::Float, "1.275", 0.25f, 2.f, "Controller zoom speed", Tab::Controls, false, Control::Slider, nullptr, "BLUEWAKE_STICK_ZOOM", nullptr, "%.2fx", "%.3f"},
    {"BLUEWAKE_AIM_INVERT_Y", "aim_invert_y", Kind::Bool, "0", 0, 0, "Invert the left stick's up and down when aiming (first person, items)", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_GYRO_AIM", "gyro_aim", Kind::Bool, "0", 0, 0, "Gyro aiming: move the controller to aim (DualSense, DualShock 4, Switch Pro, Steam Deck)", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_GYRO_AIM_SPEED", "gyro_aim_speed", Kind::Float, "1", 0.25f, 4.f, "Gyro aim speed", Tab::Controls, false, Control::Slider, nullptr, "BLUEWAKE_GYRO_AIM", nullptr, "%.2fx", "%.2f"},
    {"BLUEWAKE_HAPTICS", "haptics", Kind::Choice, "enhanced", 0, 0, "Controller vibration", Tab::Controls, false, Control::Dropdown, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_HAPTICS_STRENGTH", "haptics_strength", Kind::Int, "80", 0, 100, "Vibration strength", Tab::Controls, false, Control::Custom, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_HAPTICS_TRIGGERS", "haptics_triggers", Kind::Bool, "1", 0, 0, "Trigger feedback (Xbox impulse triggers, DualSense trigger vibration)", Tab::Controls, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_CLIMB", "climb", Kind::Bool, "0", 0, 0, "Wall climbing (experimental)", Tab::Gameplay, false, Control::Switch, nullptr, nullptr, "Climbing", nullptr, nullptr},
    {"BLUEWAKE_CLIMB_STAMINA", "climb_stamina", Kind::Float, "12", 4.f, 30.f, "Climbing stamina", Tab::Gameplay, false, Control::Slider, nullptr, "BLUEWAKE_CLIMB", nullptr, "%.0f seconds", "%.0f"},
    {"BLUEWAKE_FOREST_WATER_KEEP_TREES", "forest_water_keep_trees", Kind::Bool, "0", 0, 0, "Keep watered trees when time runs out", Tab::Gameplay, false, Control::Switch, "Forest Water still expires. Refill and continue with the remaining trees.", nullptr, "Forest Water Challenge", nullptr, nullptr},
    {"BLUEWAKE_FOREST_WATER_30_MINUTES", "forest_water_30_minutes", Kind::Bool, "0", 0, 0, "30-minute Forest Water timer", Tab::Gameplay, false, Control::Switch, "Applies the next time you collect Forest Water. Both options are off by default.", nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_STAGE_SELECT_TOGGLE", "stage_select", Kind::Bool, "0", 0, 0, "Developers' stage select (F7)", Tab::Gameplay, true, Control::Custom, "F7 during play opens the menu the developers used to jump to any room, from your own file; F7 in the menu goes back. On a controller, Back (View on Xbox, Share or Create on PlayStation) opens these settings and the button below does the same. Off by default.", nullptr, nullptr, nullptr, nullptr},
    {"DOL_AURORA_ASPECT_FIT", "keep_aspect", Kind::Bool, "0", 0, 0, "Keep the picture's shape (black bars rather than stretching)", Tab::Display, true, Control::Custom, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_PAUSE_UNFOCUSED", "pause_unfocused", Kind::Bool, "0", 0, 0, "Pause while the window is in the background", Tab::Display, false, Control::Switch, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_SHADERS_FIRST", "compile_shaders_first", Kind::Bool, "0", 0, 0, "Compile shaders before playing", Tab::Display, true, Control::Switch, "At start, wait until the shaders from earlier play are ready, so nothing is missing from the picture the first time it is drawn. Otherwise they are made while you play.", nullptr, nullptr, nullptr, nullptr},
    {"BLUEWAKE_SPRINT_CHOICE", "sprint_speed", Kind::Float, "1.5", 1.1f, 2.f, "Sprint speed", Tab::Gameplay, false, Control::None, nullptr, "BLUEWAKE_JUMP_BUTTON", nullptr, "%.2fx", "%.2f"},
    {"BLUEWAKE_DSP_MODE", "lle_audio", Kind::Choice, "hle", 0, 0, nullptr, Tab::SoundFiles, true, Control::Custom, nullptr, nullptr, "Sound", nullptr, nullptr},
};
// clang-format on

inline constexpr int kCount = sizeof kSettings / sizeof kSettings[0];

// The setting named `key`, or nullptr.
inline const Setting* find(const char* key) {
    for (const Setting& setting : kSettings)
        if (std::strcmp(setting.key, key) == 0)
            return &setting;
    return nullptr;
}

// A Bool setting's default.
inline bool default_on(const Setting& setting) { return setting.fallback[0] != '\0' && setting.fallback[0] != '0'; }

} // namespace bw_settings
