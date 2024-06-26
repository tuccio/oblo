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

        void process(std::span<const input_event> events, time dt);

        void bind(mouse_key key, action a);
        void bind(keyboard_key key, action a);

        void clear_bindings();

        void action_begin(action a);
        void action_end(action a);

        void set_screen_size(vec2 size);

        void set_speed(f32 speed);
        void set_sensitivity(f32 sensitivity);

        void reset(const vec3& position, const quaternion& rotation);
        void reset_actions();

        void set_common_wasd_bindings();

    private:
        vec3 m_position{};
        quaternion m_orientation{quaternion::identity()};
        f32 m_sensitivity{1.f};
        f32 m_speed{1.f};
        f32 m_speedMultiplier{10.f};

        vec2 m_screenSize{1, 1};
        vec2 m_mousePosition{};

        bool m_isMouseLookEnabled{};
        bool m_applySpeedMultiplier{};
        std::array<bool, 6> m_strafe{};

        std::array<action, 1 + u32(mouse_key::enum_max)> m_mouseKeyActions{};
        std::array<action, 1 + u32(keyboard_key::enum_max)> m_keyboardKeyActions{};
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
        speed_multiplier,
    };
}