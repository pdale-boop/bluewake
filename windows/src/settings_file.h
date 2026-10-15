// SPDX-License-Identifier: GPL-3.0-or-later
// The bridge from the old Windows settings file to the desktop format:
// KEY=VALUE lines with the host's own names (settings_table.h), as the Mac and
// Linux menu writes them. The menu itself no longer goes through Settings (the
// host's variables are the settings now, win_settings.cpp); a file in the old
// Windows format (aspect=, keep_aspect=, option.NAME=...) is still read into
// Settings and written out as those lines once, so a player's settings carry
// over, and win_settings.cpp copies the old file aside first. Lines it doesn't
// know are kept as they were. No Windows calls here: the host tests run it on
// every platform.
#pragma once
#include "settings_state.h"
#include "../../runtime/host/src/fps_position.h"
#include "../../runtime/host/src/settings_table.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

using BwSettingsLines = std::vector<std::pair<std::string, std::string>>;

inline bool bw_settings_bool(const std::string& v) { return v == "1" || v == "true" || v == "on" || v == "yes"; }

// One line of the old Windows file, into `d`: false when the key isn't one.
inline bool bw_settings_read_old(const std::string& k, const std::string& v, Settings& d) {
    if (k == "fullscreen") d.fullscreen = bw_settings_bool(v);
    else if (k == "window") std::sscanf(v.c_str(), "%dx%d", &d.window_w, &d.window_h);
    else if (k == "window_position") std::sscanf(v.c_str(), "%d,%d", &d.window_x, &d.window_y);
    else if (k == "render_scale") d.render_scale = std::clamp(std::atoi(v.c_str()), 0, 4);
    else if (k == "anisotropy") d.anisotropy = std::clamp(std::atoi(v.c_str()), 1, 16);
    else if (k == "smooth_motion") d.smooth_motion = bw_settings_bool(v);
    else if (k == "smooth_motion_fps") d.smooth_steps = v == "display" ? -1 : std::atoi(v.c_str()) >= 120 ? 3 : 1;
    else if (k == "show_fps") d.show_fps = bw_settings_bool(v);
    else if (k == "fps_position") d.fps_position = bw_fps_position(v.c_str());
    else if (k == "pause_unfocused") d.pause_unfocused = bw_settings_bool(v);
    else if (k == "compile_shaders_first") d.shaders_first = bw_settings_bool(v);
    else if (k == "mouse_camera") d.mouse_camera = bw_settings_bool(v);
    else if (k == "mouse_sensitivity") d.mouse_sensitivity = std::clamp(std::atof(v.c_str()), 0.1, 10.0);
    else if (k == "mouse_invert_y") d.mouse_invert_y = bw_settings_bool(v);
    else if (k == "controller_swap_ab") d.controller_swap_ab = bw_settings_bool(v);
    else if (k == "button_map") d.button_map = v;
    else if (k == "mouse_buttons") d.mouse_buttons = v;
    else if (k == "key_map") d.key_map = v;
    else if (k == "controller_swap_xy") d.controller_swap_xy = bw_settings_bool(v);
    else if (k == "controller_swap_lr") d.controller_swap_lr = bw_settings_bool(v);
    else if (k == "dpad_menus") d.dpad_menus = bw_settings_bool(v);
    else if (k == "controller_invert_x") d.pad_invert_x = bw_settings_bool(v);
    else if (k == "controller_invert_y") d.pad_invert_y = bw_settings_bool(v);
    else if (k == "stick_camera") d.stick_camera = bw_settings_bool(v);
    else if (k == "stick_camera_speed") d.stick_speed = std::clamp(std::atoi(v.c_str()), 60, 1080);
    else if (k == "stick_aim_speed") d.stick_aim_speed = std::clamp(std::atoi(v.c_str()), 30, 720);
    else if (k == "stick_zoom") d.stick_zoom = bw_settings_bool(v);
    else if (k == "stick_zoom_speed") d.stick_zoom_speed = std::clamp(std::atof(v.c_str()), 0.25, 2.0);
    else if (k == "aim_invert_y") d.aim_invert_y = bw_settings_bool(v);
    else if (k == "gyro_aim") d.gyro_aim = bw_settings_bool(v);
    else if (k == "gyro_aim_speed") d.gyro_aim_speed = std::clamp(std::atof(v.c_str()), 0.25, 4.0);
    else if (k == "climb") d.climb = bw_settings_bool(v);
    else if (k == "climb_stamina") d.climb_stamina = std::clamp(std::atoi(v.c_str()), 4, 30);
    else if (k == "forest_water_keep_trees") d.forest_keep_trees = bw_settings_bool(v);
    else if (k == "forest_water_30_minutes") d.forest_30_minutes = bw_settings_bool(v);
    else if (k == "hd_look") d.hd_look = bw_settings_bool(v);
    else if (k == "haptics") d.haptics = v == "off" ? 0 : v == "classic" ? 1 : 2;
    else if (k == "haptics_strength") d.haptics_strength = std::clamp(std::atoi(v.c_str()), 0, 100);
    else if (k == "haptics_triggers") d.haptics_triggers = bw_settings_bool(v);
    else if (k == "aspect") d.aspect = (v == "16:9" || v == "16:10" || v == "21:9") ? v : "4:3";
    else if (k == "keep_aspect") d.keep_aspect = bw_settings_bool(v);
    else if (k == "betterww") d.betterww = bw_settings_bool(v);
    else if (k.rfind("option.", 0) == 0) d.options[k.substr(7)] = bw_settings_bool(v);
    else if (k == "hd_textures") d.hd_textures = bw_settings_bool(v);
    else if (k == "texture_pack") d.texture_pack = v;
    else if (k == "lle_audio") d.lle_audio = bw_settings_bool(v);
    else if (k == "movement_extras") d.movement_extras = bw_settings_bool(v);
    else if (k == "sprint_speed") d.sprint_speed = std::clamp(std::atof(v.c_str()), 1.1, 2.0);
    else if (k == "fast_transitions") d.fast_transitions = bw_settings_bool(v);
    else if (k == "quick_doors") d.quick_doors = bw_settings_bool(v);
    else if (k == "stage_select") d.stage_select = bw_settings_bool(v);
    else return false;
    return true;
}

