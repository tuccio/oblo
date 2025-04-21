#include <oblo/app/graphics_engine.hpp>
#include <oblo/app/graphics_window.hpp>
#include <oblo/app/graphics_window_context.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/modules/module_manager.hpp>

#include <Windows.h>
#include <Windowsx.h>

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

        template private_accessor<&graphics_window::m_graphicsContext, &graphics_window::m_hitTest>;

        LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
        {
            graphics_window* const window = std::bit_cast<graphics_window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

            switch (uMsg)
            {
            case WM_SIZE:
                if (window && get_graphics_window_context(window))
                {
                    const UINT width = LOWORD(lParam);
                    const UINT height = HIWORD(lParam);

                    graphics_window_context* const graphicsCtx = get_graphics_window_context(window);
                    graphicsCtx->on_resize(width, height);
                }
                return 0;

            case WM_CLOSE:
                if (window)
                {
                    window->destroy();
                }
                return 0;

            case WM_NCCALCSIZE: {
                const auto style = GetWindowStyle(hWnd);

                // Check whether we have window_style::app borderless (i.e. WS_CAPTION should not be set, but
                // WS_POPUP | WS_THICKFRAME should be)
                constexpr auto expected = WS_POPUP | WS_THICKFRAME;
                constexpr auto check = expected | WS_CAPTION;

                if ((style & check) == expected)
                {
                    // Our borderless windows have an invisible frame in order to be resizable and enable the Windows
                    // aero snap features. This border is 7x7. There might also be an extra border when WS_BORDER is
                    // there, making it 8x8.
                    // When maximized the invisible border is out of screen (e.g. on the main display the window
                    // position will be [-7;-7], we need to make sure to offset the client area to avoid clipping the
                    // border.

                    if (IsZoomed(hWnd))
                    {
                        constexpr i32 borderSize = 7;

                        NCCALCSIZE_PARAMS* const params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);

                        params->rgrc->right -= borderSize;
                        params->rgrc->left += borderSize;

                        params->rgrc->bottom -= borderSize;
                        params->rgrc->top += borderSize;
                    }

                    return 0;
                }
            }

            case WM_NCHITTEST: {
                // Let the default procedure handle resizing areas
                const LRESULT hit = DefWindowProc(hWnd, uMsg, wParam, lParam);
                switch (hit)
                {
                case HTNOWHERE:
                case HTRIGHT:
                case HTLEFT:
                case HTTOPLEFT:
                case HTTOP:
                case HTTOPRIGHT:
                case HTBOTTOMRIGHT:
                case HTBOTTOM:
                case HTBOTTOMLEFT: {
                    return hit;
                }
                }

                if (auto& hitTest = get_graphics_window_hit_test(window))
                {
                    POINT p = {
                        .x = GET_X_LPARAM(lParam),
                        .y = GET_Y_LPARAM(lParam),
                    };

                    ScreenToClient(hWnd, &p);

                    switch (hitTest({u32(p.x), u32(p.y)}))
                    {
                    case hit_test_result::normal:
                        return HTCLIENT;
                    case hit_test_result::draggable:
                        return HTCAPTION;
                    case hit_test_result::resize_top_left:
                        return HTTOPLEFT;
                    case hit_test_result::resize_top:
                        return HTTOP;
                    case hit_test_result::resize_top_right:
                        return HTTOPRIGHT;
                    case hit_test_result::resize_right:
                        return HTRIGHT;
                    case hit_test_result::resize_bottom_right:
                        return HTBOTTOMRIGHT;
                    case hit_test_result::resize_bottom:
                        return HTBOTTOM;
                    case hit_test_result::resize_bottom_left:
                        return HTBOTTOMLEFT;
                    case hit_test_result::resize_left:
                        return HTLEFT;
                    }
                }

                return HTCLIENT;
            }
            }

            return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }

        class win32_window_class
        {
        public:
            static constexpr const char* class_name = "oblo::graphics_window";

            win32_window_class()
            {
                m_wc = {
                    .lpfnWndProc = WindowProc,
                    .hInstance = GetModuleHandle(nullptr),
                    .lpszClassName = class_name,
                };

                RegisterClass(&m_wc);
            }

            ~win32_window_class()
            {
                UnregisterClass(m_wc.lpszClassName, m_wc.hInstance);
            }

        private:
            WNDCLASS m_wc{};
        };

        static win32_window_class g_wndClass;
    }

    graphics_window::graphics_window() = default;

    graphics_window::~graphics_window()
    {
        destroy();
    }

    bool graphics_window::create(const graphics_window_initializer& initializer)
    {
        OBLO_ASSERT(!m_impl);

        DWORD style;

        if (initializer.isBorderless)
        {
            switch (initializer.style)
            {
            case window_style::app:
                style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
                break;

            default:
                style = WS_POPUP;
            }
        }
        else
        {
            style = WS_OVERLAPPEDWINDOW;
        }

        if (initializer.isMaximized)
        {
            style |= WS_MAXIMIZE;
        }

        const auto w = initializer.windowWidth ? initializer.windowWidth : 1280;
        const auto h = initializer.windowHeight ? initializer.windowHeight : 720;

        const HWND hWnd = CreateWindowExA(0,
            g_wndClass.class_name,
            // We should actually convert the title from utf8 to utf16 and use CreateWindowExW
            initializer.title.c_str(),
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            w,
            h,
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr);

        if (!hWnd)
        {
            return false;
        }

        SetWindowLongPtr(hWnd, GWLP_USERDATA, std::bit_cast<LONG_PTR>(this));

        m_impl = hWnd;
        m_style = initializer.style;

        set_hidden(initializer.isHidden);

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
            DestroyWindow(std::bit_cast<HWND>(m_impl));
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
        return IsZoomed(std::bit_cast<HWND>(m_impl)) != 0;
    }

    bool graphics_window::is_minimized() const
    {
        return IsIconic(std::bit_cast<HWND>(m_impl)) != 0;
    }

    void graphics_window::maximize()
    {
        ShowWindow(std::bit_cast<HWND>(m_impl), SW_MAXIMIZE);
    }

    void graphics_window::minimize()
    {
        ShowWindow(std::bit_cast<HWND>(m_impl), SW_MINIMIZE);
    }

    void graphics_window::restore()
    {
        ShowWindow(std::bit_cast<HWND>(m_impl), SW_RESTORE);
    }

    bool graphics_window::is_hidden() const
    {
        return IsWindowVisible(std::bit_cast<HWND>(m_impl)) == 0;
    }

    void graphics_window::set_hidden(bool hide)
    {
        ShowWindow(std::bit_cast<HWND>(m_impl), hide ? SW_HIDE : SW_SHOW);
    }

    void graphics_window::set_custom_hit_test(const hit_test_fn* f)
    {
        m_hitTest = *f;
    }

    vec2u graphics_window::get_size() const
    {
        RECT rect;
        GetClientRect(std::bit_cast<HWND>(m_impl), &rect);
        return {static_cast<u32>(rect.right - rect.left), static_cast<u32>(rect.bottom - rect.top)};
    }

    native_window_handle graphics_window::get_native_handle() const
    {
        return m_impl;
    }

    void graphics_window::set_icon(u32 w, u32 h, std::span<const byte> data)
    {
        (void) h;
        (void) w;
        (void) data;
        // Implementation for setting window icon using Win32 API
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
        mouse_key win32_map_mouse_key(u8 key)
        {
            switch (key)
            {
            case VK_LBUTTON:
                return mouse_key::left;

            case VK_RBUTTON:
                return mouse_key::right;

            case VK_MBUTTON:
                return mouse_key::middle;

            default:
                OBLO_ASSERT(false, "Unhandled mouse key");
                return mouse_key::enum_max;
            }
        }

        keyboard_key win32_map_keyboard_key(WPARAM key)
        {
            if (key >= 'A' && key <= 'Z')
            {
                return keyboard_key(u32(keyboard_key::a) + (key - 'A'));
            }

            switch (key)
            {
            case VK_SHIFT:
                return keyboard_key::left_shift;
            }

            return keyboard_key::enum_max;
        }

        time win32_convert_time(DWORD time)
        {
            return time::from_milliseconds(i64(time));
        }
    }

    bool window_event_processor::process_events() const
    {
        MSG msg;

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                return false;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (m_windowEventDispatcher.dispatch)
            {
                m_windowEventDispatcher.dispatch(&msg);
            }

            if (m_inputQueue)
            {
                switch (msg.message)
                {
                case WM_LBUTTONDOWN:
                    m_inputQueue->push({
                        .kind = input_event_kind::mouse_press,
                        .timestamp = win32_convert_time(msg.time),
                        .mousePress =
                            {
                                .key = win32_map_mouse_key(VK_LBUTTON),
                            },
                    });
                    break;

                case WM_LBUTTONUP:
                    m_inputQueue->push({
                        .kind = input_event_kind::mouse_release,
                        .timestamp = win32_convert_time(msg.time),
                        .mouseRelease =
                            {
                                .key = win32_map_mouse_key(VK_LBUTTON),
                            },
                    });
                    break;

                case WM_MOUSEMOVE:
                    m_inputQueue->push({
                        .kind = input_event_kind::mouse_move,
                        .timestamp = win32_convert_time(msg.time),
                        .mouseMove =
                            {
                                .x = f32(GET_X_LPARAM(msg.lParam)),
                                .y = f32(GET_Y_LPARAM(msg.lParam)),
                            },
                    });
                    break;

                case WM_KEYDOWN:
                    m_inputQueue->push({
                        .kind = input_event_kind::keyboard_press,
                        .timestamp = win32_convert_time(msg.time),
                        .keyboardPress =
                            {
                                .key = win32_map_keyboard_key(msg.wParam),
                            },
                    });
                    break;

                case WM_KEYUP:
                    m_inputQueue->push({
                        .kind = input_event_kind::keyboard_release,
                        .timestamp = win32_convert_time(msg.time),
                        .keyboardRelease =
                            {
                                .key = win32_map_keyboard_key(msg.wParam),
                            },
                    });

                    break;
                }
            }
        }

        return true;
    }
}