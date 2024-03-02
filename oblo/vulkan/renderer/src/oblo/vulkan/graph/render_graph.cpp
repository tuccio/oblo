#include <oblo/vulkan/graph/render_graph.hpp>

#include <oblo/core/zip_range.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/init_context.hpp>
#include <oblo/vulkan/graph/resource_pool.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    struct render_graph::pending_copy
    {
        h32<texture> target;
        u32 sourceStorageIndex;
        VkImageLayout transitionAfterCopy;
    };

    struct render_graph::pending_upload
    {
        resource<buffer> target;
        staging_buffer_span source;
    };

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

    void* render_graph::find_output(std::string_view name)
    {
        for (auto& output : m_outputs)
        {
            if (output.name == name)
            {
                return m_pinStorage[output.storageIndex].ptr;
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

    void render_graph::init(renderer& renderer)
    {
        m_staticPinCount = u32(m_pins.size());
        m_staticPinStorageCount = u32(m_pinStorage.size());

        // TODO: Hardcoded max size
        m_dynamicAllocator = std::make_unique<frame_allocator>();
        m_dynamicAllocator->init(4 << 20);

        const init_context context{renderer};

        for (auto& node : m_nodes)
        {
            if (node.init)
            {
                node.init(node.node, context);
            }
        }
    }

    void render_graph::build(renderer& renderer, resource_pool& resourcePool)
    {
        destroy_dynamic_pins();

        m_nodeTransitions.assign(m_nodes.size(), node_transitions{});
        m_textureTransitions.clear();
        m_transientTextures.clear();
        m_resourcePoolId.assign(m_pinStorage.size(), ~u32{});

        m_dynamicAllocator->restore_all();

        u32 nodeIndex{0};

        const runtime_builder builder{*this, resourcePool, renderer};

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

        for (const auto& pendingCopy : m_pendingCopies)
        {
            const u32 poolIndex = m_resourcePoolId[pendingCopy.sourceStorageIndex];
            resourcePool.add_usage(poolIndex, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        }
    }

    void render_graph::execute(renderer& renderer, resource_pool& resourcePool)
    {
        auto& resourceManager = renderer.get_resource_manager();
        auto& commandBuffer = renderer.get_active_command_buffer();

        for (const auto [resource, poolIndex] : m_transientBuffers)
        {
            const auto buf = resourcePool.get_buffer(poolIndex);
            new (access_resource_storage(resource.value)) buffer{buf};
        }

        if (!m_pendingUploads.empty())
        {
            flush_uploads(renderer.get_staging_buffer());
        }

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

        if (!m_pendingCopies.empty())
        {
            auto& vkCtx = renderer.get_vulkan_context();
            vkCtx.begin_debug_label(commandBuffer.get(), "render_graph::flush_copies");
            flush_copies(commandBuffer, resourceManager);
            vkCtx.end_debug_label(commandBuffer.get());
        }
    }

    void render_graph::flush_copies(stateful_command_buffer& commandBuffer, resource_manager& resourceManager)
    {
        for (const auto [target, storageIndex, transitionAfterCopy] : m_pendingCopies)
        {
            const h32<texture> source = *reinterpret_cast<h32<texture>*>(m_pinStorage[storageIndex].ptr);

            const texture& sourceTex = resourceManager.get(source);
            const texture& targetTex = resourceManager.get(target);

            // TODO: Fix hardcoded aspect
            const VkImageCopy copy{
                .srcSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .layerCount = 1,
                    },
                .dstSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .layerCount = 1,
                    },
                .extent = sourceTex.initializer.extent,
            };

            commandBuffer.add_pipeline_barrier(resourceManager, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            commandBuffer.add_pipeline_barrier(resourceManager, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            vkCmdCopyImage(commandBuffer.get(),
                sourceTex.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                targetTex.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copy);

            if (transitionAfterCopy != VK_IMAGE_LAYOUT_UNDEFINED)
            {
                commandBuffer.add_pipeline_barrier(resourceManager, target, transitionAfterCopy);
            }
        }

        m_pendingCopies.clear();
    }

    void render_graph::flush_uploads(staging_buffer& stagingBuffer)
    {
        for (const auto& upload : m_pendingUploads)
        {
            const auto* const b = reinterpret_cast<buffer*>(access_resource_storage(upload.target.value));
            stagingBuffer.upload(upload.source, b->buffer, b->offset);
        }

        m_pendingUploads.clear();
    }

    u32 render_graph::allocate_dynamic_resource_pin()
    {
        const auto handle = u32(m_pins.size());
        const auto storageIndex = u32(m_pinStorage.size());

        m_pins.emplace_back(storageIndex);

        m_pinStorage.push_back({.ptr = m_dynamicAllocator->allocate(sizeof(u32), alignof(u32))});

        return handle;
    }

    void render_graph::destroy_dynamic_pins()
    {
        for (usize i = m_staticPinStorageCount; i < m_pinStorage.size(); ++i)
        {
            const auto& storage = m_pinStorage[i];

            if (storage.destruct)
            {
                storage.destruct(storage.ptr);
            }
        }

        m_pinStorage.resize(m_staticPinStorageCount);
        m_pins.resize(m_staticPinCount);
    }

    bool render_graph::copy_output(std::string_view name, h32<texture> target, VkImageLayout transitionAfterCopy)
    {
        u32 storageIndex = find_output_storage_index(name);

        if (storageIndex == 0)
        {
            return false;
        }

        m_pendingCopies.emplace_back(target, storageIndex, transitionAfterCopy);
        return true;
    }

    void* render_graph::access_resource_storage(u32 h) const
    {
        const auto storageIndex = m_pins[h].storageIndex;
        auto& data = m_pinStorage[storageIndex];

        return data.ptr;
    }

    void render_graph::add_transient_resource(resource<texture> handle, u32 poolIndex)
    {
        m_transientTextures.emplace_back(handle, poolIndex);
        const u32 storageIndex = m_pins[handle.value].storageIndex;
        m_resourcePoolId[storageIndex] = poolIndex;
    }

    void render_graph::add_resource_transition(resource<texture> handle, VkImageLayout target)
    {
        m_textureTransitions.emplace_back(handle, target);
    }

    u32 render_graph::find_pool_index(resource<texture> handle) const
    {
        const u32 storageIndex = m_pins[handle.value].storageIndex;
        return m_resourcePoolId[storageIndex];
    }

    u32 render_graph::find_pool_index(resource<buffer> handle) const
    {
        const u32 storageIndex = m_pins[handle.value].storageIndex;
        return m_resourcePoolId[storageIndex];
    }

    u32 render_graph::find_output_storage_index(std::string_view name) const
    {
        for (auto& output : m_outputs)
        {
            if (output.name == name)
            {
                return output.storageIndex;
            }
        }

        // Zero is used as invalid value for pin storage
        return 0u;
    }

    void render_graph::add_transient_buffer(resource<buffer> handle, u32 poolIndex, const staging_buffer_span* upload)
    {
        m_transientBuffers.emplace_back(handle, poolIndex);
        const u32 storageIndex = m_pins[handle.value].storageIndex;
        m_resourcePoolId[storageIndex] = poolIndex;

        if (upload)
        {
            m_pendingUploads.emplace_back(handle, *upload);
        }
    }
}