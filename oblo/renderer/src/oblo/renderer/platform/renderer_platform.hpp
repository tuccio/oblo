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
}