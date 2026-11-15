#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/forward.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/vec2.hpp>

namespace oblo
{
    enum class gamepad_button : u8
    {
        dpad_up,
        dpad_down,
        dpad_left,
        dpad_right,
        start,
        back,
        left_thumb,
        right_thumb,
        left_shoulder,
        right_shoulder,
        a,
        b,
        x,
        y,
        enum_max,
    };

    struct gamepad
    {
        bool connected;

        flags<gamepad_button> pressedButtons;

        f32 leftTrigger;
        f32 rightTrigger;

        vec2 thumbLeft;
        vec2 thumbRight;
    };

    void poll_gamepads(span<gamepad> gamepads);
}
