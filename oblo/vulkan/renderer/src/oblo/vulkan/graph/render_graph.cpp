#include <oblo/vulkan/graph/render_graph.hpp>

#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    render_graph::render_graph() = default;

    render_graph::render_graph(render_graph&&) noexcept = default;

    render_graph& render_graph::operator=(render_graph&&) noexcept = default;

    render_graph::~render_graph()
    {
        // TODO: Should this destroy textures?
        for (auto& node : m_nodes)
        {
            if (node.node && node.destruct)
            {
                node.destruct(node.node);
            }
        }

        for (auto& dataStorage : m_pinStorage)
        {
            if (dataStorage.ptr && dataStorage.destruct)
            {
                dataStorage.destruct(dataStorage.ptr);
            }
        }
    }

    void* render_graph::find_input(std::string_view name)
    {
        for (auto& input : m_inputs)
        {
            if (input.name == name)
            {
                return m_pinStorage[input.storageIndex].ptr;
            }
        }

        return nullptr;
    }

    void* render_graph::find_node(type_id type)
    {
        for (auto& node : m_nodes)
        {
            if (node.typeId == type)
            {
                return node.node;
            }
        }

        return nullptr;
    }

    u32 render_graph::get_backing_texture_id(resource<texture> virtualTextureId) const
    {
        return m_pins[virtualTextureId.value].storageIndex;
    }

    void render_graph::execute(const vulkan_context& ctx)
    {
        runtime_builder builder{*this};
        runtime_context runtime{*this, ctx.get_resource_manager()};

        for (auto& node : m_nodes)
        {
            auto* const ptr = node.node;

            if (node.build)
            {
                node.build(ptr, builder);
            }

            if (node.execute)
            {
                node.execute(ptr, runtime);
            }
        }
    }

    void* render_graph::access_data(u32 h) const
    {
        const auto storageIndex = m_pins[h].storageIndex;
        auto& data = m_pinStorage[storageIndex];

        return data.ptr;
    }
}