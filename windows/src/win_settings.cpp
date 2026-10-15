// BlueWake for Windows: the settings, the in-game settings menu (F1 or Esc),
// the desktop hotkeys (F11 and Alt+Enter fullscreen, F10 Smooth Motion, F9 the
// frame rate, F2 the HD look; F5, F7 and F8 are the shared code's) and the
// window's placement.
//
// The settings live in %APPDATA%\BlueWake\settings.ini. At launch they become
// the host's own variables (BLUEWAKE_*, DOL_*) unless the command line already
// set one, so the command line wins for that session. In the menu, display and
// control settings apply at once; the mods, Better Wind Waker's options, the
// picture's shape and the audio mode apply when BlueWake restarts (Restart now
// starts it again with the environment it began with).
//
// The menu is ImGui, drawn through the host overlay hook inside Aurora's frame
// (gxruntime/aurora_backend.h). The game keeps running under it; while it is
// open the keyboard is taken off the game's pad and the mouse camera stands
// aside, so the menu's clicks and keys do not move Link.
#include "win_settings.h"
#include "settings_state.h"
#include "settings_file.h"
#include "smooth_rate.h"
#include "fps_position.h"
#include "controller_face_swap.h"
#include "restart_request.h"
#include "launch_marker.h"
#include "atomic_file.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <SDL3/SDL.h>
#include <aurora/aurora.h>
#include <aurora/imgui.h>
#include <dolphin/pad.h>
#include <imgui.h>
#include <imgui_internal.h> // FindWindowByName: the menu's position before Begin draws it
#include "button_remap.h"
#include "menu_draw.h"
#include "menu_shared.h"
#include "input_remap.h"
#include "option_notes.h"
#include "texture_pack_menu.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "gxruntime/aurora_backend.h"
#include "save_state.h"
#include "stage_select_key.h"
#include "desktop_theme.h"
#include "status_line_overlay.h"
#include <aurora/gfx.h>

extern "C" {
// runtime/host/src/mouse_camera.h and game_options.h, declared here with plain
// types: those headers bring in the guest CPU's, which this file has no use for.
void bluewake_mouse_camera_configure(bool enabled, double sensitivity, bool invert_y);
void bluewake_mouse_camera_block(bool blocked);
void bluewake_pad_hold_until_released(void);
bool bluewake_mouse_camera_captured(void);
void bluewake_mouse_camera_buttons(const char* map);
void bluewake_haptics_reload(void);
void bluewake_mouse_camera_reload(void);
void bluewake_climb_reload(void);
// forest_water.h: BLUEWAKE_FOREST_WATER_KEEP_TREES and _30_MINUTES read again.
void bluewake_forest_water_reload(void);
// hd_look.h: BLUEWAKE_HD_LOOK read again.
void bluewake_hd_look_reload(void);
// dpad_menus.h: BLUEWAKE_DPAD_MENUS read again.
void bluewake_dpad_menus_reload(void);
void bluewake_haptics_block(bool blocked);
void bluewake_haptics_preview(void);
const char* bluewake_game_options_describe(uint32_t position, const char** title, bool* default_on, bool* on);
// climb.h: the stamina wheel's state for the HUD.
bool bluewake_climb_hud(float* fraction, bool* exhausted, float* x, float* y, float* aspect, float* alpha);
}

// Aurora's frame counters (lib/gfx/common.hpp, linked in statically), for the
// session log: every present, and the game's own frames.
namespace aurora::gfx {
float calculate_fps() noexcept;
float calculate_game_fps() noexcept;
}  // namespace aurora::gfx

namespace {

// The settings are the host's own variables (BLUEWAKE_*, DOL_*): the settings
// file sets them at launch, where the command line has not already, and the
// menu reads and writes them through bw_menu::get and put (menu_draw.h). Only
// what a variable cannot hold is kept here.
std::string g_data_dir, g_path;
std::vector<wchar_t> g_environment;  // as BlueWake was started, for Restart
BwSettingsLines g_other;  // the file's lines this build doesn't know, kept as they were
// Settings already set when BlueWake started (a launch option or a variable):
// they win for this session, and the file keeps its own value for them (here)
// unless the player changes them in the menu (g_edited).
std::map<std::string, std::string> g_launch;
std::set<std::string> g_edited;
std::map<std::string, std::string> g_launched;  // the file's settings as this session started

// The settings that take effect at the next launch, as chosen now: they go to
// the file, not to this session's variables, which the game, the mods and
// Aurora have already read.
std::string g_aspect = "4:3";
bool g_betterww;
std::vector<std::pair<std::string, bool>> g_options;  // Better Wind Waker's: name, on
std::vector<std::string> g_option_titles;
std::string g_texture_pack;  // the HD pack's folder, "" when HD textures are off
std::string g_last_pack;     // the folder to come back to when they go on again
// The window's size and place, which the file keeps under Windows-only keys.
int g_window_w, g_window_h, g_window_x = INT_MIN, g_window_y = INT_MIN;

bw_texture_pack::Counter g_pack_count; // textures in the chosen pack, counted once per menu opening
RestartRequest g_restart;
bool g_safe_mode;
bool g_menu_open;
bool g_stage_select_question; // the menu shows only the stage select's question
bool g_toggle_menu, g_toggle_fullscreen;  // from the hotkeys, done in the frame
bool g_dirty;
Uint64 g_dirty_at, g_first_frame_at;
bool g_pad_applied;
SDL_JoystickID g_pad_connection = 0;
float g_font_scale = 1.0f;  // the scale the UI font was drawn at (see load_font)

// What this menu's lists of choices offer (bw_menu::choices): what is shown,
// and what is stored in the variable and the file.
const char* const kScaleNames[] = {"The window's own pixels", "1x (640x480)", "2x (1280x960)",
                                   "3x (1920x1440)", "4x (2560x1920)"};
const char* const kScaleValues[] = {"0", "1", "2", "3", "4"};
const char* const kAnisotropyNames[] = {"The game's own", "2x anisotropic", "4x anisotropic", "8x anisotropic",
                                        "16x anisotropic"};
const char* const kAnisotropyValues[] = {"1", "2", "4", "8", "16"};

// --- the settings as variables ----------------------------------------------

std::string env(const char* key, const char* fallback = "") {
    const char* value = std::getenv(key);
    return value != nullptr ? value : fallback;
}

bool env_set(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && v[0] != '\0';
}

bool env_on(const char* key, bool fallback) {
    const char* value = std::getenv(key);
    if (value == nullptr || value[0] == '\0')
        return fallback;
    return value[0] != '0';
}

int env_int(const char* key, int fallback) {
    const char* value = std::getenv(key);
    return value != nullptr && value[0] != '\0' ? std::atoi(value) : fallback;
}

// A setting the player just changed: the variable for this session, and the
// file at the next save.
void set_env(const char* key, const std::string& value) {
    _putenv_s(key, value.c_str());
    g_edited.insert(key);
    g_dirty = true;
    g_dirty_at = SDL_GetTicks();
}

// A setting from the file or a default: it never counts as an edit.
void env_default(const char* name, const std::string& value) {
    if (!env_set(name))
        _putenv_s(name, value.c_str());
}

// --- the file ---------------------------------------------------------------

// Load\Textures\GZLE01, the pack's folder when none is chosen.
std::string default_pack_folder() { return g_data_dir + "Load\\Textures\\GZLE01"; }

// The settings file, in the desktop format (settings_file.h): KEY=VALUE lines
// with the host's own names, as the Mac and Linux menu writes them. Its lines
// become this session's variables at launch, and the menu writes them back
// from those variables. A file from before the menus were shared (aspect=,
// option.NAME=...) is read through settings_file.h's Settings, turned into
// those lines once, and copied to settings.ini.before-shared-menu first. If
// the copy fails it is left as it is, and still read the old way next time.
void save_file();
void capture_launch_choices();
std::string options_value();

// One line of the file into this session: its variable, unless the launch
// already chose that setting, and the Windows-only keys, which the launch
// decides for itself (the window's place, and the pack to come back to).
void read_line(const std::string& key, const std::string& value) {
    if (key == kBwWindowKey) {
        std::sscanf(value.c_str(), "%dx%d", &g_window_w, &g_window_h);
        return;
    }
    if (key == kBwWindowPositionKey) {
        std::sscanf(value.c_str(), "%d,%d", &g_window_x, &g_window_y);
        return;
    }
    if (key == kBwPackFolderKey) {
        g_last_pack = value;
        return;
    }
    // The pack the file names is also the one to come back to: a safe launch
    // turns HD textures off for this session, and the folder is kept.
    if (key == "DOL_AURORA_TEXTURE_PACK" && !value.empty())
        g_last_pack = value;
    const bool known = bw_settings::find(key.c_str()) != nullptr;
    const auto launched = g_launch.find(key);
    if (launched != g_launch.end())
        launched->second = value;  // for this session the launch wins; the file keeps this
    else if (known || std::getenv(key.c_str()) == nullptr)
        _putenv_s(key.c_str(), value.c_str());
    if (!known)
        g_other.emplace_back(key, value);
}

void load_file() {
    FILE* f = std::fopen(g_path.c_str(), "r");
    if (f == nullptr)
        return;
    BwSettingsLines lines;
    char line[4096];
    while (std::fgets(line, sizeof line, f) != nullptr) {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
            s.pop_back();
        const size_t eq = s.find('=');
        if (s.empty() || s[0] == '#' || eq == std::string::npos)
            continue;
        lines.emplace_back(s.substr(0, eq), s.substr(eq + 1));
    }
    std::fclose(f);
    Settings old;  // only the old file's shape, for the one-time conversion
    bool old_format = false;
    for (const auto& [k, v] : lines)
        if (k.rfind("BLUEWAKE_", 0) != 0 && k.rfind("DOL_", 0) != 0 && bw_settings_read_old(k, v, old))
            old_format = true;
    if (!old_format) {
        for (const auto& [k, v] : lines)
            read_line(k, v);
        std::fprintf(stderr, "[settings] %d settings from %s\n", static_cast<int>(lines.size()), g_path.c_str());
        return;
    }
    // An old file: its settings in the shared format, then the lines the old
    // reader didn't know (none, before the file was shared), kept as they were.
    Settings ignored;
    BwSettingsLines unknown;
    for (const auto& [k, v] : lines)
        if (!bw_settings_read_old(k, v, ignored))
            unknown.emplace_back(k, v);
    for (const auto& [k, v] : bw_settings_write_new(old, default_pack_folder()))
        read_line(k, v);
    for (const auto& [k, v] : unknown)
        read_line(k, v);
    const std::string backup = g_path + ".before-shared-menu";
    if (bw_atomic_copy_if_missing(g_path.c_str(), backup.c_str())) {
        std::fprintf(stderr, "[settings] the settings file is in the shared format now; the old one is %s\n",
                     backup.c_str());
        g_dirty = true;
        save_file();
    } else {
        std::fprintf(stderr, "[settings] kept the old settings file as it was (could not copy it to %s)\n",
                     backup.c_str());
    }
}

// What the file keeps for one setting: the menu's own copy for the next-launch
// choices the session has already read, otherwise this session's variable, and
// the file's own value for a setting the launch chose and nobody changed here.
std::string file_value(const char* key) {
    std::string value;
    if (std::strcmp(key, "BLUEWAKE_ASPECT") == 0)
        value = g_aspect;
    else if (std::strcmp(key, "BLUEWAKE_MODS") == 0)
        value = g_betterww ? "betterww" : "";
    else if (std::strcmp(key, "BLUEWAKE_OPTIONS") == 0)
        value = g_options.empty() ? env(key) : options_value();
    else if (std::strcmp(key, "DOL_AURORA_TEXTURE_PACK") == 0)
        value = g_texture_pack;
    else
        value = env(key);
    const auto launched = g_launch.find(key);
    if (launched != g_launch.end() && g_edited.count(key) == 0)
        value = launched->second;
    return value;
}

void save_file() {
    g_dirty_at = SDL_GetTicks(); // back off after failures rather than retry every frame
    // The aspect, the mods and the texture pack are written from the menu's
    // copies, read when it first opens: a key (F2, F9, F10) that saves before
    // then would otherwise write them empty.
    capture_launch_choices();
    char* pending = bw_atomic_path(g_path.c_str());
    FILE* f = pending != nullptr ? std::fopen(pending, "w") : nullptr;
    if (f == nullptr) { free(pending); return; }
    std::fprintf(f, "# BlueWake settings, written by the options menu (F1 or Esc in the game).\n"
                    "# KEY=VALUE, the host's settings; a launch option or a variable set before\n"
                    "# BlueWake starts wins for that session.\n");
    for (const bw_settings::Setting& s : bw_settings::kSettings)
        std::fprintf(f, "%s=%s\n", s.key, file_value(s.key).c_str());
    if (g_window_w > 0 && g_window_h > 0)
        std::fprintf(f, "%s=%dx%d\n", kBwWindowKey, g_window_w, g_window_h);
    if (g_window_x != INT_MIN && g_window_y != INT_MIN)
        std::fprintf(f, "%s=%d,%d\n", kBwWindowPositionKey, g_window_x, g_window_y);
    if (g_texture_pack.empty() && !g_last_pack.empty())
        std::fprintf(f, "%s=%s\n", kBwPackFolderKey, g_last_pack.c_str());
    for (const auto& [k, v] : g_other)
        std::fprintf(f, "%s=%s\n", k.c_str(), v.c_str());
    const bool ok = bw_atomic_finish_dirty(f, pending, g_path.c_str(), &g_dirty);
    free(pending);
    if (!ok) std::fprintf(stderr, "[settings] save failed; previous file kept, retry pending\n");
}

// --- the launch -------------------------------------------------------------

double aspect_ratio(const std::string& aspect) {
    return aspect == "16:9" ? 16.0 / 9.0 : aspect == "16:10" ? 1.6 : aspect == "21:9" ? 21.0 / 9.0 : 4.0 / 3.0;
}

// A window a good size for this screen: the tallest multiple of 240 lines that
// fits in 80 percent of the primary screen's work area, at the picture's shape.
void default_window(double ratio, int* w, int* h) {
    MONITORINFO info{};
    info.cbSize = sizeof info;
    int work_w = 1280, work_h = 1024;
    if (GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &info)) {
        work_w = info.rcWork.right - info.rcWork.left;
        work_h = info.rcWork.bottom - info.rcWork.top;
    }
    int height = std::max(480, static_cast<int>(work_h * 0.8) / 240 * 240);
    int width = static_cast<int>(std::lround(height * ratio));
    if (width > work_w * 9 / 10) {
        width = work_w * 9 / 10;
        height = static_cast<int>(width / ratio);
    }
    *w = width;
    *h = height;
}

