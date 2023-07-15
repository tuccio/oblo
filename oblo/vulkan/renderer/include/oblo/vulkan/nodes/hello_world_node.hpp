#pragma once

#include <hello_world/hello_world.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <sandbox/context.hpp>

namespace oblo::vk
{
    struct hello_world_node
    {
        hello_world instance;

        bool initialize(renderer_context* renderContext)
        {
            return instance.init(*renderContext->initContext);
        }

        void shutdown(renderer_context* renderContext)
        {
            instance.shutdown(*renderContext->shutdownContext);
        }

        void execute(renderer_context* renderContext)
        {
            instance.update(*renderContext->renderContext);
        }
    };
}