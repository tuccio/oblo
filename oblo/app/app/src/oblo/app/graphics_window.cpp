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
        // This is used to handle WM_MOVE when moving maximized windows across different monitors
        bool g_MaybeMovingToNewMonitor = false;

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

        bool is_app_style_borderless(DWORD style)
        {
            // Check whether we have window_style::app borderless (i.e. WS_CAPTION should not be set, but
            // WS_POPUP | WS_THICKFRAME should be)
            constexpr auto expected = WS_POPUP | WS_THICKFRAME;
            constexpr auto check = expected | WS_CAPTION;

            return (style & check) == expected;
        }

        LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
        {
            // Not sure yet if this can be obtained somehow from the Win32 API, maybe dwmapi has something
            static constexpr i32 invisibleBorderSize = 7;

            graphics_window* const window = std::bit_cast<graphics_window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

            switch (uMsg)
            {
            case WM_GETMINMAXINFO:
                if (const auto style = GetWindowStyle(hWnd); is_app_style_borderless(style))
                {
                    // Clamp the size of our borderless window to the work area, this way we don't render the window
                    // over the task bar, which is not desirable in the app style.
                    const HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

                    MONITORINFO monitorInfo{
                        .cbSize = sizeof(MONITORINFO),
                    };

                    RECT rect;

                    if (GetMonitorInfo(monitor, &monitorInfo))
                    {
                        rect = monitorInfo.rcWork;
                    }
                    else
                    {
                        SystemParametersInfo(SPI_GETWORKAREA, sizeof(RECT), &rect, 0);
                    }

                    auto* const minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);

                    minMaxInfo->ptMaxSize.x = invisibleBorderSize + rect.right - rect.left;
                    minMaxInfo->ptMaxSize.y = invisibleBorderSize + rect.bottom - rect.top;
                    return 0;
                }

                break;

            case WM_MOVE: {
                // When moving the window between different monitors using windows key + arrow, we want to make sure we
                // resize the window to match the screen size.
                // In order to do it we process WM_MOVE, but with extra care to avoid reprocessing WM_MOVE recursively,
                // using a global flag.
                if (const auto style = GetWindowStyle(hWnd);
                    !g_MaybeMovingToNewMonitor && window && is_app_style_borderless(style) && IsZoomed(hWnd))
                {
                    g_MaybeMovingToNewMonitor = true;
                    window->restore();
                    window->maximize();
                    g_MaybeMovingToNewMonitor = false;
                }
                break;
            }

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

                if (const auto style = GetWindowStyle(hWnd); is_app_style_borderless(style))
                {
                    // Our borderless windows have an invisible frame in order to be resizable and enable the Windows
                    // aero snap features. This border seems to be 7x7.
                    // When maximized the invisible border is out of screen (e.g. on the main display the window
                    // position will be [-7;-7], we need to make sure to offset the client area to avoid clipping the
                    // border.

                    if (IsZoomed(hWnd))
                    {

                        NCCALCSIZE_PARAMS* const params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);

                        params->rgrc->left += invisibleBorderSize;
                        params->rgrc->top += invisibleBorderSize;
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

    graphics_window::graphics_window(graphics_window&& other) noexcept
    {
        m_impl = other.m_impl;
        m_graphicsContext = other.m_graphicsContext;
        m_hitTest = other.m_hitTest;

        other.m_impl = nullptr;
        m_hitTest = {};
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

        return *this;
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

    void graphics_window::set_custom_hit_test(const hit_test_fn& f)
    {
        m_hitTest = f;
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
        using bitmap_ptr = unique_ptr<HBITMAP__, decltype([](HBITMAP h) { DeleteObject(h); })>;

        const HWND hWnd = std::bit_cast<HWND>(m_impl);

        dynamic_array<u32> pixels;
        pixels.resize_default(data.size() / 4);

        for (u32 i = 0, j = 0; i < data.size(); ++j, i += 4)
        {
            const u32 r = u32(data[i]);
            const u32 g = u32(data[i + 1]);
            const u32 b = u32(data[i + 2]);
            const u32 a = u32(data[i + 3]);
            pixels[j] = b | (g << 8) | (r << 16) | (a << 24);
        }

        BITMAPV5HEADER bi = {
            .bV5Size = sizeof(BITMAPV5HEADER),
            .bV5Width = static_cast<LONG>(w),
            .bV5Height = -static_cast<LONG>(h), // Negative for top-down DI
            .bV5Planes = 1,
            .bV5BitCount = 32,
            .bV5Compression = BI_RGB,
        };

        const HDC hDC = GetDC(nullptr);

        if (!hDC)
        {
            return;
        }

        const bitmap_ptr hColor{CreateDIBitmap(hDC,
            reinterpret_cast<BITMAPINFOHEADER*>(&bi),
            CBM_INIT,
            pixels.data(),
            reinterpret_cast<BITMAPINFO*>(&bi),
            DIB_RGB_COLORS)};

        ReleaseDC(nullptr, hDC);

        const bitmap_ptr hMask{CreateBitmap(w, h, 1, 1, nullptr)};

        if (!hColor || !hMask)
        {
            return;
        }

        ICONINFO iconInfo = {
            .fIcon = TRUE,
            .hbmMask = hMask.get(),
            .hbmColor = hColor.get(),
        };

        HICON hIcon = CreateIconIndirect(&iconInfo);

        if (hIcon)
        {
            SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
            SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
        }
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
        constexpr mouse_key win32_map_mouse_key(u8 key)
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

                case WM_RBUTTONDOWN:
                    m_inputQueue->push({
                        .kind = input_event_kind::mouse_press,
                        .timestamp = win32_convert_time(msg.time),
                        .mousePress =
                            {
                                .key = win32_map_mouse_key(VK_RBUTTON),
                            },
                    });
                    break;

                case WM_RBUTTONUP:
                    m_inputQueue->push({
                        .kind = input_event_kind::mouse_release,
                        .timestamp = win32_convert_time(msg.time),
                        .mouseRelease =
                            {
                                .key = win32_map_mouse_key(VK_RBUTTON),
                            },
                    });
                    break;

                case WM_MBUTTONDOWN:
                    m_inputQueue->push({
                        .kind = input_event_kind::mouse_press,
                        .timestamp = win32_convert_time(msg.time),
                        .mousePress =
                            {
                                .key = win32_map_mouse_key(VK_MBUTTON),
                            },
                    });
                    break;

                case WM_MBUTTONUP:
                    m_inputQueue->push({
                        .kind = input_event_kind::mouse_release,
                        .timestamp = win32_convert_time(msg.time),
                        .mouseRelease =
                            {
                                .key = win32_map_mouse_key(VK_MBUTTON),
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