#include <oblo/renderer/nodes/providers/skybox_provider.hpp>

#include <oblo/resource/resource_ptr.hpp>
#include <oblo/scene/resources/texture.hpp>
#include <oblo/renderer/graph/node_common.hpp>

namespace oblo
{
    namespace
    {
        struct skybox_settings_buffer
        {
            vec3 multiplier;
            h32<resident_texture> texture;
        };
    }

    void skybox_provider::build(const frame_graph_build_context& ctx)
    {
        ctx.empty_pass();

        const auto& texturePtr = ctx.access(inSkyboxResource);
        const auto& settings = ctx.access(inSkyboxSettings);

        const skybox_settings_buffer content{
            .multiplier = settings.multiplier,
            .texture = ctx.load_resource(texturePtr),
        };

        ctx.create(outSkyboxSettingsBuffer,
            buffer_resource_initializer{
                .size = sizeof(skybox_settings_buffer),
                .data = as_bytes(std::span{&content, 1}),
            },
            buffer_usage::uniform);
    }
}