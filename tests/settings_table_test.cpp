#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include "../runtime/host/src/settings_table.h"

using namespace bw_settings;

// The Mac and Linux settings file's keys, in its order, as the menu wrote them
// before the table: a file saved before and after it is the same.
static const char* const kFileOrder[] = {
    "BLUEWAKE_ASPECT", "DOL_AURORA_FULLSCREEN", "DOL_AURORA_RENDER_SCALE", "DOL_AURORA_FRAME_INTERP",
    "DOL_AURORA_FRAME_INTERP_STEPS", "DOL_AURORA_SHOW_FPS", "DOL_AURORA_FPS_POSITION", "DOL_AURORA_FORCE_ANISO",
    "DOL_AURORA_TEXTURE_PACK", "BLUEWAKE_MODS", "BLUEWAKE_OPTIONS", "BLUEWAKE_FADE_FRAMES",
    "BLUEWAKE_FAST_FORWARD", "BLUEWAKE_QUICK_DOORS", "BLUEWAKE_JUMP_BUTTON", "BLUEWAKE_PAD_SWAP_AB",
    "BLUEWAKE_PAD_SWAP_XY", "BLUEWAKE_PAD_SWAP_LR", "BLUEWAKE_DPAD_MENUS", "BLUEWAKE_BUTTON_MAP",
    "BLUEWAKE_SPRINT_SPEED", "BLUEWAKE_MOUSE_CAMERA", "BLUEWAKE_MOUSE_SENSITIVITY", "BLUEWAKE_MOUSE_INVERT_Y",
    "BLUEWAKE_MOUSE_BUTTONS", "BLUEWAKE_KEY_MAP", "BLUEWAKE_STICK_CAMERA", "BLUEWAKE_STICK_CAMERA_SPEED",
    "BLUEWAKE_STICK_CAMERA_INVERT_X", "BLUEWAKE_STICK_CAMERA_INVERT_Y", "BLUEWAKE_STICK_AIM_SPEED",
    "BLUEWAKE_STICK_ZOOM", "BLUEWAKE_STICK_ZOOM_SPEED", "BLUEWAKE_AIM_INVERT_Y", "BLUEWAKE_GYRO_AIM",
    "BLUEWAKE_GYRO_AIM_SPEED", "BLUEWAKE_HAPTICS", "BLUEWAKE_HAPTICS_STRENGTH", "BLUEWAKE_HAPTICS_TRIGGERS",
    "BLUEWAKE_CLIMB", "BLUEWAKE_CLIMB_STAMINA", "BLUEWAKE_FOREST_WATER_KEEP_TREES",
    "BLUEWAKE_FOREST_WATER_30_MINUTES", "BLUEWAKE_STAGE_SELECT_TOGGLE", "BLUEWAKE_HD_LOOK", "DOL_AURORA_ASPECT_FIT",
    "BLUEWAKE_PAUSE_UNFOCUSED", "BLUEWAKE_SHADERS_FIRST", "BLUEWAKE_SPRINT_CHOICE", "BLUEWAKE_DSP_MODE",
};

int main() {
    assert(kCount == static_cast<int>(sizeof kFileOrder / sizeof kFileOrder[0]));
    std::set<std::string> keys;
    for (int i = 0; i < kCount; ++i) {
        const Setting& s = kSettings[i];
        assert(std::strcmp(s.key, kFileOrder[i]) == 0);
        assert(keys.insert(s.key).second);
        assert(find(s.key) == &s);
        if (s.kind == Kind::Bool)
            assert(std::strcmp(s.fallback, "0") == 0 || std::strcmp(s.fallback, "1") == 0);
        // A slider's default sits inside its range.
        if ((s.kind == Kind::Int || s.kind == Kind::Float) && s.label != nullptr && s.high > s.low) {
            const float value = std::strtof(s.fallback, nullptr);
            assert(value >= s.low && value <= s.high);
        }
    }
    assert(find("BLUEWAKE_NOT_A_SETTING") == nullptr);
    assert(default_on(*find("BLUEWAKE_MOUSE_CAMERA")) && !default_on(*find("BLUEWAKE_HD_LOOK")));
    assert(std::strcmp(find("BLUEWAKE_ASPECT")->windows, "aspect") == 0);
    return 0;
}
