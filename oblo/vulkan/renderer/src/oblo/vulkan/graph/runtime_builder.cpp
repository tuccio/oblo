#include <oblo/vulkan/graph/runtime_builder.hpp>

#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>

namespace oblo::vk
{
    void* runtime_builder::access_data(u32 virtualDataIndex) const
    {
        const auto storageIndex = m_graph->m_dataPins[virtualDataIndex].storageIndex;
        auto& data = m_graph->m_dataStorage[storageIndex];

        return data.get();
    }
}