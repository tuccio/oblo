#include <oblo/core/handle.hpp>

namespace oblo::vk
{
    class vulkan_context;
    struct texture;

    void destroy_texture(vulkan_context& ctx, h32<texture> texture);

    inline void reset_texture(vulkan_context& ctx, h32<texture>& texture)
    {
        if (texture)
        {
            destroy_texture(ctx, texture);
            texture = {};
        }
    }
}