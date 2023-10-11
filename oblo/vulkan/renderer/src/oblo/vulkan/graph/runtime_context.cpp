#include <oblo/vulkan/graph/runtime_context.hpp>

#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>

namespace oblo::vk
{
    texture runtime_context::access(resource<texture> h) const
    {
        const auto storageIndex = m_graph->m_texturePins[h.value].storageIndex;
        const auto textureHandle = m_graph->m_textureResources[storageIndex];

        texture result{};

        if (auto* const texture = m_resourceManager->try_find(textureHandle))
        {
            result = *texture;
        }

        return result;
    }

    void* runtime_context::access_data(u32 h) const
    {
        const auto storageIndex = m_graph->m_dataPins[h].storageIndex;
        auto& data = m_graph->m_dataStorage[storageIndex];

        return data.get();
    }
}