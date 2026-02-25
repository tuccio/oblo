#pragma once

#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/vulkan/vulkan_instance.hpp>
#include <oblo/renderer/draw/pass_manager.hpp>
#include <oblo/renderer/draw/resource_cache.hpp>
#include <oblo/renderer/draw/texture_registry.hpp>

namespace oblo
{
    class renderer;

    struct renderer_platform
    {
        gpu::vk::vulkan_instance* vk{};
        texture_registry textureRegistry;
        resource_cache resourceCache;
        pass_manager passManager;
    };

    struct frame_graph_build_args
    {
        gpu::vk::vulkan_instance& vk;
        texture_registry& textureRegistry;
        resource_cache& resourceCache;
        pass_manager& passManager;
        renderer& r;
    };

    struct frame_graph_execute_args
    {
        gpu::vk::vulkan_instance& vk;
        texture_registry& textureRegistry;
        resource_cache& resourceCache;
        pass_manager& passManager;
        renderer& r;
        const gpu::staging_buffer& stagingBuffer;
    };

    struct frame_graph_texture_impl
    {
        VkImage image;
        VkImageView view;
        gpu::image_descriptor descriptor;
        h32<gpu::image> handle;
    };

    struct frame_graph_buffer_impl
    {
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
        h32<gpu::buffer> handle;
    };
}