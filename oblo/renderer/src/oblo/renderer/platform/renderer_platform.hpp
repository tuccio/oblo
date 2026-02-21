#pragma once

#include <oblo/gpu/vulkan/vulkan_instance.hpp>
#include <oblo/renderer/draw/pass_manager.hpp>
#include <oblo/renderer/draw/resource_cache.hpp>
#include <oblo/renderer/draw/texture_registry.hpp>

namespace oblo
{
    struct renderer_platform
    {
        gpu::vk::vulkan_instance* vk{};
        texture_registry textureRegistry;
        resource_cache resourceCache;
        pass_manager passManager;
    };

    struct frame_graph_build_args : renderer_platform
    {
    };

    struct frame_graph_execute_args : renderer_platform
    {
    };

    struct frame_graph_texture
    {
        VkImage image;
        VkImageView view;
    };

    struct frame_graph_buffer
    {
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
    };
}