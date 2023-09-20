#include <oblo/vulkan/destroy_resources.hpp>

#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    void destroy_texture(vulkan_context& ctx, h32<texture> handle)
    {
        auto& rm = ctx.get_resource_manager();
        const auto& texture = rm.get(handle);

        if (texture.view)
        {
            destroy_device_object(ctx.get_device(), texture.view);
        }

        ctx.get_allocator().destroy(allocated_image{texture.image, texture.allocation});
    }
}