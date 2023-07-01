#pragma once

#include <oblo/core/types.hpp>

namespace oblo::vk
{
    struct sandbox_render_context;
    struct sandbox_init_context;
    struct sandbox_shutdown_context;

    struct renderer_state
    {
        u32 lastFrameWidth, lastFrameHeight;
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