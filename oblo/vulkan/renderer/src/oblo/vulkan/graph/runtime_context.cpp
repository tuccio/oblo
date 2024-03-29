#include <oblo/vulkan/graph/runtime_context.hpp>

#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>
#include <oblo/vulkan/resource_manager.hpp>

namespace oblo::vk
{
    texture runtime_context::access(resource<texture> h) const
    {
        const auto textureHandle = *static_cast<h32<texture>*>(m_graph->access_resource_storage(h.value));

        texture result{};

        if (auto* const texture = get_resource_manager().try_find(textureHandle))
        {
            result = *texture;
        }

        return result;
    }

    buffer runtime_context::access(resource<buffer> h) const
    {
        return *static_cast<buffer*>(m_graph->access_resource_storage(h.value));
    }

    void* runtime_context::access_resource_storage(u32 index) const
    {
        return m_graph->access_resource_storage(index);
    }
}