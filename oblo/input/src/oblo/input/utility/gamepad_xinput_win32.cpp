#include <oblo/input/gamepad.hpp>

#include <oblo/core/span.hpp>
#include <oblo/core/utility.hpp>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>

#include <xinput.h>

#pragma comment(lib, "Xinput.lib")

namespace oblo
{
    namespace
    {
        flags<gamepad_button> get_pressed_buttons(const XINPUT_GAMEPAD& gamepad)
        {
            flags<gamepad_button> pressed{};

            // Physical positions on an Xbox layout: A is south, B east, X west, Y north
            if ((gamepad.wButtons & XINPUT_GAMEPAD_A) != 0)
            {
                pressed.set(gamepad_button::xbox_a);
            }

            if ((gamepad.wButtons & XINPUT_GAMEPAD_B) != 0)
            {
                pressed.set(gamepad_button::xbox_b);
            }

            if ((gamepad.wButtons & XINPUT_GAMEPAD_X) != 0)
            {
                pressed.set(gamepad_button::xbox_x);
            }

            if ((gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0)
            {
                pressed.set(gamepad_button::xbox_y);
            }

            if ((gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0)
            {
                pressed.set(gamepad_button::left_shoulder);
            }

            if ((gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0)
            {
                pressed.set(gamepad_button::right_shoulder);
            }

            if ((gamepad.wButtons & XINPUT_GAMEPAD_START) != 0)
            {
                pressed.set(gamepad_button::start);
            }

            if ((gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0)
            {
                pressed.set(gamepad_button::back);
            }

            if ((gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0)
            {
                pressed.set(gamepad_button::left_thumb);
            }

            if ((gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0)
            {
                pressed.set(gamepad_button::right_thumb);
            }

            return pressed;
        }
    }

    void poll_gamepads(span<gamepad> gamepads)
    {
        const u32 count = min<u32>(gamepads.size32(), XUSER_MAX_COUNT);

        XINPUT_STATE state{};

        for (u32 i = 0; i < count; ++i)
        {
            const DWORD result = XInputGetState(i, &state);

            // We may want to check ERROR_DEVICE_NOT_CONNECTED instead, but need to handle errors too
            const bool isConnected = result == ERROR_SUCCESS;

            auto& current = gamepads[i];

            current.connected = isConnected;

            if (!isConnected)
            {
                continue;
            }

            current.pressedButtons = get_pressed_buttons(state.Gamepad);

            const float triggerNorm = 255.f;
            const float thumbNorm = 32767.f;

            current.leftTrigger = float(state.Gamepad.bLeftTrigger) / triggerNorm;
            current.rightTrigger = float(state.Gamepad.bRightTrigger) / triggerNorm;

            current.thumbRight = vec2{
                float(state.Gamepad.sThumbRX) / thumbNorm,
                float(state.Gamepad.sThumbRY) / thumbNorm,
            };

            current.thumbLeft = vec2{
                float(state.Gamepad.sThumbLX) / thumbNorm,
                float(state.Gamepad.sThumbLY) / thumbNorm,
            };
        }

        for (u32 i = count; i < gamepads.size(); ++i)
        {
            gamepads[i].connected = false;
        }
    }
}