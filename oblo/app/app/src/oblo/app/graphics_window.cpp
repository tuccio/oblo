#include <oblo/app/graphics_window.hpp>

#include <oblo/app/graphics_engine.hpp>
#include <oblo/app/graphics_window_context.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/modules/module_manager.hpp>

#include <SDL.h>
#include <SDL_syswm.h>

namespace oblo
{
    namespace
    {
        constexpr const char* g_WindowGraphicsContext = "gfx";
        constexpr const char* g_WindowPtr = "wnd";

        SDL_Window* sdl_window(void* impl)
        {
            return static_cast<SDL_Window*>(impl);
        }

        struct window_info
        {
            SDL_Window* window;
            graphics_window_context* graphicsContext;
        };

        window_info get_graphics_context(const SDL_Event& event)
        {
            SDL_Window* const window = SDL_GetWindowFromID(event.window.windowID);
            void* const windowData = window ? SDL_GetWindowData(window, g_WindowGraphicsContext) : nullptr;
            auto* const graphicsContext = static_cast<graphics_window_context*>(windowData);
            return {window, graphicsContext};
        }

        SDL_HitTestResult to_sdl_hit_test_result(hit_test_result res)
        {
            switch (res)
            {
            case hit_test_result::normal:
                return SDL_HITTEST_NORMAL;
            case hit_test_result::draggable:
                return SDL_HITTEST_DRAGGABLE;
            case hit_test_result::resize_top_left:
                return SDL_HITTEST_RESIZE_TOPLEFT;
            case hit_test_result::resize_top:
                return SDL_HITTEST_RESIZE_TOP;
            case hit_test_result::resize_top_right:
                return SDL_HITTEST_RESIZE_TOPRIGHT;
            case hit_test_result::resize_right:
                return SDL_HITTEST_RESIZE_RIGHT;
            case hit_test_result::resize_bottom_right:
                return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
            case hit_test_result::resize_bottom:
                return SDL_HITTEST_RESIZE_BOTTOM;
            case hit_test_result::resize_bottom_left:
                return SDL_HITTEST_RESIZE_BOTTOMLEFT;
            case hit_test_result::resize_left:
                return SDL_HITTEST_RESIZE_LEFT;
            default:
                unreachable();
            }
        }
    }

    graphics_window::graphics_window() = default;

