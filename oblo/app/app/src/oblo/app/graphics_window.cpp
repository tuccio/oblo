#include <oblo/app/graphics_window.hpp>

#include <oblo/app/graphics_engine.hpp>
#include <oblo/app/graphics_window_context.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/modules/module_manager.hpp>

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

        constexpr const char* g_WindowGraphicsContext = "oblo::gfx";
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

        i32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

        if (initializer.isMaximized)
        {
            windowFlags |= SDL_WINDOW_MAXIMIZED;
        }

        if (initializer.isHidden)
        {
            windowFlags |= SDL_WINDOW_HIDDEN;
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

    bool window_event_processor::process_events() const
    {
        for (SDL_Event event; SDL_PollEvent(&event);)
        {
            if (m_windowEventDispatcher.dispatch)
            {
                m_windowEventDispatcher.dispatch(&event);
            }

            switch (event.type)
            {
            case SDL_QUIT:
                return false;

            case SDL_WINDOWEVENT: {
                SDL_Window* const window = SDL_GetWindowFromID(event.window.windowID);
                void* const windowData = SDL_GetWindowData(window, g_WindowGraphicsContext);
                auto* const graphicsContext = static_cast<graphics_window_context*>(windowData);

                if (graphicsContext)
                {
                    switch (event.window.event)
                    {
                    case SDL_WINDOWEVENT_MAXIMIZED:
                    case SDL_WINDOWEVENT_RESTORED:
                        break;

                    case SDL_WINDOWEVENT_MINIMIZED:
                        break;

                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED: {
                        const u32 w = u32(event.window.data1);
                        const u32 h = u32(event.window.data2);

                        graphicsContext->on_resize(w, h);
                        break;
                    }
                    }
                }
            }
            }
        }

        return true;
    }
}