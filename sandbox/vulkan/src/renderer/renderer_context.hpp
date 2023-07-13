#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    class stringInterner;
}

namespace oblo::vk
{
    class render_pass_manager;

    struct sandbox_render_context;
    struct sandbox_init_context;
    struct sandbox_shutdown_context;

    struct renderer_state
    {
        u32 lastFrameWidth;
        u32 lastFrameHeight;
        string_interner* stringInterner;
        render_pass_manager* renderPassManager;
    };

    struct renderer_context
    {
        union {
            const sandbox_render_context* renderContext;
            const sandbox_init_context* initContext;
            const sandbox_shutdown_context* shutdownContext;
        };

        renderer_state state;
    };
}