std::string texture_folder() { return default_pack_folder(); }
// The folder the pack is loaded from: the one chosen, else texture_folder().
std::string texture_pack_folder() { return g_texture_pack.empty() ? texture_folder() : g_texture_pack; }

// --- the window -------------------------------------------------------------

SDL_Window* game_window() {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* window = windows != nullptr && count > 0 ? windows[0] : nullptr;
    SDL_free(windows);
    return window;
}

bool is_fullscreen(SDL_Window* w) { return (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN) != 0; }

// Fullscreen for this session only: whether the game starts in fullscreen is
// its own setting (fullscreen= in settings.ini), so trying fullscreen never
// changes how the game starts (#320).
void set_fullscreen(SDL_Window* w, bool on) {
    if (w == nullptr)
        return;
    SDL_SetWindowFullscreen(w, on);
    std::fprintf(stderr, "[windows] fullscreen %s\n", on ? "on" : "off");
}

// Whether a window whose client area starts at (x, y) has its title bar and
// its top left on a connected monitor. Asked at launch, before the window
// exists, so Win32's rather than SDL's; the manifest makes BlueWake per-monitor
// DPI aware, so these are the pixels SDL saved the position in.
bool on_a_monitor(int x, int y) {
    const POINT title{x + 60, y - 16};
    const POINT corner{x + 60, y + 60};
    return MonitorFromPoint(title, MONITOR_DEFAULTTONULL) != nullptr &&
           MonitorFromPoint(corner, MONITOR_DEFAULTTONULL) != nullptr;
}

// Remember the window's place and size as the player leaves them.
void track_window(SDL_Window* w) {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(w);
    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED))
        return;
    int x = 0, y = 0, width = 0, height = 0;
    if (!SDL_GetWindowPosition(w, &x, &y) || !SDL_GetWindowSize(w, &width, &height))
        return;
    static bool first = true;
    if (first) {
        first = false;  // where it opened, which the file already holds
        g_window_x = x;
        g_window_y = y;
        g_window_w = width;
        g_window_h = height;
        return;
    }
    if (x != g_window_x || y != g_window_y || width != g_window_w || height != g_window_h) {
        g_window_x = x;
        g_window_y = y;
        g_window_w = width;
        g_window_h = height;
        g_dirty = true;
        g_dirty_at = SDL_GetTicks();
    }
}

void reset_window(SDL_Window* w) {
    if (w == nullptr)
        return;
    if (is_fullscreen(w))
        set_fullscreen(w, false);
    int width = 0, height = 0;
    default_window(aspect_ratio(env("BLUEWAKE_ASPECT", "4:3")), &width, &height);
    SDL_SetWindowSize(w, width, height);
    SDL_SetWindowPosition(w, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

// --- settings that apply at once ------------------------------------------------

// A controller's camera stick, inverted as asked, as the iOS app does it
// (apple/ios/src/controller_apply.cpp). Only touched once the player inverts
// an axis, so a mapping set elsewhere is otherwise left alone.
void apply_controller() {
    BwButtonMap map;
    const bool remapped = bw_button_map_parse(env("BLUEWAKE_BUTTON_MAP"), &map);
    const bool ab = env_on("BLUEWAKE_PAD_SWAP_AB", false), xy = env_on("BLUEWAKE_PAD_SWAP_XY", false);
    const bool lr = env_on("BLUEWAKE_PAD_SWAP_LR", false);
    const bool ix = env_on("BLUEWAKE_STICK_CAMERA_INVERT_X", false);
    const bool iy = env_on("BLUEWAKE_STICK_CAMERA_INVERT_Y", false);
    if (!ix && !iy && !ab && !xy && !lr && !remapped && !g_pad_applied)
        return;
    if (PADGetIndexForPort(0) < 0)
        return;
    PADRestoreDefaultMapping(0);
    // A custom layout replaces the swaps; otherwise the swaps as before.
    if (remapped)
        bw_apply_button_map(0, map);
    else
        bw_apply_face_swaps(0, ab, xy);
    bw_apply_camera_axes(0, ix, iy);
    bw_apply_trigger_swap(0, lr);
    g_pad_applied = true;
}

// The keyboard's keys for the GameCube buttons (input_remap.h), applied once
// the pad's keyboard bindings exist and again whenever they change.
void apply_key_map() {
    static bool applied = false;
    static std::string last;
    const std::string text = env("BLUEWAKE_KEY_MAP");
    if (applied && text == last)
        return;
    BwKeyMap map;
    bw_key_map_parse(text, &map);
    if (bw_apply_key_map(0, map)) {
        applied = true;
        last = text;
    }
}

// Adapted from Elliott Tate's display-rate selection; resolve the current
// session so a command-line override never changes a saved preference.
float display_refresh(SDL_Window* window) {
    const SDL_DisplayMode* mode = window != nullptr ? SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window)) : nullptr;
    return mode != nullptr ? mode->refresh_rate : 0.f;
}