// The Windows-only keys the desktop file also keeps: the window's size and
// place, and the texture pack's folder while HD textures are off.
inline constexpr const char* kBwWindowKey = "DOL_AURORA_WINDOW";
inline constexpr const char* kBwWindowPositionKey = "BLUEWAKE_WINDOW_POSITION";
inline constexpr const char* kBwPackFolderKey = "BLUEWAKE_TEXTURE_PACK_FOLDER";

// The desktop file's lines into `d`. `default_pack` is the folder an empty
// texture_pack stands for (Load\Textures\GZLE01). Lines it doesn't know go to
// `other`. An empty value keeps the default, as a missing line does.
inline void bw_settings_read_new(const BwSettingsLines& lines, const std::string& default_pack, Settings& d,
                                 BwSettingsLines& other) {
    std::string sprint_speed, sprint_choice, pack, pack_folder;
    bool jump = false, have_pack = false;
    for (const auto& [k, v] : lines) {
        if (v.empty() && k != "DOL_AURORA_TEXTURE_PACK") {
            if (bw_settings::find(k.c_str()) == nullptr && k != kBwWindowKey && k != kBwWindowPositionKey &&
                k != kBwPackFolderKey)
                other.emplace_back(k, v);
            continue;
        }
        if (k == "BLUEWAKE_ASPECT") d.aspect = (v == "16:9" || v == "16:10" || v == "21:9") ? v : "4:3";
        else if (k == "DOL_AURORA_FULLSCREEN") d.fullscreen = bw_settings_bool(v);
        else if (k == kBwWindowKey) std::sscanf(v.c_str(), "%dx%d", &d.window_w, &d.window_h);
        else if (k == kBwWindowPositionKey) std::sscanf(v.c_str(), "%d,%d", &d.window_x, &d.window_y);
        else if (k == "DOL_AURORA_RENDER_SCALE") d.render_scale = std::clamp(std::atoi(v.c_str()), 0, 4);
        else if (k == "DOL_AURORA_FRAME_INTERP") d.smooth_motion = bw_settings_bool(v);
        else if (k == "DOL_AURORA_FRAME_INTERP_STEPS") d.smooth_steps = v == "display" ? -1 : std::atoi(v.c_str()) >= 3 ? 3 : 1;
        else if (k == "DOL_AURORA_SHOW_FPS") d.show_fps = bw_settings_bool(v);
        else if (k == "DOL_AURORA_FPS_POSITION") d.fps_position = bw_fps_position(v.c_str());
        else if (k == "DOL_AURORA_FORCE_ANISO") d.anisotropy = std::clamp(std::atoi(v.c_str()), 1, 16);
        else if (k == "DOL_AURORA_TEXTURE_PACK") { pack = v; have_pack = true; }
        else if (k == kBwPackFolderKey) pack_folder = v;
        else if (k == "BLUEWAKE_MODS") d.betterww = v.find("betterww") != std::string::npos;
        else if (k == "BLUEWAKE_OPTIONS") {
            // "a,-b": a on, b off. A list starting "none" (the Mac and Linux
            // menu's) names the ones on.
            size_t at = 0;
            while (at <= v.size()) {
                const size_t end = std::min(v.find(',', at), v.size());
                std::string name = v.substr(at, end - at);
                if (!name.empty() && name != "none") {
                    const bool on = name[0] != '-';
                    d.options[on ? name : name.substr(1)] = on;
                }
                at = end + 1;
            }
        }
        else if (k == "BLUEWAKE_FADE_FRAMES") {}  // follows Fast transitions
        else if (k == "BLUEWAKE_FAST_FORWARD") d.fast_transitions = bw_settings_bool(v);
        else if (k == "BLUEWAKE_QUICK_DOORS") d.quick_doors = bw_settings_bool(v);
        else if (k == "BLUEWAKE_JUMP_BUTTON") jump = bw_settings_bool(v);
        else if (k == "BLUEWAKE_PAD_SWAP_AB") d.controller_swap_ab = bw_settings_bool(v);
        else if (k == "BLUEWAKE_PAD_SWAP_XY") d.controller_swap_xy = bw_settings_bool(v);
        else if (k == "BLUEWAKE_PAD_SWAP_LR") d.controller_swap_lr = bw_settings_bool(v);
        else if (k == "BLUEWAKE_DPAD_MENUS") d.dpad_menus = bw_settings_bool(v);
        else if (k == "BLUEWAKE_BUTTON_MAP") d.button_map = v;
        else if (k == "BLUEWAKE_SPRINT_SPEED") sprint_speed = v;
        else if (k == "BLUEWAKE_MOUSE_CAMERA") d.mouse_camera = bw_settings_bool(v);
        else if (k == "BLUEWAKE_MOUSE_SENSITIVITY") d.mouse_sensitivity = std::clamp(std::atof(v.c_str()), 0.1, 10.0);
        else if (k == "BLUEWAKE_MOUSE_INVERT_Y") d.mouse_invert_y = bw_settings_bool(v);
        else if (k == "BLUEWAKE_MOUSE_BUTTONS") d.mouse_buttons = v;
        else if (k == "BLUEWAKE_KEY_MAP") d.key_map = v;
        else if (k == "BLUEWAKE_STICK_CAMERA") d.stick_camera = bw_settings_bool(v);
        else if (k == "BLUEWAKE_STICK_CAMERA_SPEED") d.stick_speed = std::clamp(static_cast<int>(std::lround(std::atof(v.c_str()))), 60, 1080);
        else if (k == "BLUEWAKE_STICK_CAMERA_INVERT_X") d.pad_invert_x = bw_settings_bool(v);
        else if (k == "BLUEWAKE_STICK_CAMERA_INVERT_Y") d.pad_invert_y = bw_settings_bool(v);
        else if (k == "BLUEWAKE_STICK_AIM_SPEED") d.stick_aim_speed = std::clamp(static_cast<int>(std::lround(std::atof(v.c_str()))), 30, 720);
        else if (k == "BLUEWAKE_STICK_ZOOM") d.stick_zoom = bw_settings_bool(v);
        else if (k == "BLUEWAKE_STICK_ZOOM_SPEED") d.stick_zoom_speed = std::clamp(std::atof(v.c_str()), 0.25, 2.0);
        else if (k == "BLUEWAKE_AIM_INVERT_Y") d.aim_invert_y = bw_settings_bool(v);
        else if (k == "BLUEWAKE_GYRO_AIM") d.gyro_aim = bw_settings_bool(v);
        else if (k == "BLUEWAKE_GYRO_AIM_SPEED") d.gyro_aim_speed = std::clamp(std::atof(v.c_str()), 0.25, 4.0);
        else if (k == "BLUEWAKE_HAPTICS") d.haptics = v == "off" || v == "0" ? 0 : v == "classic" ? 1 : 2;
        else if (k == "BLUEWAKE_HAPTICS_STRENGTH") d.haptics_strength = std::clamp(std::atoi(v.c_str()), 0, 100);
        else if (k == "BLUEWAKE_HAPTICS_TRIGGERS") d.haptics_triggers = bw_settings_bool(v);
        else if (k == "BLUEWAKE_CLIMB") d.climb = bw_settings_bool(v);
        else if (k == "BLUEWAKE_CLIMB_STAMINA") d.climb_stamina = std::clamp(static_cast<int>(std::lround(std::atof(v.c_str()))), 4, 30);
        else if (k == "BLUEWAKE_FOREST_WATER_KEEP_TREES") d.forest_keep_trees = bw_settings_bool(v);
        else if (k == "BLUEWAKE_FOREST_WATER_30_MINUTES") d.forest_30_minutes = bw_settings_bool(v);
        else if (k == "BLUEWAKE_STAGE_SELECT_TOGGLE") d.stage_select = bw_settings_bool(v);
        else if (k == "BLUEWAKE_HD_LOOK") d.hd_look = bw_settings_bool(v);
        else if (k == "DOL_AURORA_ASPECT_FIT") d.keep_aspect = bw_settings_bool(v);
        else if (k == "BLUEWAKE_PAUSE_UNFOCUSED") d.pause_unfocused = bw_settings_bool(v);
        else if (k == "BLUEWAKE_SHADERS_FIRST") d.shaders_first = bw_settings_bool(v);
        else if (k == "BLUEWAKE_SPRINT_CHOICE") sprint_choice = v;
        else if (k == "BLUEWAKE_DSP_MODE") d.lle_audio = v == "lle";
        else other.emplace_back(k, v);
    }
    // Jump & Sprint is on with the jump or a sprint faster than walking; the
    // speed is the chosen one, else the sprint's own.
    const double speed = sprint_speed.empty() ? 1.0 : std::atof(sprint_speed.c_str());
    d.movement_extras = jump || speed > 1.01;
    if (!sprint_choice.empty())
        d.sprint_speed = std::clamp(std::atof(sprint_choice.c_str()), 1.1, 2.0);
    else if (speed > 1.01)
        d.sprint_speed = std::clamp(speed, 1.1, 2.0);
    // HD textures are on while a folder is set; Load\Textures\GZLE01 is kept as
    // "" (the default folder), and the folder chosen before they went off comes back.
    if (have_pack) {
        d.hd_textures = !pack.empty();
        d.texture_pack = pack.empty() ? pack_folder : pack == default_pack ? "" : pack;
    }
}

