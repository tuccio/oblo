#include <oblo/window/graphics_window.hpp>

#include <oblo/modules/module_manager.hpp>
#include <oblo/window/graphics_engine.hpp>
#include <oblo/window/graphics_window_context.hpp>

#include <SDL.h>
#include <SDL_syswm.h>

namespace oblo
{
    namespace
    {
        SDL_Window* sdl_window(void* impl)
        {
            return static_cast<SDL_Window*>(impl);
        }
    }

    graphics_window::graphics_window() = default;

    graphics_window::graphics_window(graphics_window&& other) noexcept
    {
        m_impl = other.m_impl;
        m_graphicsContext = other.m_graphicsContext;
        other.m_impl = nullptr;
        other.m_graphicsContext = nullptr;
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
        other.m_impl = nullptr;
        other.m_graphicsContext = nullptr;

        return *this;
    }

    bool graphics_window::create(const graphics_window_initializer& initializer)
    {
        OBLO_ASSERT(!m_impl);

        i32 windowFlags = SDL_WINDOW_RESIZABLE;

        if (initializer.isMaximized)
        {
            windowFlags |= SDL_WINDOW_MAXIMIZED;
        }

        if (!initializer.isHidden)
        {
            windowFlags |= SDL_WINDOW_SHOWN;
        }

        const i32 w = initializer.windowWidth == 0 ? 1280u : initializer.windowWidth;
        const i32 h = initializer.windowHeight == 0 ? 720u : initializer.windowHeight;

        SDL_Window* const window = SDL_CreateWindow(initializer.title.c_str(),
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            w,
            h,
            windowFlags);

        m_impl = window;

        return m_impl != nullptr;
    }

    void graphics_window::destroy()
    {
        if (m_impl)
        {
            SDL_DestroyWindow(sdl_window(m_impl));
            m_impl = nullptr;
        }

        if (m_graphicsContext)
        {
            m_graphicsContext->on_destroy();
            m_graphicsContext = nullptr;
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

        m_graphicsContext = gfxEngine->create_context(*this);
        return gfxEngine != nullptr;
    }

    bool graphics_window::is_ready() const
    {
        return m_impl && m_graphicsContext;
    }

    bool graphics_window::is_open() const
    {
        return m_impl;
    }

    bool graphics_window::is_visible() const
    {
        const auto flags = SDL_GetWindowFlags(sdl_window(m_impl));
        return (flags & SDL_WINDOW_SHOWN) != 0;
    }

    void graphics_window::set_visible(bool visible)
    {
        SDL_Window* const window = sdl_window(m_impl);

        if (visible)
        {
            SDL_ShowWindow(window);
        }
        else
        {
            SDL_HideWindow(window);
        }
    }

    vec2u graphics_window::get_size() const
    {
        int w, h;
        SDL_GetWindowSize(sdl_window(m_impl), &w, &h);
        return {u32(w), u32(h)};
    }

    void graphics_window::update()
    {
        OBLO_ASSERT(is_ready());

        // TODO: Poll events
    }

    native_window_handle graphics_window::get_native_handle() const
    {
        SDL_Window* const window = sdl_window(m_impl);
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);

        return SDL_GetWindowWMInfo(window, &wmInfo) ? wmInfo.info.win.window : nullptr;
    }
}