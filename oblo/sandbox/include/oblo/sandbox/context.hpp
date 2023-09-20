#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <vulkan/vulkan.h>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    class vulkan_context;
    class render_pass_manager;
    struct texture;

    struct sandbox_init_context
    {
        vulkan_context* vkContext;
        frame_allocator* frameAllocator;
        VkFormat swapchainFormat;
        u32 width;
        u32 height;
    };

    struct sandbox_shutdown_context
    {
        vulkan_context* vkContext;
        frame_allocator* frameAllocator;
    };

    struct sandbox_render_context
    {
        vulkan_context* vkContext;
        frame_allocator* frameAllocator;
        h32<texture> swapchainTexture;
        u32 width;
        u32 height;
    };

    struct sandbox_update_imgui_context
    {
        vulkan_context* vkContext;
    };
}