// `d` as the desktop file's lines, in its order (settings_table.h), then the
// Windows-only ones.
inline BwSettingsLines bw_settings_write_new(const Settings& d, const std::string& default_pack) {
    const auto flag = [](bool on) { return std::string(on ? "1" : "0"); };
    const auto number = [](const char* format, double value) {
        char text[32];
        std::snprintf(text, sizeof text, format, value);
        return std::string(text);
    };
    std::string options;
    for (const auto& [name, on] : d.options)
        options += (options.empty() ? "" : ",") + std::string(on ? "" : "-") + name;
    BwSettingsLines lines;
    for (const bw_settings::Setting& s : bw_settings::kSettings) {
        const std::string k = s.key;
        std::string v;
        if (k == "BLUEWAKE_ASPECT") v = d.aspect;
        else if (k == "DOL_AURORA_FULLSCREEN") v = flag(d.fullscreen);
        else if (k == "DOL_AURORA_RENDER_SCALE") v = std::to_string(d.render_scale);
        else if (k == "DOL_AURORA_FRAME_INTERP") v = flag(d.smooth_motion);
        else if (k == "DOL_AURORA_FRAME_INTERP_STEPS") v = d.smooth_steps == -1 ? "display" : d.smooth_steps >= 3 ? "3" : "1";
        else if (k == "DOL_AURORA_SHOW_FPS") v = flag(d.show_fps);
        else if (k == "DOL_AURORA_FPS_POSITION") v = kBwFpsPositionValues[std::clamp(d.fps_position, 0, BW_FPS_POSITIONS - 1)];
        else if (k == "DOL_AURORA_FORCE_ANISO") v = std::to_string(d.anisotropy);
        else if (k == "DOL_AURORA_TEXTURE_PACK") v = d.hd_textures ? (d.texture_pack.empty() ? default_pack : d.texture_pack) : "";
        else if (k == "BLUEWAKE_MODS") v = d.betterww ? "betterww" : "";
        else if (k == "BLUEWAKE_OPTIONS") v = options;
        else if (k == "BLUEWAKE_FADE_FRAMES") v = d.fast_transitions ? "6" : "0";
        else if (k == "BLUEWAKE_FAST_FORWARD") v = flag(d.fast_transitions);
        else if (k == "BLUEWAKE_QUICK_DOORS") v = flag(d.quick_doors);
        else if (k == "BLUEWAKE_JUMP_BUTTON") v = flag(d.movement_extras);
        else if (k == "BLUEWAKE_PAD_SWAP_AB") v = flag(d.controller_swap_ab);
        else if (k == "BLUEWAKE_PAD_SWAP_XY") v = flag(d.controller_swap_xy);
        else if (k == "BLUEWAKE_PAD_SWAP_LR") v = flag(d.controller_swap_lr);
        else if (k == "BLUEWAKE_DPAD_MENUS") v = flag(d.dpad_menus);
        else if (k == "BLUEWAKE_BUTTON_MAP") v = d.button_map;
        else if (k == "BLUEWAKE_SPRINT_SPEED") v = d.movement_extras ? number("%.2f", d.sprint_speed) : "1";
        else if (k == "BLUEWAKE_MOUSE_CAMERA") v = flag(d.mouse_camera);
        else if (k == "BLUEWAKE_MOUSE_SENSITIVITY") v = number("%.2f", d.mouse_sensitivity);
        else if (k == "BLUEWAKE_MOUSE_INVERT_Y") v = flag(d.mouse_invert_y);
        else if (k == "BLUEWAKE_MOUSE_BUTTONS") v = d.mouse_buttons;
        else if (k == "BLUEWAKE_KEY_MAP") v = d.key_map;
        else if (k == "BLUEWAKE_STICK_CAMERA") v = flag(d.stick_camera);
        else if (k == "BLUEWAKE_STICK_CAMERA_SPEED") v = std::to_string(d.stick_speed);
        else if (k == "BLUEWAKE_STICK_CAMERA_INVERT_X") v = flag(d.pad_invert_x);
        else if (k == "BLUEWAKE_STICK_CAMERA_INVERT_Y") v = flag(d.pad_invert_y);
        else if (k == "BLUEWAKE_STICK_AIM_SPEED") v = std::to_string(d.stick_aim_speed);
        else if (k == "BLUEWAKE_STICK_ZOOM") v = flag(d.stick_zoom);
        else if (k == "BLUEWAKE_STICK_ZOOM_SPEED") v = number("%.3f", d.stick_zoom_speed);
        else if (k == "BLUEWAKE_AIM_INVERT_Y") v = flag(d.aim_invert_y);
        else if (k == "BLUEWAKE_GYRO_AIM") v = flag(d.gyro_aim);
        else if (k == "BLUEWAKE_GYRO_AIM_SPEED") v = number("%.2f", d.gyro_aim_speed);
        else if (k == "BLUEWAKE_HAPTICS") v = d.haptics == 0 ? "off" : d.haptics == 1 ? "classic" : "enhanced";
        else if (k == "BLUEWAKE_HAPTICS_STRENGTH") v = std::to_string(d.haptics_strength);
        else if (k == "BLUEWAKE_HAPTICS_TRIGGERS") v = flag(d.haptics_triggers);
        else if (k == "BLUEWAKE_CLIMB") v = flag(d.climb);
        else if (k == "BLUEWAKE_CLIMB_STAMINA") v = std::to_string(d.climb_stamina);
        else if (k == "BLUEWAKE_FOREST_WATER_KEEP_TREES") v = flag(d.forest_keep_trees);
        else if (k == "BLUEWAKE_FOREST_WATER_30_MINUTES") v = flag(d.forest_30_minutes);
        else if (k == "BLUEWAKE_STAGE_SELECT_TOGGLE") v = flag(d.stage_select);
        else if (k == "BLUEWAKE_HD_LOOK") v = flag(d.hd_look);
        else if (k == "DOL_AURORA_ASPECT_FIT") v = flag(d.keep_aspect);
        else if (k == "BLUEWAKE_PAUSE_UNFOCUSED") v = flag(d.pause_unfocused);
        else if (k == "BLUEWAKE_SHADERS_FIRST") v = flag(d.shaders_first);
        else if (k == "BLUEWAKE_SPRINT_CHOICE") v = number("%.2f", d.sprint_speed);
        else if (k == "BLUEWAKE_DSP_MODE") v = d.lle_audio ? "lle" : "hle";
        lines.emplace_back(k, v);
    }
    if (d.window_w > 0 && d.window_h > 0)
        lines.emplace_back(kBwWindowKey, std::to_string(d.window_w) + "x" + std::to_string(d.window_h));
    if (d.window_x != INT_MIN && d.window_y != INT_MIN)
        lines.emplace_back(kBwWindowPositionKey, std::to_string(d.window_x) + "," + std::to_string(d.window_y));
    if (!d.hd_textures && !d.texture_pack.empty())
        lines.emplace_back(kBwPackFolderKey, d.texture_pack);
    return lines;
}

// A settings file in the old Windows format (aspect=, option.NAME=...) as the
// desktop format's lines, its unknown lines after them: true when `lines` were
// in the old format and are converted, false (and left alone) otherwise.
inline bool bw_settings_convert_old(BwSettingsLines& lines, const std::string& default_pack) {
    Settings old;
    bool old_format = false;
    for (const auto& [k, v] : lines)
        if (k.rfind("BLUEWAKE_", 0) != 0 && k.rfind("DOL_", 0) != 0 && bw_settings_read_old(k, v, old))
            old_format = true;
    if (!old_format)
        return false;
    Settings ignored;
    BwSettingsLines converted = bw_settings_write_new(old, default_pack);
    for (const auto& [k, v] : lines)
        if (!bw_settings_read_old(k, v, ignored))
            converted.emplace_back(k, v);
    lines = converted;
    return true;
}
