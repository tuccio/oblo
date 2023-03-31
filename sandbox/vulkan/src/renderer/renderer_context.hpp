#pragma once

#include <sandbox/context.hpp>

namespace oblo::vk
{
    union renderer_context {
        const sandbox_render_context* renderContext;
        const sandbox_init_context* initContext;
        const sandbox_shutdown_context* shutdownContext;
    };
}