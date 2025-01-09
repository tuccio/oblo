#include <oblo/vulkan/nodes/skybox_provider.hpp>

#include <oblo/resource/resource_ptr.hpp>
#include <oblo/scene/assets/texture.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    void skybox_provider::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::none);

        const auto& texturePtr = ctx.access(inSkyboxResource);

        auto& skybox = ctx.access(outSkyboxResidentTexture);

        if (texturePtr)
        {
            skybox = ctx.load_resource(texturePtr);
        }
        else
        {
            skybox = {};
        }
    }
}