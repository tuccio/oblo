#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/ui/texture.hpp>

namespace oblo::ui
{
    struct texture_storage
    {
        h32_flat_pool_dense_map<texture, texture> textures;

        dynamic_array<texture_command> commands;

        void begin_frame();

        h32<texture> create_texture(u32 width, u32 height, texture_format format);

        texture* find_texture(h32<texture> id);

        void notify_upload_required(h32<texture> id, u32 x, u32 y, u32 width, u32 height);
    };
}