    graphics_window::graphics_window(graphics_window&& other) noexcept
    {
        m_impl = other.m_impl;
        m_graphicsContext = other.m_graphicsContext;
        m_style = other.m_style;
        other.m_impl = nullptr;
        other.m_graphicsContext = nullptr;

        if (auto* sdlWindow = sdl_window(m_impl))
        {
            SDL_SetWindowData(sdlWindow, g_WindowPtr, this);
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
        m_style = other.m_style;
        other.m_impl = nullptr;
        other.m_graphicsContext = nullptr;

        if (auto* sdlWindow = sdl_window(m_impl))
        {
            SDL_SetWindowData(sdlWindow, g_WindowPtr, this);
        }

        return *this;
    }

    bool graphics_window::create(const graphics_window_initializer& initializer)
    {
        OBLO_ASSERT(!m_impl);

        i32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

        if (initializer.style == window_style::app)
        {
            SDL_SetHintWithPriority("SDL_BORDERLESS_WINDOWED_STYLE", "1", SDL_HINT_OVERRIDE);
            SDL_SetHintWithPriority("SDL_BORDERLESS_RESIZABLE_STYLE",
                "1",
                SDL_HINT_OVERRIDE); // This seems to allow the snap features in Windows Aero
        }

        if (initializer.isMaximized)
        {
            windowFlags |= SDL_WINDOW_MAXIMIZED;
        }

        if (initializer.isHidden)
        {
            windowFlags |= SDL_WINDOW_HIDDEN;
        }

        if (initializer.isBorderless)
        {
            windowFlags |= SDL_WINDOW_BORDERLESS;
        }

        const i32 w = initializer.windowWidth == 0 ? 1280u : initializer.windowWidth;
        const i32 h = initializer.windowHeight == 0 ? 720u : initializer.windowHeight;

        SDL_Window* const window = SDL_CreateWindow(initializer.title.c_str(),
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            w,
            h,
            windowFlags);

        if (initializer.style == window_style::app)
        {
            SDL_SetHintWithPriority("SDL_BORDERLESS_WINDOWED_STYLE", nullptr, SDL_HINT_OVERRIDE);
            SDL_SetHintWithPriority("SDL_BORDERLESS_RESIZABLE_STYLE", nullptr, SDL_HINT_OVERRIDE);
        }

        m_impl = window;
        m_style = initializer.style;

        if (window)
        {
            SDL_SetWindowData(window, g_WindowPtr, this);
        }

        return m_impl != nullptr;
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
            SDL_DestroyWindow(sdl_window(m_impl));
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
        SDL_SetWindowData(sdl_window(m_impl), g_WindowGraphicsContext, m_graphicsContext);

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
        const auto flags = SDL_GetWindowFlags(sdl_window(m_impl));
        return (flags & SDL_WINDOW_MAXIMIZED) != 0;
    }

    bool graphics_window::is_minimized() const
    {
        const auto flags = SDL_GetWindowFlags(sdl_window(m_impl));
        return (flags & SDL_WINDOW_MINIMIZED) != 0;
    }

    void graphics_window::maximize()
    {
        SDL_Window* const window = sdl_window(m_impl);
        SDL_MaximizeWindow(window);
    }

    void graphics_window::minimize()
    {
        SDL_Window* const window = sdl_window(m_impl);
        SDL_MinimizeWindow(window);
    }

    void graphics_window::restore()
    {
        SDL_Window* const window = sdl_window(m_impl);
        SDL_RestoreWindow(window);
    }

    bool graphics_window::is_hidden() const
    {
        const auto flags = SDL_GetWindowFlags(sdl_window(m_impl));
        return (flags & SDL_WINDOW_HIDDEN) != 0;
    }

    void graphics_window::set_hidden(bool hide)
    {
        SDL_Window* const window = sdl_window(m_impl);

        if (hide)
        {
            SDL_HideWindow(window);
        }
        else
        {
            SDL_ShowWindow(window);
        }
    }

    void graphics_window::set_borderless(bool borderless)
    {
        if (m_style == window_style::app)
        {
            SDL_SetHintWithPriority("SDL_BORDERLESS_WINDOWED_STYLE", "0", SDL_HINT_OVERRIDE);
            SDL_SetHintWithPriority("SDL_BORDERLESS_RESIZABLE_STYLE",
                "0",
                SDL_HINT_OVERRIDE); // This seems to allow the snap features in Windows Aero
        }

        SDL_Window* const window = sdl_window(m_impl);
        SDL_SetWindowBordered(window, borderless ? SDL_FALSE : SDL_TRUE);

        if (m_style == window_style::app)
        {
            SDL_SetHintWithPriority("SDL_BORDERLESS_WINDOWED_STYLE", nullptr, SDL_HINT_OVERRIDE);
            SDL_SetHintWithPriority("SDL_BORDERLESS_RESIZABLE_STYLE", nullptr, SDL_HINT_OVERRIDE);
        }
    }

    namespace
    {
        SDL_HitTestResult hit_test(SDL_Window*, const SDL_Point* position, void* data)
        {
            auto& f = *static_cast<const hit_test_fn*>(data);
            const auto result = f(vec2u{u32(position->x), u32(position->y)});
            return to_sdl_hit_test_result(result);
        }
    }

    void graphics_window::set_custom_hit_test(const hit_test_fn* f)
    {
        SDL_Window* const window = sdl_window(m_impl);

        if (f)
        {
            SDL_SetWindowHitTest(window, hit_test, const_cast<void*>(static_cast<const void*>(f)));
        }
        else
        {
            SDL_SetWindowHitTest(window, nullptr, nullptr);
        }
    }

    vec2u graphics_window::get_size() const
    {
        int w, h;
        SDL_GetWindowSize(sdl_window(m_impl), &w, &h);
        return {u32(w), u32(h)};
    }

    native_window_handle graphics_window::get_native_handle() const
    {
        SDL_Window* const window = sdl_window(m_impl);
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);

        return SDL_GetWindowWMInfo(window, &wmInfo) ? wmInfo.info.win.window : nullptr;
    }

    void graphics_window::set_icon(u32 w, u32 h, std::span<const byte> data)
    {
        SDL_Window* const window = sdl_window(m_impl);

        SDL_Surface* const surface = SDL_CreateRGBSurfaceWithFormat(0, i32(w), i32(h), 8, SDL_PIXELFORMAT_RGBA32);
        std::memcpy(surface->pixels, data.data(), data.size());

        SDL_SetWindowIcon(window, surface);
        SDL_FreeSurface(surface);
    }

    void window_event_processor::set_event_dispatcher(const window_event_dispatcher& dispatcher)
    {
        m_windowEventDispatcher = dispatcher;
    }

    void window_event_processor::set_input_queue(input_queue* inputQueue)
    {
        m_inputQueue = inputQueue;
    }

