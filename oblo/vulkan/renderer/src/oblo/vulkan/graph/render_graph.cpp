#include <oblo/vulkan/graph/render_graph.hpp>

#include <oblo/core/zip_range.hpp>
#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/resource_pool.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>
#include <oblo/vulkan/renderer.hpp>
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

    void render_graph::build(resource_pool& resourcePool)
    {
        runtime_builder builder{*this, resourcePool};

        m_nodeTransitions.assign(m_nodes.size(), node_transitions{});
        m_textureTransitions.clear();
        m_transientTextures.clear();
        m_resourcePoolId.assign(m_pinStorage.size(), ~u32{});

        u32 nodeIndex{0};

        for (auto& node : m_nodes)
        {
            auto* const ptr = node.node;

            if (node.build)
            {
                auto& nodeTransitions = m_nodeTransitions[nodeIndex];
                nodeTransitions.firstTextureTransition = u32(m_textureTransitions.size());

                node.build(ptr, builder);

                nodeTransitions.lastTextureTransition = u32(m_textureTransitions.size());
            }
        }
    }

    void render_graph::execute(renderer& renderer, resource_pool& resourcePool)
    {
        auto& resourceManager = renderer.get_resource_manager();
        auto& commandBuffer = renderer.get_active_command_buffer();

        for (const auto [resource, poolIndex] : m_transientTextures)
        {
            constexpr VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            const auto tex = resourcePool.get_texture(poolIndex);
            // TODO: Unregister them
            const auto handle = resourceManager.register_texture(tex, initialLayout);
            commandBuffer.set_starting_layout(handle, initialLayout);

            new (access_resource_storage(resource.value)) h32<texture>{handle};
        }

        runtime_context runtime{*this, renderer, commandBuffer.get()};

        for (auto&& [node, transitions] : zip_range(m_nodes, m_nodeTransitions))
        {
            auto* const ptr = node.node;

            for (u32 i = transitions.firstTextureTransition; i != transitions.lastTextureTransition; ++i)
            {
                const auto& textureTransition = m_textureTransitions[i];

                const auto* const texturePtr =
                    static_cast<h32<texture>*>(access_resource_storage(textureTransition.texture.value));
                OBLO_ASSERT(texturePtr && *texturePtr);

                commandBuffer.add_pipeline_barrier(resourceManager, *texturePtr, textureTransition.target);
            }

            if (node.execute)
            {
                node.execute(ptr, runtime);
            }
        }
    }

    void* render_graph::access_resource_storage(u32 h) const
    {
        const auto storageIndex = m_pins[h].storageIndex;
        auto& data = m_pinStorage[storageIndex];

        return data.ptr;
    }

    void render_graph::add_transient_resource(resource<texture> texture, u32 poolIndex)
    {
        m_transientTextures.emplace_back(texture, poolIndex);
        const u32 storageIndex = m_pins[texture.value].storageIndex;
        m_resourcePoolId[storageIndex] = poolIndex;
    }

    void render_graph::add_resource_transition(resource<texture> texture, VkImageLayout target)
    {
        m_textureTransitions.emplace_back(texture, target);
    }

    u32 render_graph::find_pool_index(resource<texture> texture) const
    {
        const u32 storageIndex = m_pins[texture.value].storageIndex;
        return m_resourcePoolId[storageIndex];
    }
}