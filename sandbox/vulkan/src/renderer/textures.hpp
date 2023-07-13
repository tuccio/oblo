#pragma once

#include <oblo/vulkan/allocator.hpp>

namespace oblo::vk
{
    template <typename Context>
    void try_destroy_texture(const Context& context, h32<texture> handle)
    {
        if (handle)
        {
            const auto& texture = context.resourceManager->get(handle);
            context.allocator->destroy(allocated_image{texture.image, texture.allocation});

            if (texture.view)
            {
                destroy_device_object(context.engine->get_device(), texture.view);
            }
        }
    }
}