#include <oblo/input/utility/fps_camera_controller.hpp>

namespace oblo
{
    namespace
    {
        constexpr u32 get_strafe_index(fps_camera_controller::action a)
        {
            using action = fps_camera_controller::action;

            constexpr auto base = min(action::strafe_left,
                action::strafe_right,
                action::strafe_forward,
                action::strafe_backward,
                action::strafe_upward,
                action::strafe_downward);

            return u32(a) - u32(base);
        }
    }

    vec3 fps_camera_controller::get_position() const
    {
        return m_position;
    }

    quaternion fps_camera_controller::get_orientation() const
    {
        return m_orientation;
    }

    void fps_camera_controller::process(std::span<const input_event> events, time dt)
    {
        const auto lastMousePosition = m_mousePosition;

        for (const auto& e : events)
        {
            switch (e.kind)
            {
            case input_event_kind::mouse_press:
                if (const auto action = m_mouseKeyActions[u32(e.mousePress.key)]; action != action::none)
                {
                    action_begin(action);
                }

                break;

            case input_event_kind::mouse_release:
                if (const auto action = m_mouseKeyActions[u32(e.keyboardRelease.key)]; action != action::none)
                {
                    action_end(action);
                }

                break;

            case input_event_kind::mouse_move:
                m_mousePosition = {e.mouseMove.x, e.mouseMove.y};
                break;

            case input_event_kind::keyboard_press:
                if (const auto action = m_keyboardKeyActions[u32(e.keyboardPress.key)]; action != action::none)
                {
                    action_begin(action);
                }

                break;

            case input_event_kind::keyboard_release:
                if (const auto action = m_keyboardKeyActions[u32(e.keyboardRelease.key)]; action != action::none)
                {
                    action_end(action);
                }

                break;

            default:
                break;
            }
        }

        if (m_isMouseLookEnabled)
        {
            const vec2 distance = m_mousePosition - lastMousePosition;
            const vec2 t = distance / vec2{.x = m_screenSize.x, .y = m_screenSize.y};
            const vec2 delta{.x = std::atan(t.x), .y = std::atan(t.y)};
            const radians deltaX{-delta.x * m_sensitivity};
            const radians deltaY{-delta.y * m_sensitivity};

            const auto yaw = quaternion::from_axis_angle(vec3{.y = 1.f}, deltaX);
            const auto pitch = quaternion::from_axis_angle(vec3{.x = 1.f}, deltaY);

            m_orientation = normalize(yaw * m_orientation * pitch);
        }

        vec3 strafe{};

        if (m_strafe[get_strafe_index(action::strafe_left)])
        {
            strafe.x -= 1.f;
        }

        if (m_strafe[get_strafe_index(action::strafe_right)])
        {
            strafe.x += 1.f;
        }

        if (m_strafe[get_strafe_index(action::strafe_forward)])
        {
            strafe.z -= 1.f;
        }

        if (m_strafe[get_strafe_index(action::strafe_backward)])
        {
            strafe.z += 1.f;
        }

        if (m_strafe[get_strafe_index(action::strafe_upward)])
        {
            strafe.y += 1.f;
        }

        if (m_strafe[get_strafe_index(action::strafe_downward)])
        {
            strafe.y -= 1.f;
        }

        const f32 dtSeconds{to_f32_seconds(dt)};

        const f32 speed = m_applySpeedMultiplier ? m_speed * m_speedMultiplier : m_speed;
        m_position = m_position + m_orientation * (strafe * speed * dtSeconds);
    }

    void fps_camera_controller::bind(mouse_key key, action a)
    {
        m_mouseKeyActions[u32(key)] = a;
    }

    void fps_camera_controller::bind(keyboard_key key, action a)
    {
        m_keyboardKeyActions[u32(key)] = a;
    }

    void fps_camera_controller::clear_bindings()
    {
        m_mouseKeyActions = {};
        m_keyboardKeyActions = {};
    }

    void fps_camera_controller::action_begin(action a)
    {
        switch (a)
        {
        case action::mouse_look:
            m_isMouseLookEnabled = true;
            break;

        case action::strafe_left:
        case action::strafe_right:
        case action::strafe_forward:
        case action::strafe_backward:
        case action::strafe_upward:
        case action::strafe_downward:
            m_strafe[get_strafe_index(a)] = true;
            break;

        case action::speed_multiplier:
            m_applySpeedMultiplier = true;
            break;

        default:
            break;
        }
    }

    void fps_camera_controller::action_end(action a)
    {
        switch (a)
        {
        case action::mouse_look:
            m_isMouseLookEnabled = false;
            break;

        case action::strafe_left:
        case action::strafe_right:
        case action::strafe_forward:
        case action::strafe_backward:
        case action::strafe_upward:
        case action::strafe_downward:
            m_strafe[get_strafe_index(a)] = false;
            break;

        case action::speed_multiplier:
            m_applySpeedMultiplier = false;

        default:
            break;
        }
    }

    void fps_camera_controller::set_screen_size(vec2 size)
    {
        m_screenSize = size;
    }

    void fps_camera_controller::set_speed(f32 speed)
    {
        m_speed = speed;
    }

    void fps_camera_controller::set_sensitivity(f32 sensitivity)
    {
        m_sensitivity = sensitivity;
    }

    void fps_camera_controller::reset(const vec3& position, const quaternion& rotation)
    {
        m_position = position;
        m_orientation = rotation;
    }

    void fps_camera_controller::reset_actions()
    {
        m_isMouseLookEnabled = false;
        m_applySpeedMultiplier = false;
        m_strafe = {};
    }

    void fps_camera_controller::set_common_wasd_bindings()
    {
        bind(mouse_key::right, action::mouse_look);

        bind(keyboard_key::w, action::strafe_forward);
        bind(keyboard_key::a, action::strafe_left);
        bind(keyboard_key::s, action::strafe_backward);
        bind(keyboard_key::d, action::strafe_right);
        bind(keyboard_key::e, action::strafe_upward);
        bind(keyboard_key::q, action::strafe_downward);

        bind(keyboard_key::left_shift, action::speed_multiplier);
    }
}