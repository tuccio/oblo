#include <oblo/ui/texture_storage.hpp>

#include <oblo/core/utility.hpp>

namespace oblo::ui
{
    namespace
    {
        u32 get_row_pitch(texture_format format, u32 width)
        {
            constexpr u32 packing = 4u;

            switch (format)
            {
            case texture_format::r8_unorm:
                return round_up_multiple(width, packing);

            default:
                return 0;
            }
        }
    }

    void texture_storage::begin_frame()
    {
        commands.clear();
    }

    h32<texture> texture_storage::create_texture(u32 width, u32 height, texture_format format)
    {
        const u32 rowPitch = get_row_pitch(format, width);

        if (rowPitch <= 0)
        {
            return {};
        }

        const auto [it, key] = textures.emplace();

        if (key)
        {
            it->id = key;
            it->width = width;
            it->height = height;
            it->format = format;
            it->rowPitch = rowPitch;

            it->data.resize_default(rowPitch * height);
        }

        return key;
    }

    texture* texture_storage::find_texture(h32<texture> id)
    {
        return textures.try_find(id);
    }

    void texture_storage::notify_upload_required(h32<texture> id, u32 x, u32 y, u32 width, u32 height)
    {
        commands.push_back({
            .kind = texture_command_kind::update,
            .update =
                {
                    .id = id,
                    .x = x,
                    .y = y,
                    .width = width,
                    .height = height,
                },
        });
    }
}