int smooth_requested() { return bw_smooth_requested(env("DOL_AURORA_FRAME_INTERP_STEPS", "1").c_str()); }

void apply_smooth_rate(SDL_Window* window) {
    aurora_set_frame_interp_steps(bw_smooth_steps(smooth_requested(), display_refresh(window)));
}

void apply_fps_position() {
    aurora_set_fps_overlay_position(
        static_cast<AuroraFpsOverlayPosition>(bw_fps_position(env("DOL_AURORA_FPS_POSITION").c_str())));
}

// Smooth Motion off, or on at a rate: the switch and its rate are one choice,
// so F10 still turns it on at the rate last chosen.
void set_smooth_motion(const char* steps) {
    if (steps != nullptr)
        set_env("DOL_AURORA_FRAME_INTERP_STEPS", steps);
    set_env("DOL_AURORA_FRAME_INTERP", steps != nullptr ? "1" : "0");
    aurora_set_frame_interpolation(steps != nullptr);
    apply_smooth_rate(game_window());
}

// Every setting the game and Aurora read while they run, from the variables:
// the first frame, after the launch has filled them in.
void apply_live() {
    aurora_set_frame_buffer_scale(static_cast<float>(env_int("DOL_AURORA_RENDER_SCALE", 0)));
    aurora_set_forced_anisotropy(static_cast<unsigned>(env_int("DOL_AURORA_FORCE_ANISO", 1)));
    apply_smooth_rate(game_window());
    aurora_set_frame_interpolation(env_on("DOL_AURORA_FRAME_INTERP", false));
    aurora_set_fps_overlay(env_on("DOL_AURORA_SHOW_FPS", false));
    apply_fps_position();
    aurora_set_pause_on_focus_lost(env_on("BLUEWAKE_PAUSE_UNFOCUSED", false));
    bluewake_mouse_camera_reload();  // the mouse, both sticks, gyro and the mouse's buttons
    apply_controller();
    bluewake_haptics_reload();
    bluewake_climb_reload();
    bluewake_forest_water_reload();
    bluewake_hd_look_reload();
    bluewake_dpad_menus_reload();
}

// Applied the moment a setting changes, from its variable; a setting that only
// takes effect at the next launch has no entry here (the table's next_launch
// draws its note instead).
void apply_now(const char* key) {
    const std::string value = env(key);
    if (std::strcmp(key, "BLUEWAKE_HD_LOOK") == 0)
        bluewake_hd_look_reload();
    else if (std::strcmp(key, "DOL_AURORA_SHOW_FPS") == 0)
        aurora_set_fps_overlay(value != "0");
    else if (std::strcmp(key, "DOL_AURORA_FPS_POSITION") == 0)
        apply_fps_position();
    else if (std::strcmp(key, "DOL_AURORA_RENDER_SCALE") == 0)
        aurora_set_frame_buffer_scale(static_cast<float>(std::atoi(value.c_str())));
    else if (std::strcmp(key, "DOL_AURORA_FORCE_ANISO") == 0)
        aurora_set_forced_anisotropy(static_cast<unsigned>(std::atoi(value.c_str())));
    else if (std::strcmp(key, "BLUEWAKE_PAUSE_UNFOCUSED") == 0)
        aurora_set_pause_on_focus_lost(value != "0");
    else if (std::strcmp(key, "BLUEWAKE_FOREST_WATER_KEEP_TREES") == 0 ||
             std::strcmp(key, "BLUEWAKE_FOREST_WATER_30_MINUTES") == 0)
        bluewake_forest_water_reload();
    else if (std::strcmp(key, "BLUEWAKE_FAST_FORWARD") == 0)
        // One switch for the player; the frames it skips move with it (next
        // launch, next_launch_here), and both keep working on their own for testing.
        set_env("BLUEWAKE_FADE_FRAMES", value != "0" ? "6" : "0");
    else if (std::strcmp(key, "BLUEWAKE_CLIMB") == 0 || std::strcmp(key, "BLUEWAKE_CLIMB_STAMINA") == 0)
        bluewake_climb_reload();
    else if (std::strcmp(key, "BLUEWAKE_DPAD_MENUS") == 0)
        bluewake_dpad_menus_reload();
    else if (std::strcmp(key, "BLUEWAKE_KEY_MAP") == 0)
        apply_key_map();
    // The pad's own mapping: the swaps, a custom layout, and the sticks below.
    else if (std::strncmp(key, "BLUEWAKE_PAD_SWAP_", 18) == 0 || std::strcmp(key, "BLUEWAKE_BUTTON_MAP") == 0)
        apply_controller();
    // Everything the mouse, the sticks and gyro aiming do is read back together.
    else if (std::strncmp(key, "BLUEWAKE_MOUSE_", 15) == 0 || std::strncmp(key, "BLUEWAKE_STICK_", 15) == 0 ||
             std::strncmp(key, "BLUEWAKE_GYRO_", 14) == 0 || std::strcmp(key, "BLUEWAKE_AIM_INVERT_Y") == 0) {
        bluewake_mouse_camera_reload();
        // An inverted camera stick reaches the game's own C-stick too.
        if (std::strncmp(key, "BLUEWAKE_STICK_CAMERA_INVERT_", 29) == 0)
            apply_controller();
    }
    else if (std::strncmp(key, "BLUEWAKE_HAPTICS", 16) == 0)
        bluewake_haptics_reload();
}

void set_menu_open(bool open) {
    if (open == g_menu_open)
        return;
    g_menu_open = open;
    if (open) {
        capture_launch_choices();  // the next-launch choices, as this session started
        g_pack_count.forget();
    }
    if (!open && g_stage_select_question) {
        g_stage_select_question = false;
        bluewake_stage_select_answer(false); // closed without an answer: cancel
    }
    PADBlockInput(open);
    // The menu's keys and clicks are not the game's.
    PADSetKeyboardActive(0, open ? FALSE : TRUE);
    bluewake_mouse_camera_block(open);
    bluewake_haptics_block(open);
    if (!open)
        bluewake_pad_hold_until_released();
    std::fprintf(stderr, "[windows] settings menu %s\n", open ? "open" : "closed");
    if (!open && g_dirty)
        save_file();
}

void open_folder(const std::string& path) {
    CreateDirectoryA(path.c_str(), nullptr);
    wchar_t wide[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide, MAX_PATH * 2) > 0)
        ShellExecuteW(nullptr, L"open", wide, nullptr, nullptr, SW_SHOWNORMAL);
}

void restart() {
    if (g_dirty) save_file();
    bool ok = g_restart.request(!g_dirty, [] {
        SDL_Event quit{};
        quit.type = SDL_EVENT_QUIT;
        return SDL_PushEvent(&quit);
    });
    if (!ok) std::fprintf(stderr, "[windows] restart refused: settings save or quit request failed\n");
}

// Start BlueWake again, with the command line and environment it began with,
// so the settings file decides what the new session is.
bool relaunch() {
    wchar_t exe[MAX_PATH * 2];
    if (GetModuleFileNameW(nullptr, exe, MAX_PATH * 2) == 0)
        return false;
    std::vector<wchar_t> command(GetCommandLineW(), GetCommandLineW() + wcslen(GetCommandLineW()) + 1);
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe, command.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT,
                        g_environment.empty() ? nullptr : g_environment.data(), nullptr, &startup, &process)) {
        std::fprintf(stderr, "[windows] restart failed (error %lu)\n", GetLastError());
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    std::fprintf(stderr, "[windows] restarting with the new settings\n");
    return true;
}

// --- the menu ---------------------------------------------------------------

// Whether this menu takes a setting at the next launch: the table's next-launch
// settings, and three that Mac and Linux apply at once but Windows has always
// taken at the next launch (Fast transitions, Quick doors, Jump & Sprint).
bool next_launch_here(const bw_settings::Setting& s) {
    static const char* const kWindowsNextLaunch[] = {"BLUEWAKE_FAST_FORWARD", "BLUEWAKE_QUICK_DOORS",
                                                      "BLUEWAKE_JUMP_BUTTON", "BLUEWAKE_SPRINT_SPEED"};
    if (s.next_launch)
        return true;
    for (const char* key : kWindowsNextLaunch)
        if (std::strcmp(s.key, key) == 0)
            return true;
    return false;
}

// Whether restarting would change anything: a setting that takes effect at the
// next launch is no longer what this session started with.
bool needs_restart() {
    for (const bw_settings::Setting& s : bw_settings::kSettings) {
        if (!next_launch_here(s))
            continue;
        const auto was = g_launched.find(s.key);
        if (was != g_launched.end() && was->second != file_value(s.key))
            return true;
    }
    return false;
}

