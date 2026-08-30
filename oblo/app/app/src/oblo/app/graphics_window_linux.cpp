#include <oblo/app/graphics_engine.hpp>
#include <oblo/app/graphics_window.hpp>
#include <oblo/app/graphics_window_context.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/modules/module_manager.hpp>

#include <SDL.h>

namespace oblo
{
    namespace
    {
        graphics_window_context* get_graphics_window_context(graphics_window* w);
        const hit_test_fn& get_graphics_window_hit_test(graphics_window* w);

        template <auto Ctx, auto Hit>
        struct private_accessor
        {
            friend graphics_window_context* get_graphics_window_context(graphics_window* w)
            {
                return w->*Ctx;
            }

            friend const hit_test_fn& get_graphics_window_hit_test(graphics_window* w)
            {
                return w->*Hit;
            }
        };

        template struct private_accessor<&graphics_window::m_graphicsContext, &graphics_window::m_hitTest>;

        constexpr mouse_key sdl_map_mouse_key(u8 button)
        {
            switch (button)
            {
            case SDL_BUTTON_LEFT:
                return mouse_key::left;

            case SDL_BUTTON_RIGHT:
                return mouse_key::right;

            case SDL_BUTTON_MIDDLE:
                return mouse_key::middle;

            default:
                return mouse_key::enum_max;
            }
        }

        keyboard_key sdl_map_keyboard_key(SDL_Keycode key)
        {
            if (key >= SDLK_a && key <= SDLK_z)
            {
                return keyboard_key(u32(keyboard_key::a) + (key - SDLK_a));
            }

            switch (key)
            {
            case SDLK_LCTRL:
                return keyboard_key::left_ctrl;
            case SDLK_LSHIFT:
                return keyboard_key::left_shift;
            }

            return keyboard_key::enum_max;
        }

        time sdl_convert_time(Uint32 timestamp)
        {
            return time::from_milliseconds(i64(timestamp));
        }

        graphics_window* find_window(SDL_Window* sdlWindow)
        {
            return static_cast<graphics_window*>(SDL_GetWindowData(sdlWindow, "oblo::graphics_window"));
        }
    }

    graphics_window::graphics_window() = default;

    graphics_window::graphics_window(graphics_window&& other) noexcept
    {
        m_impl = other.m_impl;
        m_graphicsContext = other.m_graphicsContext;
        m_hitTest = other.m_hitTest;

        other.m_impl = nullptr;
        m_hitTest = {};

        if (m_impl)
        {
            SDL_SetWindowData(static_cast<SDL_Window*>(m_impl), "oblo::graphics_window", this);
        }
    }

    graphics_window::~graphics_window()
    {
        destroy();
    }

    graphics_window& graphics_window::operator=(graphics_window&& other) noexcept
    {
        destroy();

        m_impl = other.m_impl;
        m_graphicsContext = other.m_graphicsContext;
        m_hitTest = other.m_hitTest;

        other.m_impl = nullptr;
        m_hitTest = {};

        if (m_impl)
        {
            SDL_SetWindowData(static_cast<SDL_Window*>(m_impl), "oblo::graphics_window", this);
        }

        return *this;
    }

    bool graphics_window::create(const graphics_window_initializer& initializer)
    {
        OBLO_ASSERT(!m_impl);

        const u32 w = initializer.windowWidth ? initializer.windowWidth : 1280;
        const u32 h = initializer.windowHeight ? initializer.windowHeight : 720;

        u32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

        if (initializer.isHidden)
        {
            flags |= SDL_WINDOW_HIDDEN;
        }

        if (initializer.isBorderless)
        {
            flags |= SDL_WINDOW_BORDERLESS;
        }

        if (initializer.isMaximized)
        {
            flags |= SDL_WINDOW_MAXIMIZED;
        }

        SDL_Window* const sdlWindow = SDL_CreateWindow(
            initializer.title.c_str(),
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            i32(w),
            i32(h),
            flags);

        if (!sdlWindow)
        {
            return false;
        }

        SDL_SetWindowData(sdlWindow, "oblo::graphics_window", this);

        m_impl = sdlWindow;

        return true;
    }

    void graphics_window::destroy()
    {
        if (m_graphicsContext)
        {
            m_graphicsContext->on_destroy();
            m_graphicsContext = nullptr;
        }

        if (m_impl)
        {
            SDL_DestroyWindow(static_cast<SDL_Window*>(m_impl));
            m_impl = nullptr;
        }
    }

    bool graphics_window::initialize_graphics()
    {
        if (!is_open())
        {
            return false;
        }

        auto* const gfxEngine = module_manager::get().find_unique_service<graphics_engine>();

        if (!gfxEngine)
        {
            return false;
        }

        const auto [w, h] = get_size();
        m_graphicsContext = gfxEngine->create_context(get_native_handle(), w, h);

        return m_graphicsContext != nullptr;
    }

    bool graphics_window::is_ready() const
    {
        return m_impl && m_graphicsContext;
    }

    bool graphics_window::is_open() const
    {
        return m_impl;
    }

    bool graphics_window::is_maximized() const
    {
        return (SDL_GetWindowFlags(static_cast<SDL_Window*>(m_impl)) & SDL_WINDOW_MAXIMIZED) != 0;
    }

    bool graphics_window::is_minimized() const
    {
        return (SDL_GetWindowFlags(static_cast<SDL_Window*>(m_impl)) & SDL_WINDOW_MINIMIZED) != 0;
    }

    void graphics_window::maximize()
    {
        SDL_MaximizeWindow(static_cast<SDL_Window*>(m_impl));
    }

    void graphics_window::minimize()
    {
        SDL_MinimizeWindow(static_cast<SDL_Window*>(m_impl));
    }