    namespace
    {
        mouse_key sdl_map_mouse_key(u8 key)
        {
            switch (key)
            {
            case SDL_BUTTON_LEFT:
                return mouse_key::left;

            case SDL_BUTTON_RIGHT:
                return mouse_key::right;

            case SDL_BUTTON_MIDDLE:
                return mouse_key::middle;

            default:
                OBLO_ASSERT(false, "Unhandled mouse key");
                return mouse_key::enum_max;
            }
        }

        keyboard_key sdl_map_keyboard_key(SDL_Keycode key)
        {
            if (key >= 'a' && key <= 'z')
            {
                return keyboard_key(u32(keyboard_key::a) + (key - 'a'));
            }

            switch (key)
            {
            case SDLK_LSHIFT:
                return keyboard_key::left_shift;
            }

            return keyboard_key::enum_max;
        }

        time sdl_convert_time(u32 time)
        {
            return time::from_milliseconds(time);
        }
    }

    bool window_event_processor::process_events() const
    {
        for (SDL_Event event; SDL_PollEvent(&event);)
        {
            if (m_windowEventDispatcher.dispatch)
            {
                m_windowEventDispatcher.dispatch(&event);
            }

            if (m_inputQueue)
            {
                switch (event.type)
                {
                case SDL_MOUSEBUTTONDOWN:
                    m_inputQueue->push({
                        .kind = input_event_kind::mouse_press,
                        .timestamp = sdl_convert_time(event.button.timestamp),
                        .mousePress =
                            {
                                .key = sdl_map_mouse_key(event.button.button),
                            },
                    });
                    break;

                case SDL_MOUSEBUTTONUP:
                    m_inputQueue->push({
                        .kind = input_event_kind::mouse_release,
                        .timestamp = sdl_convert_time(event.button.timestamp),
                        .mouseRelease =
                            {
                                .key = sdl_map_mouse_key(event.button.button),
                            },
                    });
                    break;

                case SDL_MOUSEMOTION:
                    m_inputQueue->push({
                        .kind = input_event_kind::mouse_move,
                        .timestamp = sdl_convert_time(event.motion.timestamp),
                        .mouseMove =
                            {
                                .x = f32(event.motion.x),
                                .y = f32(event.motion.y),
                            },
                    });
                    break;

                case SDL_KEYDOWN:
                    m_inputQueue->push({
                        .kind = input_event_kind::keyboard_press,
                        .timestamp = sdl_convert_time(event.key.timestamp),
                        .keyboardPress =
                            {
                                .key = sdl_map_keyboard_key(event.key.keysym.sym),
                            },
                    });
                    break;

                case SDL_KEYUP:
                    m_inputQueue->push({
                        .kind = input_event_kind::keyboard_release,
                        .timestamp = sdl_convert_time(event.key.timestamp),
                        .keyboardRelease =
                            {
                                .key = sdl_map_keyboard_key(event.key.keysym.sym),
                            },
                    });

                    break;
                }
            }

            switch (event.type)
            {
            case SDL_QUIT:
                return false;

            case SDL_WINDOWEVENT: {
                switch (event.window.event)
                {
                case SDL_WINDOWEVENT_MAXIMIZED:
                case SDL_WINDOWEVENT_RESTORED: {
                    const auto [window, graphicsContext] = get_graphics_context(event);
                    graphicsContext->on_visibility_change(true);
                    break;
                }

                case SDL_WINDOWEVENT_MINIMIZED: {
                    const auto [window, graphicsContext] = get_graphics_context(event);
                    graphicsContext->on_visibility_change(false);
                    break;
                }

                case SDL_WINDOWEVENT_MOVED: {

                    const u32 x = u32(event.window.data1);
                    const u32 y = u32(event.window.data2);

                    (void) x;
                    (void) y;
                    break;
                }

                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED: {
                    const auto [window, graphicsContext] = get_graphics_context(event);

                    if (graphicsContext)
                    {
                        const u32 w = u32(event.window.data1);
                        const u32 h = u32(event.window.data2);

                        graphicsContext->on_resize(w, h);
                    }

                    break;
                }

                case SDL_WINDOWEVENT_CLOSE: {
                    SDL_Window* const sdlWindow = SDL_GetWindowFromID(event.window.windowID);
                    auto* const graphicsWindow =
                        sdlWindow ? static_cast<graphics_window*>(SDL_GetWindowData(sdlWindow, g_WindowPtr)) : nullptr;

                    if (graphicsWindow)
                    {
                        graphicsWindow->destroy();
                    }

                    break;
                }
                }
            }
            }
        }

        return true;
    }
}