// Sliders as on Mac and Linux: the label above, the slider the menu's width,
// and left and right move one while it's selected, a twentieth of its range a
// press, without A to grab it first (menu_shared.h). The shared drawing draws
// the same slider (menu_draw.h); this is for the rows that draw themselves.
bool slider_float(const char* label, float* value, float low, float high, const char* format,
                  ImGuiSliderFlags flags = 0) {
    ImGui::PushID(label);
    ImGui::TextWrapped("%s", label);
    ImGui::SetNextItemWidth(-1.0f);
    bool changed = ImGui::SliderFloat("##value", value, low, high, format, flags);
    if (const int step = bw_menu_slider_nudge()) {
        *value = std::clamp(*value + step * (high - low) / 20.0f, low, high);
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

void restart_note() {
    ImGui::SameLine();
    ImGui::TextDisabled("(next launch)");
}

// A setting the menu holds a copy of (the next-launch choices the session has
// already read): changed here, written to the file, not to this session.
void copy_changed(const char* key) {
    g_edited.insert(key);
    g_dirty = true;
    g_dirty_at = SDL_GetTicks();
}

// Better Wind Waker's options as the file keeps them: "none" and the ones on,
// as the Mac and Linux menu writes them.
std::string options_value() {
    std::string list = "none";
    for (const auto& [name, on] : g_options)
        if (on)
            list += "," + name;
    return list;
}

// The launch-time choices (this session's variables, and the options the game
// started with), read the first time the menu opens or the file is saved.
// Later openings keep what the player chose since: reading them again would
// show, and then save, the launch values over the player's changes.
void capture_launch_choices() {
    static bool captured = false;
    if (!captured) {
        captured = true;
        g_aspect = env("BLUEWAKE_ASPECT", "4:3");
        g_betterww = env("BLUEWAKE_MODS").find("betterww") != std::string::npos;
        g_texture_pack = env("DOL_AURORA_TEXTURE_PACK");
        if (!g_texture_pack.empty())
            g_last_pack = g_texture_pack;
        g_launched["BLUEWAKE_ASPECT"] = g_aspect;
        g_launched["BLUEWAKE_MODS"] = g_betterww ? "betterww" : "";
        g_launched["DOL_AURORA_TEXTURE_PACK"] = g_texture_pack;
    }
    // Better Wind Waker's options are listed once the game has started.
    if (!g_options.empty())
        return;
    g_option_titles.clear();
    for (uint32_t i = 0;; ++i) {
        const char* title = nullptr;
        bool default_on = false, on = false;
        const char* name = bluewake_game_options_describe(i, &title, &default_on, &on);
        if (name == nullptr)
            break;
        g_options.emplace_back(name, g_betterww ? on : default_on);
        g_option_titles.emplace_back(title != nullptr ? title : name);
    }
    if (!g_options.empty())
        g_launched["BLUEWAKE_OPTIONS"] = options_value();
}

// The pack's folder, "" to turn HD textures off; the folder it was on with is
// kept for when they come back (BLUEWAKE_TEXTURE_PACK_FOLDER in the file).
void set_texture_pack(const std::string& folder) {
    if (!g_texture_pack.empty())
        g_last_pack = g_texture_pack;
    g_texture_pack = folder;
    copy_changed("DOL_AURORA_TEXTURE_PACK");
}

// Wind Waker HD textures: the same section, in the same order, as the Mac and
// Linux Mods tab and the iPhone and iPad Mods menu (texture_pack_menu.h). They
// are on while a folder is chosen, as in the settings file.
void hd_textures_section() {
    std::string picked;
    if (bw_texture_pack::take_folder(picked))
        set_texture_pack(picked);
    ImGui::SeparatorText("Wind Waker HD textures");
    bool on = !g_texture_pack.empty();
    if (bw_menu_checkbox("HD textures", &on))
        set_texture_pack(on ? (!g_last_pack.empty() ? g_last_pack : texture_folder()) : "");
    restart_note();
    on = !g_texture_pack.empty();
    const std::string folder = texture_pack_folder();
    const size_t textures = g_pack_count.get(folder);
    {
        const bw_texture_pack::PackState state = bw_texture_pack::state_of(textures, on);
        std::string line;
        if (!on)
            line = "Off. A Dolphin-format pack for GZLE01 (a folder of .png or .dds files) replaces the game's textures.";
        else if (textures == 0)
            line = "No .png or .dds textures found in the chosen folder.";
        else
            line = std::to_string(textures) + " textures in " + folder;
        ImGui::TextColored(bw_texture_pack::state_color(state), "%s", line.c_str());
    }
    if (ImGui::SmallButton("Choose texture pack folder..."))
        bw_texture_pack::choose_folder(nullptr, folder);
    ImGui::SameLine();
    if (ImGui::SmallButton("Open the texture folder"))
        open_folder(folder);
    if (on && g_texture_pack != texture_folder()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Use Load\\Textures\\GZLE01 instead"))
            set_texture_pack(texture_folder());
    }
    const std::string error = bw_texture_pack::picker_error();
    if (!error.empty())
        ImGui::TextWrapped("The folder picker did not open (%s). Put the pack in the texture folder instead.",
                           error.c_str());
    ImGui::Spacing();
}

// A Custom row draws itself: the switch that needs a button beside it, the two
// settings that are one choice, the tables of keys and buttons, and the
// next-launch choices this menu keeps a copy of.
bool draw_custom(const bw_settings::Setting& s) {
    if (std::strcmp(s.key, "DOL_AURORA_FULLSCREEN") == 0) {
        // An action for this session, not a setting: a button above the switch.
        SDL_Window* w = game_window();
        const bool full = w != nullptr && is_fullscreen(w);
        if (ImGui::Button(full ? "Leave fullscreen   (F11 or Alt+Enter)" : "Fullscreen   (F11 or Alt+Enter)"))
            set_fullscreen(w, !full);
        bool start_fullscreen = false;
        bw_menu::draw_switch(s, &start_fullscreen);
        restart_note();
        return true;
    }
    if (std::strcmp(s.key, "DOL_AURORA_FRAME_INTERP") == 0) {
        // The switch and its rate are one dropdown, as on Mac and Linux.
        static const char* const kSmooth[] = {"Off (30, the game's)", "60 frames a second",
                                              "120 frames a second (120 Hz displays)",
                                              "Match the display (up to 240)"};
        const int requested = smooth_requested();
        int smooth = !env_on(s.key, false) ? 0 : requested == -1 ? 3 : requested >= 3 ? 2 : 1;
        if (bw_menu_combo(s.label, &smooth, kSmooth, 4))
            set_smooth_motion(smooth == 0 ? nullptr : smooth == 3 ? "display" : smooth == 2 ? "3" : "1");
        // What that comes to on this display, as the Windows menu has shown it.
        const float refresh = display_refresh(game_window());
        ImGui::TextDisabled("    Target: %d FPS on this %.0f Hz display; pacing may reduce it.",
                            env_on(s.key, false) ? (bw_smooth_steps(smooth_requested(), refresh) + 1) * 30 : 30,
                            refresh);
        if (s.note != nullptr)
            ImGui::TextDisabled("%s", s.note);
        return true;
    }
    if (std::strcmp(s.key, "BLUEWAKE_BUTTON_MAP") == 0) {
        if (!ImGui::CollapsingHeader(s.label))
            return true;
        ImGui::TextWrapped("Choose which controller button presses each GameCube button. Picking one that is "
                           "already used swaps the two.");
        BwButtonMap map;
        bw_button_map_parse(env(s.key), &map);
        if (bw_button_map_ui(&map)) {
            set_env(s.key, bw_button_map_format(map));
            apply_controller();
        }
        return true;
    }
    if (std::strcmp(s.key, "BLUEWAKE_KEY_MAP") == 0) {
        if (!ImGui::CollapsingHeader(s.label))
            return true;
        ImGui::TextWrapped("Choose the key for each GameCube button. Picking one that is already used swaps the two.");
        BwKeyMap keys;
        bw_key_map_parse(env(s.key), &keys);
        if (bw_key_map_ui(&keys)) {
            set_env(s.key, bw_key_map_format(keys));
            apply_key_map();
        }
        return true;
    }
    if (std::strcmp(s.key, "BLUEWAKE_MOUSE_BUTTONS") == 0) {
        if (!ImGui::CollapsingHeader(s.label))
            return true;
        ImGui::TextWrapped("What each mouse button presses while the mouse is the camera. The first left click "
                           "only hands the mouse to the game.");
        BwMouseMap buttons;
        bw_mouse_map_parse(env(s.key).c_str(), &buttons);
        if (bw_mouse_map_ui(&buttons)) {
            char text[64];
            bw_mouse_map_format(&buttons, text, sizeof text);
            set_env(s.key, text);
            bluewake_mouse_camera_reload();
        }
        return true;
    }
    if (std::strcmp(s.key, "BLUEWAKE_GYRO_AIM_SPEED") == 0) {
        // The slider, and under it what the right stick does, which depends on
        // the direct camera above rather than on this setting.
        if (bw_menu::draw_slider(s))
            bluewake_mouse_camera_reload();
        ImGui::PushTextWrapPos();
        ImGui::TextDisabled(env_on("BLUEWAKE_STICK_CAMERA", true)
                                ? "Click the right stick for first person. When controller zoom is on, hold it and "
                                  "move the stick up/down. In the telescope and Picto Box, the left stick or D-pad zooms."
                                : "The game's right stick: its left and right follow Better Wind Waker's "
                                  "\"Invert camera\" (Gameplay).");
        ImGui::PopTextWrapPos();
        return true;
    }
    if (std::strcmp(s.key, "BLUEWAKE_HAPTICS") == 0) {
        // The three vibration settings move together: the kind, how strong, and
        // the triggers, which only the shaped kind uses.
        static const char* const kHaptics[] = {"Off", "Classic (the game's own on and off)",
                                               "Enhanced (shaped, with the triggers)"};
        const std::string mode = env(s.key, s.fallback);
        int haptics = mode.empty() || mode[0] == 'e' || mode[0] == '1' ? 2 : mode[0] == 'c' ? 1 : 0;
        if (bw_menu_combo(s.label, &haptics, kHaptics, 3)) {
            set_env(s.key, haptics == 0 ? "off" : haptics == 1 ? "classic" : "enhanced");
            bluewake_haptics_reload();
        }
        ImGui::BeginDisabled(haptics != 2);
        float strength = static_cast<float>(std::clamp(env_int("BLUEWAKE_HAPTICS_STRENGTH", 80), 0, 100));
        if (slider_float(bw_menu::setting("BLUEWAKE_HAPTICS_STRENGTH").label, &strength, 0.f, 100.f, "%.0f%%")) {
            set_env("BLUEWAKE_HAPTICS_STRENGTH", std::to_string(static_cast<int>(std::lround(strength))));
            bluewake_haptics_reload();
            bluewake_haptics_preview();  // a buzz at the new strength
        }
        bool triggers = false;
        if (bw_menu::draw_switch(bw_menu::setting("BLUEWAKE_HAPTICS_TRIGGERS"), &triggers))
            bluewake_haptics_reload();
        ImGui::EndDisabled();
        ImGui::TextDisabled(haptics == 2 ? "Hits, falls, explosions and quakes as the game times them, shaped by "
                                           "their strength."
                            : haptics == 1 ? "The motor on and off, as a GameCube controller's."
                                           : "No vibration. (The game's own Vibration option turns it off too.)");
        return true;
    }
    if (std::strcmp(s.key, "DOL_AURORA_TEXTURE_PACK") == 0) {
        hd_textures_section();
        ImGui::Spacing();
        return true;
    }
    if (std::strcmp(s.key, "BLUEWAKE_ASPECT") == 0) {
        // The chosen shape is held in g_aspect: the picture, the mods and the
        // Display tab's "keep the shape" all follow it.
        static const char* const kAspects[] = {"Off (4:3, the game's)", "16:10", "16:9", "21:9"};
        static const char* const kAspectValues[] = {"4:3", "16:10", "16:9", "21:9"};
        int aspect = 0;
        for (int i = 0; i < 4; ++i)
            if (g_aspect == kAspectValues[i])
                aspect = i;
        if (bw_menu_combo(s.label, &aspect, kAspects, 4)) {
            g_aspect = kAspectValues[aspect];
            copy_changed(s.key);
        }
        restart_note();
        ImGui::TextDisabled("Widescreen widens the camera and moves the HUD to the edges.");
        ImGui::Spacing();
        return true;
    }
    if (std::strcmp(s.key, "BLUEWAKE_MODS") == 0) {
        // Better Wind Waker, and its own options once the game has read them.
        if (bw_menu_checkbox(s.label, &g_betterww))
            copy_changed(s.key);
        restart_note();
        if (g_options.empty()) {
            ImGui::TextDisabled("This build has no Better Wind Waker options (build with mods).");
            return true;
        }
        ImGui::TextDisabled("Wind Waker HD's changes. Each can be turned on or off:");
        ImGui::BeginDisabled(!g_betterww);
        ImGui::Indent();
        for (size_t i = 0; i < g_options.size(); ++i) {
            bool on = g_options[i].second;
            ImGui::PushID(g_options[i].first.c_str());
            if (bw_menu_checkbox(g_option_titles[i].c_str(), &on)) {
                g_options[i].second = on;
                copy_changed("BLUEWAKE_OPTIONS");
            }
            if (const char* note = bw_option_note(g_options[i].first.c_str())) {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
                ImGui::TextDisabled("%s", note);
                ImGui::PopTextWrapPos();
            }
            ImGui::PopID();
        }
        ImGui::Unindent();
        ImGui::EndDisabled();
        return true;
    }
    if (std::strcmp(s.key, "BLUEWAKE_DSP_MODE") == 0) {
        // Two ways to make the sound, as a pair of choices rather than a switch.
        int exact = env(s.key, s.fallback) == "lle" ? 1 : 0;
        bool picked = ImGui::RadioButton("Fast (Dolphin's high-level Zelda sound)", &exact, 0);
        picked |= ImGui::RadioButton("Exact (the sound chip's own program; slower)", &exact, 1);
        if (picked)
            set_env(s.key, exact ? "lle" : "hle");
        restart_note();
        return true;
    }
    if (std::strcmp(s.key, "BLUEWAKE_STAGE_SELECT_TOGGLE") == 0) {
        bool stage_select = false;
        bw_menu::draw_switch(s, &stage_select);
        restart_note();
        if (s.note != nullptr)
            ImGui::TextWrapped("%s", s.note);
        bw_stage_select_save_warning();
        if (bw_stage_select_button())
            set_menu_open(false);
        return true;
    }
    if (std::strcmp(s.key, "BLUEWAKE_JUMP_BUTTON") == 0) {
        // Jump & Sprint, one switch as on the iPad and Mac, with the sprint's
        // speed under it, both for the next launch here (next_launch_here). The
        // chosen speed is kept apart
        // (BLUEWAKE_SPRINT_CHOICE) so turning it off (BLUEWAKE_SPRINT_SPEED 1)
        // and on again keeps it.
        const float current = static_cast<float>(std::atof(env("BLUEWAKE_SPRINT_SPEED", "1").c_str()));
        bool extras = env_on(s.key, false) || current > 1.01f;
        float choice = static_cast<float>(std::atof(env("BLUEWAKE_SPRINT_CHOICE", "0").c_str()));
        if (choice < 1.1f || choice > 2.f)
            choice = current > 1.01f && current <= 2.f ? std::max(current, 1.1f) : 1.5f;
        const auto apply_sprint = [&]() {
            char text[16];
            std::snprintf(text, sizeof text, "%.2f", choice);
            set_env("BLUEWAKE_SPRINT_CHOICE", text);
            set_env("BLUEWAKE_SPRINT_SPEED", extras ? text : "1");
            set_env(s.key, extras ? "1" : "0");
        };
        if (bw_menu_checkbox(s.label, &extras))
            apply_sprint();
        restart_note();
        ImGui::BeginDisabled(!extras);
        if (slider_float(bw_menu::setting("BLUEWAKE_SPRINT_CHOICE").label, &choice, 1.1f, 2.f, "%.2fx"))
            apply_sprint();
        ImGui::EndDisabled();
        return true;
    }
    if (std::strcmp(s.key, "DOL_AURORA_ASPECT_FIT") == 0) {
        // When the window isn't the picture's shape: black bars, or the picture
        // stretched to fill it. A widened picture (16:10, 16:9, 21:9) always
        // keeps its shape, so the choice is only for 4:3.
        if (g_aspect != "4:3") {
            ImGui::TextDisabled("Widened pictures always keep their shape (black bars rather than stretching).");
            return true;
        }
        bool keep_shape = false;
        bw_menu::draw_switch(s, &keep_shape);
        restart_note();
        return true;
    }
    return false;
}

}  // namespace

// What the shared drawing (menu_draw.h) asks of this menu: the settings are the
// host's own variables, so reading and writing a setting is reading and writing
// one of those.
namespace bw_menu {
std::string get(const char* key, const char* fallback) { return env(key, fallback); }
void put(const char* key, const std::string& value) { set_env(key, value); }
void next_launch_note(const bw_settings::Setting& s) {
    if (next_launch_here(s))
        restart_note();
}
bool custom(const bw_settings::Setting& s) { return draw_custom(s); }
bool choices(const char* key, const char* const** shown, const char* const** stored, int* count) {
    if (std::strcmp(key, "DOL_AURORA_RENDER_SCALE") == 0) {
        *shown = kScaleNames;
        *stored = kScaleValues;
        *count = IM_ARRAYSIZE(kScaleNames);
        return true;
    }
    if (std::strcmp(key, "DOL_AURORA_FORCE_ANISO") == 0) {
        *shown = kAnisotropyNames;
        *stored = kAnisotropyValues;
        *count = IM_ARRAYSIZE(kAnisotropyNames);
        return true;
    }
    if (std::strcmp(key, "DOL_AURORA_FPS_POSITION") == 0) {
        *shown = kBwFpsPositionNames;
        *stored = kBwFpsPositionValues;
        *count = BW_FPS_POSITIONS;
        return true;
    }
    return false;
}
// put has set the variable and marked the file dirty; this hands the new value
// to the running game. The file is written a second later, and when the menu
// closes.
void changed(const bw_settings::Setting& s) { apply_now(s.key); }
}  // namespace bw_menu

namespace {

// Each tab is settings_table.h's rows for it, in the table's order (menu_draw.h
// draws them). What is left in a tab is what is not a setting: an action, or
// something to read.
void tab_display(SDL_Window* w) {
    bw_menu::draw_tab(bw_settings::Tab::Display);
    ImGui::Spacing();
    if (ImGui::Button("Reset the window"))
        reset_window(w);
    ImGui::SameLine();
    ImGui::TextDisabled("Out of fullscreen, at its starting size, in the middle of the screen.");
}

void tab_gameplay() { bw_menu::draw_tab(bw_settings::Tab::Gameplay); }

// Mods: HD textures first, then widescreen and Better Wind Waker, as in the Mac
// and Linux Mods tab and the iPhone and iPad Mods menu.
void tab_mods() { bw_menu::draw_tab(bw_settings::Tab::Mods); }

void tab_controls() {
    bw_menu::draw_tab(bw_settings::Tab::Controls);
    // The keys as they are now (a remapped key shows its new name), in the same
    // table as on Mac and Linux; then the mouse and the controller.
    ImGui::SeparatorText("Keyboard");
    BwKeyMap keys;
    bw_key_map_parse(env("BLUEWAKE_KEY_MAP"), &keys);
    bw_keyboard_table(keys);
    ImGui::TextWrapped("Mouse: click the game, then move it to turn the camera and aim; left click is A and the "
                       "wheel zooms. Esc gives the mouse back, and Esc again opens this menu.");
    ImGui::SeparatorText("Controller");
    bw_controller_table();
    ImGui::TextDisabled("Game controllers work as they are plugged in (Xbox, PlayStation, Switch Pro, ...).");
}

void tab_game() {
    bw_menu::draw_tab(bw_settings::Tab::SoundFiles);
    ImGui::Spacing();
    ImGui::SeparatorText("Your files");
    ImGui::TextDisabled("Saves, settings and logs are kept apart from the build:");
    ImGui::TextUnformatted(g_data_dir.c_str());
    if (ImGui::Button("Open that folder"))
        open_folder(g_data_dir);
    ImGui::SameLine();
    if (ImGui::Button("Open the session logs"))
        open_folder(g_data_dir + "logs");
}

// The UI's size for this window: the display's scale (Windows' own, or more
// on a big screen at 100 percent), smaller when the window is too small for
// the menu. No larger than the font was drawn at.
float ui_scale(SDL_Window* w) {
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    const float display = w != nullptr ? SDL_GetWindowDisplayScale(w) : 1.0f;
    const float wanted = std::max({1.0f, display, size.y / 900.0f});
    const float fits = std::min(size.x / 660.0f, size.y / 560.0f);
    return std::clamp(std::min(wanted, fits), 0.75f, g_font_scale);
}

// On the first frame: a clear font drawn at the screen's scale, the default
// for everything ImGui draws from the next frame on (the menu and Aurora's
// frame rate). ImGui's own is a 13-pixel bitmap font, tiny on a 4K screen and
// blurred when scaled.
//
// The font has an atlas of its own, handed to Aurora as an ImGui texture
// (aurora_imgui_add_texture copies it on the render worker), so it can be
// made inside the frame. Adding it to ImGui's atlas instead would have the
// WebGPU backend rebuild and upload that from the main thread while the
// render worker submits: Dawn's device is not thread-safe here, and the bigger
// upload crashed early frames.
void load_font(SDL_Window* w) {
    float scale = std::max(1.0f, SDL_GetWindowDisplayScale(w));
    if (const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(w)))
        scale = std::max(scale, mode->h / 900.0f);
    scale = std::min(scale, 4.0f);
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2010, 0x205E, 0x2190, 0x2193, 0};
    auto* atlas = new ImFontAtlas();  // for the life of the process
    atlas->Flags |= ImFontAtlasFlags_NoMouseCursors;
    ImFont* font = nullptr;
    char fonts[MAX_PATH];
    const UINT length = GetWindowsDirectoryA(fonts, MAX_PATH);
    if (length > 0 && length < MAX_PATH - 32) {
        const std::string path = std::string(fonts) + "\\Fonts\\segoeui.ttf";
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            font = atlas->AddFontFromFileTTF(path.c_str(), std::round(15.0f * scale), nullptr, ranges);
    }
    if (font == nullptr) {
        // ImGui's own, at a whole multiple so its pixels stay square.
        scale = std::max(1.0f, std::round(scale));
        ImFontConfig pixel;
        pixel.SizePixels = 13.0f * scale;
        font = atlas->AddFontDefault(&pixel);
    }
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    atlas->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        std::fprintf(stderr, "[windows] the UI font did not build; keeping ImGui's\n");
        return;
    }
    atlas->SetTexID(aurora_imgui_add_texture(static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixels));
    atlas->ClearTexData();
    ImGui::GetIO().FontDefault = font;
    ImGui::GetStyle().ScaleAllSizes(scale);
    g_font_scale = scale;
    std::fprintf(stderr, "[windows] UI scale %.2f (font atlas %dx%d)\n", scale, width, height);
}

