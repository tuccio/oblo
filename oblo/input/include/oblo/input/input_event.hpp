#pragma once

#include <oblo/core/time/time.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    enum class mouse_key : u8
    {
        left,
        right,
        middle,
        enum_max,
    };

    enum class keyboard_key : u8
    {
        a,
        b,
        c,
        d,
        e,
        f,
        g,
        h,
        i,
        j,
        k,
        l,
        m,
        n,
        o,
        p,
        q,
        r,
        s,
        t,
        u,
        v,
        w,
        x,
        y,
        z,
        left_shift,
        enum_max
    };

    enum class input_event_kind : u8
    {
        mouse_move,
        mouse_press,
        mouse_release,
        mouse_wheel,
        keyboard_press,
        keyboard_release,
    };

    struct mouse_move
    {
        f32 x;
        f32 y;
    };

    struct mouse_press
    {
        mouse_key key;
    };

    struct mouse_release
    {
        mouse_key key;
    };

    struct keyboard_press
    {
        keyboard_key key;
    };

    struct keybard_release
    {
        keyboard_key key;
    };

    struct input_event
    {
        input_event_kind kind;
        time timestamp;

        union {
            mouse_move mouseMove;
            mouse_press mousePress;
            mouse_release mouseRelease;
            keyboard_press keyboardPress;
            keybard_release keyboardRelease;
        };
    };
}