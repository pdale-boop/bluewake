#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstring>
#include <sstream>
#include "../windows/src/settings_file.h"

// Every setting the file keeps (not the session-only option_defaults_off).
static bool same(const Settings& a, const Settings& b) {
    const auto near = [](double x, double y) { return std::fabs(x - y) < 0.006; };
    return a.fullscreen == b.fullscreen && a.window_w == b.window_w && a.window_h == b.window_h &&
           a.window_x == b.window_x && a.window_y == b.window_y && a.render_scale == b.render_scale &&
           a.anisotropy == b.anisotropy && a.smooth_motion == b.smooth_motion && a.smooth_steps == b.smooth_steps &&
           a.show_fps == b.show_fps && a.fps_position == b.fps_position && a.pause_unfocused == b.pause_unfocused &&
           a.shaders_first == b.shaders_first && a.mouse_camera == b.mouse_camera &&
           near(a.mouse_sensitivity, b.mouse_sensitivity) && a.mouse_invert_y == b.mouse_invert_y &&
           a.controller_swap_ab == b.controller_swap_ab && a.controller_swap_xy == b.controller_swap_xy &&
           a.controller_swap_lr == b.controller_swap_lr && a.dpad_menus == b.dpad_menus &&
           a.button_map == b.button_map && a.mouse_buttons == b.mouse_buttons && a.key_map == b.key_map &&
           a.pad_invert_x == b.pad_invert_x && a.pad_invert_y == b.pad_invert_y && a.stick_camera == b.stick_camera &&
           a.stick_speed == b.stick_speed && a.stick_aim_speed == b.stick_aim_speed && a.stick_zoom == b.stick_zoom &&
           near(a.stick_zoom_speed, b.stick_zoom_speed) && a.aim_invert_y == b.aim_invert_y &&
           a.gyro_aim == b.gyro_aim && near(a.gyro_aim_speed, b.gyro_aim_speed) && a.climb == b.climb &&
           a.climb_stamina == b.climb_stamina && a.forest_keep_trees == b.forest_keep_trees &&
           a.forest_30_minutes == b.forest_30_minutes && a.hd_look == b.hd_look && a.haptics == b.haptics &&
           a.haptics_strength == b.haptics_strength && a.haptics_triggers == b.haptics_triggers &&
           a.aspect == b.aspect && a.keep_aspect == b.keep_aspect && a.betterww == b.betterww &&
           a.options == b.options && a.hd_textures == b.hd_textures && a.texture_pack == b.texture_pack &&
           a.lle_audio == b.lle_audio && a.movement_extras == b.movement_extras &&
           near(a.sprint_speed, b.sprint_speed) && a.fast_transitions == b.fast_transitions &&
           a.quick_doors == b.quick_doors && a.stage_select == b.stage_select;
}

static BwSettingsLines lines_of(const char* text) {
    BwSettingsLines lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (!line.empty() && line[0] != '#' && eq != std::string::npos)
            lines.emplace_back(line.substr(0, eq), line.substr(eq + 1));
    }
    return lines;
}

static Settings round_trip(const Settings& d, const std::string& pack, BwSettingsLines* other = nullptr) {
    Settings back;
    BwSettingsLines unknown;
    bw_settings_read_new(bw_settings_write_new(d, pack), pack, back, other != nullptr ? *other : unknown);
    return back;
}

int main() {
    const std::string pack = "C:\\Users\\p\\AppData\\Roaming\\BlueWake\\Load\\Textures\\GZLE01";

    // The defaults, and every table key written, in the table's order.
    const Settings defaults;
    const BwSettingsLines written = bw_settings_write_new(defaults, pack);
    assert(static_cast<int>(written.size()) >= bw_settings::kCount);
    for (int i = 0; i < bw_settings::kCount; ++i)
        assert(written[i].first == bw_settings::kSettings[i].key);
    assert(same(round_trip(defaults, pack), defaults));

    // An old Windows file, a player's: every setting carries over.
    const char* old_file =
        "# BlueWake settings (the in-game menu, F1, writes this file)\n"
        "fullscreen=1\nwindow=1920x1080\nwindow_position=40,60\nrender_scale=3\nanisotropy=8\nsmooth_motion=1\n"
        "show_fps=1\npause_unfocused=1\nfps_position=top-left\ncompile_shaders_first=1\nsmooth_motion_fps=display\n"
        "mouse_camera=0\nmouse_sensitivity=0.35\nmouse_invert_y=1\ncontroller_swap_ab=1\ncontroller_swap_xy=1\n"
        "controller_swap_lr=1\ndpad_menus=1\nbutton_map=1,0,2,3,10,6\nmouse_buttons=a,b,x,y,z\nkey_map=13,14,24,12,20,8,21,40\n"
        "controller_invert_x=1\ncontroller_invert_y=1\nstick_camera=0\nstick_camera_speed=500\nstick_aim_speed=90\n"
        "stick_zoom=1\nstick_zoom_speed=0.750\naim_invert_y=1\ngyro_aim=1\ngyro_aim_speed=2.50\nclimb=1\n"
        "climb_stamina=20\nforest_water_keep_trees=1\nforest_water_30_minutes=1\nhd_look=1\n"
        "texture_pack=D:\\Packs\\WWHD\nhaptics=classic\nhaptics_strength=35\nhaptics_triggers=0\naspect=21:9\n"
        "keep_aspect=0\nbetterww=1\nhd_textures=1\nlle_audio=1\noption.swift_sail=1\noption.instant_text=0\n"
        "movement_extras=1\nsprint_speed=1.84\nfast_transitions=1\nquick_doors=1\nstage_select=1\n";
    Settings player;
    for (const auto& [k, v] : lines_of(old_file))
        assert(bw_settings_read_old(k, v, player));
    assert(player.aspect == "21:9" && player.texture_pack == "D:\\Packs\\WWHD" && player.smooth_steps == -1);
    assert(same(round_trip(player, pack), player));

    // HD textures off keep the folder for when they come back; the default
    // folder is written out and read back as "".
    Settings off = player;
    off.hd_textures = false;
    assert(same(round_trip(off, pack), off));
    Settings default_folder = player;
    default_folder.texture_pack = "";
    const BwSettingsLines lines = bw_settings_write_new(default_folder, pack);
    for (const auto& [k, v] : lines)
        if (k == "DOL_AURORA_TEXTURE_PACK")
            assert(v == pack);
    assert(same(round_trip(default_folder, pack), default_folder));

    // Jump & Sprint off keeps the chosen speed; the host's own key is 1.
    Settings walking = player;
    walking.movement_extras = false;
    assert(same(round_trip(walking, pack), walking));

    // Lines this doesn't know stay, as they were.
    BwSettingsLines in = bw_settings_write_new(player, pack), other;
    in.emplace_back("BLUEWAKE_PICTO_CROP", "1");
    in.emplace_back("SOMETHING_ELSE", "x=y");
    Settings back;
    bw_settings_read_new(in, pack, back, other);
    assert(same(back, player));
    assert(other.size() == 2 && other[0].first == "BLUEWAKE_PICTO_CROP" && other[1].second == "x=y");

    // A Mac or Linux file's options ("none" and the ones on).
    Settings mac;
    BwSettingsLines unused;
    bw_settings_read_new(lines_of("BLUEWAKE_OPTIONS=none,instant_text,swift_sail\n"), pack, mac, unused);
    assert(mac.options.size() == 2 && mac.options["instant_text"] && mac.options["swift_sail"]);
    return 0;
}