    void graphics_window::restore()
    {
        SDL_RestoreWindow(static_cast<SDL_Window*>(m_impl));
    }

    bool graphics_window::is_hidden() const
    {
        return (SDL_GetWindowFlags(static_cast<SDL_Window*>(m_impl)) & SDL_WINDOW_HIDDEN) != 0;
    }

    void graphics_window::set_hidden(bool hide)
    {
        SDL_Window* const sdlWindow = static_cast<SDL_Window*>(m_impl);

        if (hide)
        {
            SDL_HideWindow(sdlWindow);
        }
        else
        {
            SDL_ShowWindow(sdlWindow);
        }
    }

    void graphics_window::set_custom_hit_test(const hit_test_fn& f)
    {
        m_hitTest = f;
    }

    vec2u graphics_window::get_size() const
    {
        int w{};
        int h{};

        SDL_GetWindowSize(static_cast<SDL_Window*>(m_impl), &w, &h);

        return {u32(w), u32(h)};
    }

    native_window_handle graphics_window::get_native_handle() const
    {
        return m_impl;
    }

    void graphics_window::set_icon(u32 w, u32 h, std::span<const byte> data)
    {
        SDL_Window* const sdlWindow = static_cast<SDL_Window*>(m_impl);

        // Our pixel data is laid out as BGRA in memory, i.e. little-endian value b | (g << 8) | (r << 16) | (a << 24)
        SDL_Surface* const surface = SDL_CreateRGBSurfaceFrom(
            const_cast<byte*>(data.data()),
            i32(w),
            i32(h),
            32,
            i32(w * 4),
            0x00FF0000,
            0x0000FF00,
            0x000000FF,
            0xFF000000);

        if (!surface)
        {
            return;
        }

        SDL_SetWindowIcon(sdlWindow, surface);
        SDL_FreeSurface(surface);
    }

    void graphics_window::set_input_queue(input_queue* inputQueue)
    {
        m_inputQueue = inputQueue;
    }

    input_queue* graphics_window::get_input_queue() const
    {
        return m_inputQueue;
    }

    void window_event_processor::set_event_dispatcher(const window_event_dispatcher& dispatcher)
    {
        m_windowEventDispatcher = dispatcher;
    }

    bool window_event_processor::process_events() const
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                return false;

            case SDL_WINDOWEVENT: {
                SDL_Window* const sdlWindow = SDL_GetWindowFromID(event.window.windowID);
                graphics_window* const window = sdlWindow ? find_window(sdlWindow) : nullptr;

                if (!window)
                {
                    break;
                }

                switch (event.window.event)
                {
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    if (graphics_window_context* const ctx = get_graphics_window_context(window))
                    {
                        ctx->on_resize(u32(event.window.data1), u32(event.window.data2));
                    }
                    break;

                case SDL_WINDOWEVENT_CLOSE:
                    window->destroy();
                    break;
                }

                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                graphics_window* const window = find_window(SDL_GetWindowFromID(event.button.windowID));
                input_queue* const inputQueue = window ? window->get_input_queue() : nullptr;

                if (inputQueue)
                {
                    const bool pressed = event.type == SDL_MOUSEBUTTONDOWN;

                    if (pressed)
                    {
                        inputQueue->push({
                            .kind = input_event_kind::mouse_press,
                            .timestamp = sdl_convert_time(event.button.timestamp),
                            .mousePress =
                                {
                                    .key = sdl_map_mouse_key(event.button.button),
                                    .x = f32(event.button.x),
                                    .y = f32(event.button.y),
                                },
                        });
                    }
                    else
                    {
                        inputQueue->push({
                            .kind = input_event_kind::mouse_release,
                            .timestamp = sdl_convert_time(event.button.timestamp),
                            .mouseRelease =
                                {
                                    .key = sdl_map_mouse_key(event.button.button),
                                    .x = f32(event.button.x),
                                    .y = f32(event.button.y),
                                },
                        });
                    }
                }

                break;
            }

            case SDL_MOUSEMOTION: {
                graphics_window* const window = find_window(SDL_GetWindowFromID(event.motion.windowID));
                input_queue* const inputQueue = window ? window->get_input_queue() : nullptr;

                if (inputQueue)
                {
                    inputQueue->push({
                        .kind = input_event_kind::mouse_move,
                        .timestamp = sdl_convert_time(event.motion.timestamp),
                        .mouseMove =
                            {
                                .x = f32(event.motion.x),
                                .y = f32(event.motion.y),
                            },
                    });
                }

                break;
            }

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                graphics_window* const window = find_window(SDL_GetWindowFromID(event.key.windowID));
                input_queue* const inputQueue = window ? window->get_input_queue() : nullptr;

                if (inputQueue)
                {
                    const bool pressed = event.type == SDL_KEYDOWN;

                    if (pressed)
                    {
                        inputQueue->push({
                            .kind = input_event_kind::keyboard_press,
                            .timestamp = sdl_convert_time(event.key.timestamp),
                            .keyboardPress =
                                {
                                    .key = sdl_map_keyboard_key(event.key.keysym.sym),
                                },
                        });
                    }
                    else
                    {
                        inputQueue->push({
                            .kind = input_event_kind::keyboard_release,
                            .timestamp = sdl_convert_time(event.key.timestamp),
                            .keyboardRelease =
                                {
                                    .key = sdl_map_keyboard_key(event.key.keysym.sym),
                                },
                        });
                    }
                }

                break;
            }
            }

            if (m_windowEventDispatcher.dispatch)
            {
                m_windowEventDispatcher.dispatch(&event);
            }
        }

        return true;
    }
}
