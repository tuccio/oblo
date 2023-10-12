#include <oblo/vulkan/graph/runtime_context.hpp>

#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>

namespace oblo::vk
{
    texture runtime_context::access(resource<texture> h) const
    {
        const auto textureHandle = *static_cast<h32<texture>*>(m_graph->access_data(h.value));

        texture result{};

        if (auto* const texture = m_resourceManager->try_find(textureHandle))
        {
            result = *texture;
        }

        return result;
    }
}