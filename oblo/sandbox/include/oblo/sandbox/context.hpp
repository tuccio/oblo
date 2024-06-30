#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <vulkan/vulkan.h>

namespace oblo
{
    class input_queue;
}

namespace oblo::vk
{
    class vulkan_context;
    class pass_manager;
    struct texture;

    struct sandbox_startup_context
    {
        vulkan_context* vkContext;
        const input_queue* inputQueue;
        VkFormat swapchainFormat;
        u32 width;
        u32 height;
    };

    struct sandbox_shutdown_context
    {
        vulkan_context* vkContext;
    };

    struct sandbox_render_context
    {
        vulkan_context* vkContext;
        h32<texture> swapchainTexture;
        u32 width;
        u32 height;
    };

    struct sandbox_update_imgui_context
    {
        vulkan_context* vkContext;
    };
}