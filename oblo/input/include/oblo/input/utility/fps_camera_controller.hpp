#pragma once

#include <oblo/input/input_event.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>

#include <array>
#include <span>

namespace oblo
{
    class fps_camera_controller
    {
    public:
        enum class action : u8;

    public:
        vec3 get_position() const;
        quaternion get_orientation() const;

        void process(std::span<const input_event> events);

        void bind(mouse_key key, action a);
        void bind(keyboard_key key, action a);

        void clear_bindings();

        void action_begin(action a, timestamp ts);
        void action_end(action a, timestamp ts);

        void set_screen_size(vec2 size);

        void set_speed(f32 speed);
        void set_sensitivity(f32 sensitivity);

    private:
        vec3 m_position{};
        quaternion m_orientation{quaternion::identity()};
        f32 m_sensitivity{1.f};
        f32 m_speed{1.f};

        vec2 m_screenSize{1, 1};
        vec2 m_mousePosition{};
        timestamp m_lastTimestamp{};

        bool m_isMouseLookEnabled{};
        std::array<bool, 6> m_strafe;

        std::array<action, u32(mouse_key::enum_max)> m_mouseKeyActions;
        std::array<action, u32(keyboard_key::enum_max)> m_keyboardKeyActions;
    };

    enum class fps_camera_controller::action : u8
    {
        none,
        mouse_look,
        strafe_left,
        strafe_right,
        strafe_forward,
        strafe_backward,
        strafe_upward,
        strafe_downward,
    };
}