void draw_menu(SDL_Window* w) {
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // The controller, read here and handed to ImGui as keys (menu_shared.h).
    const BwMenuPad pad = bw_menu_controller(env("BLUEWAKE_BUTTON_MAP"), env_on("BLUEWAKE_PAD_SWAP_AB", false));
    const float scale = ui_scale(w);
    bluewake_ui::begin_theme(scale);
    if (g_stage_select_question) {
        if (pad.back) {  // B cancels the question, as Esc does
            io.AddKeyEvent(ImGuiKey_Escape, true);
            io.AddKeyEvent(ImGuiKey_Escape, false);
        }
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.94f);
        int answer = 0;
        if (ImGui::Begin("Developers' stage select", nullptr,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetWindowFontScale(scale / g_font_scale);
            answer = bw_stage_select_question();
        }
        ImGui::End();
        bluewake_ui::end_theme();
        if (answer != 0)
            set_menu_open(false);
        return;
    }
    // A size of its own rather than its contents', so each tab's body scrolls
    // inside it and the footer stays put, as in the Mac and Linux menu: it opens
    // in the middle, and can be moved and resized, but only inside the game's window.
    const ImVec2 display = io.DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(display.x - 24.f, 900.f * scale),
                                    std::min(display.y - 24.f, 720.f * scale)), ImGuiCond_Appearing);
    // Resizing stays between a size that still holds the tabs, hint and buttons
    // and the edges of the game's window, less a margin, from where the menu is.
    const ImVec2 largest(std::max(1.f, display.x - 24.f), std::max(1.f, display.y - 24.f));
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(std::min(460.f * scale, largest.x), std::min(320.f * scale, largest.y)), largest,
        [](ImGuiSizeCallbackData* data) {
            // Growing stops at the edge; a menu already past it (moved there) isn't shrunk.
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            const auto limit = [](float desired, float current, float room) {
                return desired <= room ? desired : std::max(room, std::min(current, desired));
            };
            data->DesiredSize.x = limit(data->DesiredSize.x, data->CurrentSize.x, screen.x - 12.f - data->Pos.x);
            data->DesiredSize.y = limit(data->DesiredSize.y, data->CurrentSize.y, screen.y - 12.f - data->Pos.y);
        });
    ImGui::SetNextWindowBgAlpha(0.94f);
    // Dragged, or resized from the top or left, it stays inside the game's
    // window. ImGui moves a dragged window as the frame starts, so this has to
    // come before Begin draws it there; if it is ever bigger than the game's
    // window, the top left (the tabs) stays in view.
    if (const ImGuiWindow* window = ImGui::FindWindowByName("BlueWake settings")) {
        const ImVec2 kept(std::max(12.f, std::min(window->Pos.x, display.x - 12.f - window->Size.x)),
                          std::max(12.f, std::min(window->Pos.y, display.y - 12.f - window->Size.y)));
        if (kept.x != window->Pos.x || kept.y != window->Pos.y)
            ImGui::SetNextWindowPos(kept);
    }
    // A click on the game or outside the menu leaves no window focused, and
    // then the controller has nothing to move through: give the menu focus back.
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
        ImGui::SetNextWindowFocus();
    bool open = true;
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("BlueWake settings", &open, flags)) {
        ImGui::SetWindowFontScale(scale / g_font_scale);
        // Esc normally closes it in the keyboard hook (bw_settings_key), which
        // keeps the key from SDL; one that reaches ImGui instead closes it too.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            open = false;
        // The bumpers switch tabs. Each tab's body scrolls on its own but is
        // flattened for navigation, so the D-pad moves from the tabs through the
        // options to the buttons below without A to enter it and B to leave it.
        static const char* const kTabs[] = {"Display", "Gameplay", "Mods", "Controls", "Sound and files"};
        constexpr int kTabCount = IM_ARRAYSIZE(kTabs);
        static int shown = 0;
        int request = -1;
        if (pad.tab_step != 0)
            request = (shown + kTabCount + pad.tab_step) % kTabCount;
        // The body takes what the footer leaves (menu_shared.h), as on Mac and Linux.
        const float footer = bw_menu_footer_height(needs_restart());
        if (ImGui::BeginTabBar("tabs")) {
            // No title bar, so the mouse's close button sits at the end of the tabs.
            if (ImGui::TabItemButton("x", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
                open = false;
            for (int i = 0; i < kTabCount; ++i) {
                if (!ImGui::BeginTabItem(kTabs[i], nullptr, i == request ? ImGuiTabItemFlags_SetSelected : 0))
                    continue;
                shown = i;
                ImGui::PushID(i);
                ImGui::BeginChild("##body", ImVec2(0, -footer), ImGuiChildFlags_NavFlattened);
                ImGui::PushTextWrapPos(0.f); // text wraps at the menu's edge
                switch (i) {
                case 0: tab_display(w); break;
                case 1: tab_gameplay(); break;
                case 2: tab_mods(); break;
                case 3: tab_controls(); break;
                default: tab_game(); break;
                }
                ImGui::PopTextWrapPos();
                bw_menu_stick_scroll(pad.scroll);
                ImGui::EndChild();
                ImGui::PopID();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        const bool restart_note = needs_restart();
        bw_menu_footer_note(restart_note);
        if (restart_note) {
            ImGui::SameLine();
            if (ImGui::Button("Restart now"))
                restart();
        }
        switch (bw_menu_footer_buttons(g_path.c_str())) {
        case BW_MENU_RESUME:
            open = false;
            break;
        case BW_MENU_SAVE_STATE:
            bluewake_save_state_hotkey(false);
            open = false;
            break;
        case BW_MENU_LOAD_STATE:
            bluewake_save_state_hotkey(true);
            open = false;
            break;
        case BW_MENU_QUIT: {
            open = false;
            if (g_dirty)
                save_file();
            SDL_Event quit{};
            quit.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quit);
            break;
        }
        case BW_MENU_NONE:
            break;
        }
        // The game's B ends an edit, closes a dropdown, or closes the menu (menu_shared.h).
        if (bw_menu_back(pad.back))
            open = false;
    }
    ImGui::End();
    bluewake_ui::end_theme();
    if (!open)
        set_menu_open(false);
}

// The climbing stamina wheel (climb.c), beside Link in the game's picture: the
// picture is the window's middle at the game's shape, or the whole window when
// "keep the picture's shape" is off (DOL_AURORA_ASPECT_FIT=0, set at launch).
void draw_climb_wheel() {
    float fraction, x, y, aspect, alpha;
    bool exhausted;
    if (!bluewake_climb_hud(&fraction, &exhausted, &x, &y, &aspect, &alpha))
        return;
    static const bool fit = [] {
        const char* v = std::getenv("DOL_AURORA_ASPECT_FIT");
        return v == nullptr || v[0] != '0';
    }();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    float w = display.x, h = display.y, x0 = 0.f, y0 = 0.f;
    if (h <= 0.f || aspect <= 0.f)
        return;
    if (fit && w / h > aspect) {
        w = h * aspect;
        x0 = (display.x - w) * 0.5f;
    } else if (fit) {
        h = w / aspect;
        y0 = (display.y - h) * 0.5f;
    }
    const float radius = h * 0.03f, thick = radius * 0.45f, pi = 3.14159265f;
    const ImVec2 center(x0 + x * w + radius * 2.4f, y0 + y * h - radius * 0.6f);
    ImDrawList* list = ImGui::GetForegroundDrawList();
    const auto a = [alpha](float v) { return static_cast<int>(v * alpha); };
    list->PathArcTo(center, radius, 0.f, 2.f * pi, 48);
    list->PathStroke(IM_COL32(20, 30, 20, a(150.f)), 0, thick + 3.f);
    if (fraction <= 0.002f)
        return;
    ImU32 color = IM_COL32(120, 230, 90, a(245.f));  // green
    if (exhausted) {
        const float pulse = 0.65f + 0.35f * std::sin(static_cast<float>(ImGui::GetTime()) * 8.f);
        color = IM_COL32(235, 70, 50, a(245.f * pulse));  // refilling after running out
    } else if (fraction < 0.25f) {
        color = IM_COL32(245, 190, 60, a(245.f));  // nearly out
    }
    list->PathArcTo(center, radius, -0.5f * pi, -0.5f * pi + 2.f * pi * fraction, 48);
    list->PathStroke(color, 0, thick);
}

// Compile shaders before playing: the game is held at its first present
// (dol_aurora_set_hold, the picture redrawn under this overlay) until every
// pipeline the cache queued at start is compiled, or two minutes have passed.
// They compile on several threads (Aurora's pipeline cache), and while the
// game is held it asks for nothing new. Otherwise they compile while the game
// runs, and a draw whose pipeline is not ready yet is left out of its frame.
bool g_shader_wait;
Uint64 g_shader_wait_since;
uint32_t g_shader_wait_first;

bool shader_wait_hold(void*) {
    if (!g_shader_wait)
        return false;
    const AuroraStats* stats = aurora_get_stats();
    const uint32_t left = stats != nullptr ? stats->queuedPipelines : 0u;
    if (g_shader_wait_since == 0) {
        g_shader_wait_since = SDL_GetTicks();
        g_shader_wait_first = left;
    }
    const Uint64 waited = SDL_GetTicks() - g_shader_wait_since;
    if (left == 0u || waited > 120000) {
        g_shader_wait = false;
        std::fprintf(stderr, "[windows] shaders compiled before play: %u in %.1f s%s\n", g_shader_wait_first,
                     waited / 1000.0, left != 0u ? " (stopped waiting)" : "");
        return false;
    }
    return true;
}

void draw_shader_wait(SDL_Window* w) {
    if (!g_shader_wait)
        return;
    const AuroraStats* stats = aurora_get_stats();
    const uint32_t left = stats != nullptr ? stats->queuedPipelines : 0u;
    ImGuiIO& io = ImGui::GetIO();
    const float scale = ui_scale(w);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.75f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##bluewake-shaders", nullptr, flags)) {
        ImGui::SetWindowFontScale(scale / g_font_scale);
        ImGui::Text("Compiling shaders: %u to go", left);
        if (g_shader_wait_first > 0u)
            ImGui::ProgressBar(1.f - static_cast<float>(left) / static_cast<float>(g_shader_wait_first),
                               ImVec2(260.f * scale, 0.f), "");
    }
    ImGui::End();
}

// For the first few seconds, where the settings and fullscreen are.
void draw_hint(SDL_Window* w) {
    const Uint64 shown = SDL_GetTicks() - g_first_frame_at;
    if (shown > 7000 || g_menu_open)
        return;
    ImGuiIO& io = ImGui::GetIO();
    const float scale = ui_scale(w);
    ImGui::SetNextWindowPos(ImVec2(12 * scale, io.DisplaySize.y - 12 * scale), ImGuiCond_Always, ImVec2(0, 1));
    ImGui::SetNextWindowBgAlpha(shown > 6000 ? 0.6f * (7000 - shown) / 1000.0f : 0.6f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##bluewake-hint", nullptr, flags)) {
        ImGui::SetWindowFontScale(scale / g_font_scale);
        ImGui::TextUnformatted("F1 settings   F11 fullscreen   F9 frame rate   Click the game to steer the camera");
    }
    ImGui::End();
}

// Every presented frame, on the main thread, inside Aurora's frame.
void frame(void*) {
    SDL_Window* w = game_window();
    if (w == nullptr)
        return;
    if (g_first_frame_at == 0) {
        g_first_frame_at = SDL_GetTicks();
        apply_live();
        load_font(w);
        return;  // the font is ImGui's default from the next frame
    }
    if (g_toggle_fullscreen) {
        g_toggle_fullscreen = false;
        const bool on = !is_fullscreen(w);
        set_fullscreen(w, on);
        bluewake_status_post(on ? "Fullscreen on" : "Fullscreen off");
    }
    if (g_toggle_menu) {
        g_toggle_menu = false;
        set_menu_open(!g_menu_open);
    }
    track_window(w);
    // The frame rate in the session log, a line a second beside the host's
    // [perf] line: frames shown (60 with Smooth Motion at full speed) and the
    // game's own (30 at full speed).
    // With it, where the game thread waited in that second (GXRuntime's
    // counters): for the GX worker at the game's draw-done barriers, and in
    // the present, of which end_frame is Aurora's submission.
    static Uint64 fps_logged;
    static DolAuroraFrameTiming timing_before;
    const Uint64 now = SDL_GetTicks();
    if (now - fps_logged >= 1000) {
        apply_smooth_rate(w);
        DolAuroraFrameTiming timing{};
        dol_aurora_frame_timing(&timing);
        if (fps_logged != 0)
            std::fprintf(stderr, "[fps] shown=%.1f game=%.1f drain_ms=%.0f present_ms=%.0f end_frame_ms=%.0f\n",
                         aurora::gfx::calculate_fps(), aurora::gfx::calculate_game_fps(),
                         (timing.drain_us - timing_before.drain_us) / 1000.0,
                         (timing.present_us - timing_before.present_us) / 1000.0,
                         (timing.end_frame_us - timing_before.end_frame_us) / 1000.0);
        fps_logged = now;
        timing_before = timing;
    }
    // A controller that connects starts from Aurora's mapping: look twice a
    // second whether the one on port 0 changed.
    static unsigned frames;
    apply_key_map();
    if ((++frames % 30u) == 0u) {
        const SDL_JoystickID connection = bw_controller_connection(0);
        if (connection != g_pad_connection) {
            g_pad_connection = connection;
            apply_controller();
        }
    }
    static unsigned healthy_frames;
    if (!g_menu_open && ++healthy_frames == 600u && !bw_launch_clear((g_data_dir + "launch.pending").c_str()))
        std::fprintf(stderr, "[safe-mode] could not clear launch marker\n");
    if (g_safe_mode && SDL_GetTicks() - g_first_frame_at < 15000) {
        ImGui::SetNextWindowBgAlpha(0.85f);
        ImGui::Begin("Launch recovery", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        // A fixed wrap width: TextWrapped in an auto-resizing window wraps to
        // the window's own width and collapses it to a narrow column.
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted("BlueWake didn't finish starting last time, so this time it starts with Fast "
                               "sound, 4:3 and the mods off. Your earlier settings were saved beside "
                               "settings.ini. You can turn things back on in the settings (F1).");
        ImGui::PopTextWrapPos();
        ImGui::End();
    }
    draw_climb_wheel();
    draw_shader_wait(w);
    if (bluewake_stage_select_take_question()) {
        set_menu_open(true);
        g_stage_select_question = true;
    }
    if (g_menu_open)
        draw_menu(w);
    draw_hint(w);
    bluewake_ui::draw_status_line(SDL_GetTicks(), ui_scale(w));
    if (g_dirty && SDL_GetTicks() - g_dirty_at > 1000 && !g_menu_open)
        save_file();
}

void save_at_exit() {
    if (g_dirty)
        save_file();
}

}  // namespace

namespace {

// A key the safe launch decides: its own line in the file goes, so the safe
// value is the only one written.
void forget_other(const char* key) {
    g_other.erase(std::remove_if(g_other.begin(), g_other.end(),
                                 [key](const std::pair<std::string, std::string>& line) {
                                     return line.first == key;
                                 }),
                  g_other.end());
}

// Safe mode (BLUEWAKE_SAFE_MODE, or a launch that never got to a clean exit):
// the settings file is copied aside, then this session and the file get the
// safe choices: Fast sound, 4:3, in a window, no HD look, mods, options or
// texture pack, and the gameplay extras off. If the copy fails the file is left
// alone and only this session is safe.
void apply_safe_mode() {
    const bool backed_up = bw_launch_backup(g_path.c_str());
    if (!backed_up)
        std::fprintf(stderr, "[safe-mode] settings backup failed; only this session is reset\n");
    static const char* const kSafe[][2] = {
        {"BLUEWAKE_DSP_MODE", "hle"},        {"BLUEWAKE_MODS", "none"},
        {"BLUEWAKE_OPTIONS", "none"},        {"BLUEWAKE_ASPECT", "4:3"},
        // Windows keeps the picture's shape by default, in safe mode too.
        {"DOL_AURORA_ASPECT_FIT", "1"},      {"DOL_AURORA_TEXTURE_PACK", ""},
        {"DOL_AURORA_FULLSCREEN", "0"},      {"BLUEWAKE_JUMP_BUTTON", "0"},
        {"BLUEWAKE_SPRINT_SPEED", "1"},      {"BLUEWAKE_FAST_FORWARD", "0"},
        {"BLUEWAKE_FADE_FRAMES", "0"},       {"BLUEWAKE_QUICK_DOORS", "0"},
        {"BLUEWAKE_STAGE_SELECT_TOGGLE", "0"},
        {"BLUEWAKE_HD_LOOK", "0"},
        // Clear overrides so the HD switch can enable every effect again later.
        {"BLUEWAKE_HD_DISTANT_ISLANDS", ""}, {"BLUEWAKE_HD_BLOOM", ""},
        {"BLUEWAKE_HD_FAR_BLUR", ""},        {"BLUEWAKE_HD_AO", ""},
        {"BLUEWAKE_HD_CLOUD_SHADOWS", ""},   {"BLUEWAKE_HD_SUN_SHADOWS", ""},
        {"BLUEWAKE_HD_EDGE_LIGHT", ""},      {"BLUEWAKE_HD_TORCH_LIGHT", ""},
    };
    for (const auto& pair : kSafe) {
        _putenv_s(pair[0], pair[1]);
        // Per-effect overrides are otherwise kept verbatim as unmanaged keys.
        if (backed_up)
            forget_other(pair[0]);
        g_edited.insert(pair[0]);  // saved as safe, even over a launch choice
    }
    std::fprintf(stderr, "[safe-mode] Fast sound, 4:3, a window, no HD look, mods or options%s\n",
                 backed_up ? "; the earlier settings were copied beside settings.ini" : "");
    if (backed_up) {
        g_dirty = true;
        save_file();
    }
}

}  // namespace

extern "C" void bw_settings_capture_environment(void) {
    wchar_t* block = GetEnvironmentStringsW();
    if (block == nullptr)
        return;
    wchar_t* end = block;
    while (*end != L'\0')
        end += wcslen(end) + 1;
    g_environment.assign(block, end + 1);
    FreeEnvironmentStringsW(block);
}

extern "C" void bw_settings_load(const char* data_dir) {
    g_data_dir = data_dir;
    g_path = g_data_dir + "settings.ini";
    // What was set before the file is read is the player's for this session (a
    // launch option, or a variable set before BlueWake started): it wins over
    // the file, which keeps its own value for it.
    for (const bw_settings::Setting& s : bw_settings::kSettings)
        if (std::getenv(s.key) != nullptr)
            g_launch[s.key] = "";
    load_file();
    if (!g_launch.empty()) {
        std::string names;
        for (const auto& [key, value] : g_launch)
            names += (names.empty() ? "" : ", ") + key;
        std::fprintf(stderr, "[settings] for this session, as launched: %s\n", names.c_str());
    }
    g_safe_mode = env_set("BLUEWAKE_SAFE_MODE") || bw_launch_pending((g_data_dir + "launch.pending").c_str());
    if (g_safe_mode)
        apply_safe_mode();
}

extern "C" void bw_settings_apply_launch(void) {
    // Windows' own defaults, where neither the file nor the command line said:
    // the picture keeps its shape, and the two door settings are off until they
    // have been played through here (settings_table.h has them on).
    env_default("DOL_AURORA_ASPECT_FIT", "1");
    env_default("BLUEWAKE_FAST_FORWARD", "0");
    env_default("BLUEWAKE_QUICK_DOORS", "0");
    // The frames a fade skips follow Fast transitions; both still work on their own.
    env_default("BLUEWAKE_FADE_FRAMES", env_on("BLUEWAKE_FAST_FORWARD", false) ? "6" : "0");
    // The table's defaults for the rest, so every setting the menu shows and
    // the file keeps has a value, and the texture filtering stays the game's
    // own until it is asked for.
    for (const bw_settings::Setting& s : bw_settings::kSettings)
        if (s.fallback[0] != '\0' && std::strcmp(s.key, "DOL_AURORA_FORCE_ANISO") != 0)
            env_default(s.key, s.fallback);
    // BLUEWAKE_STAGE_SELECT=1 on the command line arms it for this session (the
    // stage select takes either key, stage_select.h); the file keeps its toggle.
    if (env_set("BLUEWAKE_STAGE_SELECT")) {
        const bool armed = std::getenv("BLUEWAKE_STAGE_SELECT")[0] == '1';
        if (g_launch.count("BLUEWAKE_STAGE_SELECT_TOGGLE") == 0)
            g_launch["BLUEWAKE_STAGE_SELECT_TOGGLE"] = env("BLUEWAKE_STAGE_SELECT_TOGGLE", "0");
        _putenv_s("BLUEWAKE_STAGE_SELECT_TOGGLE", armed ? "1" : "0");
    }
    // The stage select's English names, which the builder installs beside BlueWake.exe.
    {
        char exe[MAX_PATH];
        const DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            std::string names(exe, n);
            names = names.substr(0, names.find_last_of("\\/") + 1) + "stage_select";
            const DWORD attributes = GetFileAttributesA(names.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
                env_default("BLUEWAKE_STAGE_SELECT_NAMES", names);
        }
    }
    CreateDirectoryA((g_data_dir + "Load").c_str(), nullptr);
    CreateDirectoryA((g_data_dir + "Load\\Textures").c_str(), nullptr);
    CreateDirectoryA(texture_folder().c_str(), nullptr);
    int width = g_window_w, height = g_window_h;
    if (width < 320 || height < 240)
        default_window(aspect_ratio(env("BLUEWAKE_ASPECT", "4:3")), &width, &height);
    env_default(kBwWindowKey, std::to_string(width) + "x" + std::to_string(height));
    // Where the window was last time, for the host to create it there
    // (runtime/host/src/main.c): Aurora shows the window before its first
    // frame, and nothing moves it after. Otherwise it opens centred: the first
    // launch, a spot no longer on a monitor, or a fullscreen start, which keeps
    // the display it always had. Aurora takes no negative position, so a spot
    // left of or above the main display opens centred too.
    if (!env_on("DOL_AURORA_FULLSCREEN", false) && g_window_x != INT_MIN && g_window_y != INT_MIN) {
        if (!on_a_monitor(g_window_x, g_window_y))
            std::fprintf(stderr, "[windows] window centred: %d,%d is no longer on a monitor\n", g_window_x,
                         g_window_y);
        else if (g_window_x < 0 || g_window_y < 0)
            std::fprintf(stderr, "[windows] window centred: Aurora cannot open it at %d,%d (negative)\n",
                         g_window_x, g_window_y);
        else
            env_default(kBwWindowPositionKey, std::to_string(g_window_x) + "," + std::to_string(g_window_y));
    }
    // Aurora read Smooth Motion and the frame rate before main; these hand it
    // the same values again, and the display's rate, which it could not know.
    aurora_set_frame_interpolation(env_on("DOL_AURORA_FRAME_INTERP", false));
    apply_smooth_rate(game_window());
    aurora_set_fps_overlay(env_on("DOL_AURORA_SHOW_FPS", false));
    apply_fps_position();
    // What a restart would change is measured against this (needs_restart);
    // the aspect, the mods and the pack are added as the menu reads them
    // (capture_launch_choices).
    for (const bw_settings::Setting& s : bw_settings::kSettings)
        g_launched[s.key] = file_value(s.key);
}

extern "C" void bw_settings_install(void) {
    dol_aurora_set_overlay(frame, nullptr);
    g_shader_wait = env_on("BLUEWAKE_SHADERS_FIRST", false);
    if (g_shader_wait) {
        dol_aurora_set_hold(shader_wait_hold, nullptr);
        dol_aurora_set_hold_redraw(true);
    }
    std::atexit(save_at_exit);
}

extern "C" int bw_settings_key(unsigned virtual_key, int alt) {
    switch (virtual_key) {
    case VK_F1:
        g_toggle_menu = true;
        return 1;
    case VK_ESCAPE:
        // Esc first gives the mouse back (the mouse camera sees it); then it
        // opens and closes the menu.
        if (!g_menu_open && bluewake_mouse_camera_captured())
            return 0;
        g_toggle_menu = true;
        return 1;
    case VK_F11:
        g_toggle_fullscreen = true;
        return 1;
    case VK_RETURN:
        if (!alt)
            return 0;
        g_toggle_fullscreen = true;
        return 1;
    case VK_F10: {
        // On at the rate last chosen, as the menu's one dropdown does it.
        const bool on = !aurora_get_frame_interpolation();
        set_smooth_motion(on ? env("DOL_AURORA_FRAME_INTERP_STEPS", "1").c_str() : nullptr);
        std::fprintf(stderr, "[windows] Smooth Motion %s\n", on ? "on" : "off");
        bluewake_status_post(on ? "Smooth Motion on" : "Smooth Motion off");
        return 1;
    }
    case VK_F6:
        // A second save key, unlisted: Wind Waker Recomp's, which players
        // learned. F5 (save) and F8 (load) are the shared keys (mouse_camera.c),
        // the same on every build (#320).
        bluewake_save_state_hotkey(false);
        return 1;
    case VK_F2: {
        // The Wind Waker HD look on or off, at once, and kept (#108).
        const bool on = !env_on("BLUEWAKE_HD_LOOK", false);
        set_env("BLUEWAKE_HD_LOOK", on ? "1" : "0");
        bluewake_hd_look_reload();
        std::fprintf(stderr, "[windows] Wind Waker HD look %s (F2)\n", on ? "on" : "off");
        bluewake_status_post(on ? "HD look on" : "HD look off");
        return 1;
    }
    case VK_F9: {
        const bool on = !env_on("DOL_AURORA_SHOW_FPS", false);
        set_env("DOL_AURORA_SHOW_FPS", on ? "1" : "0");
        aurora_set_fps_overlay(on);
        bluewake_status_post(on ? "Frame rate on" : "Frame rate off");
        return 1;
    }
    default:
        return 0;
    }
}

extern "C" int bw_settings_relaunch(void) {
    return !g_restart.take() || relaunch() ? 0 : 1;
}

extern "C" bool bluewake_settings_menu_event(const void* sdl_event) {
    const SDL_Event* event = static_cast<const SDL_Event*>(sdl_event);
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
        set_menu_open(!g_menu_open);
        return true;
    }
    return g_menu_open;
}
