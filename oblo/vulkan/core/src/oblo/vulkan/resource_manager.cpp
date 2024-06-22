#include <oblo/vulkan/resource_manager.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/utility/pipeline_barrier.hpp>

namespace oblo::vk
{
    struct resource_manager::stored_texture
    {
        texture data;
        VkImageLayout layout;
    };

    struct resource_manager::stored_buffer
    {
        buffer data;
    };

    resource_manager::resource_manager() = default;

    resource_manager::~resource_manager() = default;

    h32<texture> resource_manager::register_texture(const texture& data, VkImageLayout currentLayout)
    {
        [[maybe_unused]] const auto [it, handle] = m_textures.emplace(data, currentLayout);
        OBLO_ASSERT(handle);
        return handle;
    }

    void resource_manager::unregister_texture(h32<texture> handle)
    {
        [[maybe_unused]] const auto n = m_textures.erase(handle);
        OBLO_ASSERT(n == 1);
    }

    h32<buffer> resource_manager::register_buffer(const buffer& data)
    {
        [[maybe_unused]] const auto [it, handle] = m_buffers.emplace(data);
        OBLO_ASSERT(handle);
        return handle;
    }

    void resource_manager::unregister_buffer(h32<buffer> handle)
    {
        [[maybe_unused]] const auto n = m_buffers.erase(handle);
        OBLO_ASSERT(n == 1);
    }

    h32<texture> resource_manager::create(gpu_allocator& allocator, const image_initializer& initializer)
    {
        allocated_image allocatedImage;

        if (allocator.create_image(initializer, &allocatedImage) != VK_SUCCESS)
        {
            return {};
        }

        return register_texture(
            {
                .image = allocatedImage.image,
                .allocation = allocatedImage.allocation,
                .view = nullptr,
                .initializer = initializer,
            },
            VK_IMAGE_LAYOUT_UNDEFINED);
    }

    h32<buffer> resource_manager::create(gpu_allocator& allocator, const buffer_initializer& initializer)
    {
        allocated_buffer allocatedBuffer;

        if (allocator.create_buffer(initializer, &allocatedBuffer) != VK_SUCCESS)
        {
            return {};
        }

        return register_buffer({
            .buffer = allocatedBuffer.buffer,
            .offset = 0u,
            .size = initializer.size,
            .allocation = allocatedBuffer.allocation,
        });
    }

    void resource_manager::destroy(gpu_allocator& allocator, h32<texture> handle)
    {
        const texture& res = get(handle);
        allocator.destroy(allocated_image{.image = res.image, .allocation = res.allocation});
        unregister_texture(handle);
    }

    void resource_manager::destroy(gpu_allocator& allocator, h32<buffer> handle)
    {
        const buffer& res = get(handle);
        allocator.destroy(allocated_buffer{.buffer = res.buffer, .allocation = res.allocation});
        unregister_buffer(handle);
    }

    const texture* resource_manager::try_find(h32<texture> handle) const
    {
        auto* const storage = m_textures.try_find(handle);
        return storage ? &storage->data : nullptr;
    }

    texture* resource_manager::try_find(h32<texture> handle)
    {
        auto* const storage = m_textures.try_find(handle);
        return storage ? &storage->data : nullptr;
    }

    const buffer* resource_manager::try_find(h32<buffer> handle) const
    {
        auto* const storage = m_buffers.try_find(handle);
        return storage ? &storage->data : nullptr;
    }

    buffer* resource_manager::try_find(h32<buffer> handle)
    {
        auto* const storage = m_buffers.try_find(handle);
        return storage ? &storage->data : nullptr;
    }

    const texture& resource_manager::get(h32<texture> handle) const
    {
        auto* ptr = try_find(handle);
        OBLO_ASSERT(ptr);
        return *ptr;
    }

    texture& resource_manager::get(h32<texture> handle)
    {
        auto* ptr = try_find(handle);
        OBLO_ASSERT(ptr);
        return *ptr;
    }

    const buffer& resource_manager::get(h32<buffer> handle) const
    {
        auto* ptr = try_find(handle);
        OBLO_ASSERT(ptr);
        return *ptr;
    }

    buffer& resource_manager::get(h32<buffer> handle)
    {
        auto* ptr = try_find(handle);
        OBLO_ASSERT(ptr);
        return *ptr;
    }

    bool resource_manager::commit(command_buffer_state& commandBufferState, VkCommandBuffer transitionsBuffer)
    {
        const bool anyTransition{commandBufferState.has_incomplete_transitions()};
        OBLO_ASSERT(!anyTransition || transitionsBuffer);

        // Fill up the transitions buffer with the incomplete transitions, which we can now check from global state
        for (const auto& pendingTransition : commandBufferState.m_incompleteTransitions)
        {
            const auto* it = m_textures.try_find(pendingTransition.handle);
            OBLO_ASSERT(it != nullptr, "The texture is not registered, unable to add transition");

            if (it)
            {
                const auto oldLayout = it->layout;

                add_pipeline_barrier_cmd(transitionsBuffer,
                    oldLayout,
                    pendingTransition.newLayout,
                    it->data.image,
                    pendingTransition.format,
                    pendingTransition.layerCount,
                    pendingTransition.mipLevels);
            }
        }

        // Update the global state
        const std::span keys = commandBufferState.m_transitions.keys();
        const std::span values = commandBufferState.m_transitions.values();

        for (usize i = 0; i < keys.size(); ++i)
        {
            const auto handle = keys[i];

            auto* const layoutPtr = m_textures.try_find(handle);

            if (layoutPtr)
            {
                layoutPtr->layout = values[i];
            }
        }

        return anyTransition;
    }

    void command_buffer_state::set_starting_layout(h32<texture> texture, VkImageLayout currentLayout)
    {
        m_transitions.emplace(texture, currentLayout);
    }

    void command_buffer_state::add_pipeline_barrier(
        const texture& tex, h32<texture> handle, VkCommandBuffer commandBuffer, VkImageLayout newLayout)
    {
        auto* const transitionPtr = m_transitions.try_find(handle);

        if (!transitionPtr)
        {
            m_transitions.emplace(handle, newLayout);

            m_incompleteTransitions.emplace_back(image_transition{
                .handle = handle,
                .newLayout = newLayout,
                .format = tex.initializer.format,
                .layerCount = tex.initializer.arrayLayers,
                .mipLevels = tex.initializer.mipLevels,
            });
        }
        else
        {
            const auto oldLayout = *transitionPtr;
            *transitionPtr = newLayout;

            add_pipeline_barrier_cmd(commandBuffer,
                oldLayout,
                newLayout,
                tex.image,
                tex.initializer.format,
                tex.initializer.arrayLayers,
                tex.initializer.mipLevels);
        }
    }

    void command_buffer_state::add_pipeline_barrier(const resource_manager& resourceManager,
        h32<texture> handle,
        VkCommandBuffer commandBuffer,
        VkImageLayout newLayout)
    {
        auto* texturePtr = resourceManager.try_find(handle);
        OBLO_ASSERT(texturePtr);

        add_pipeline_barrier(*texturePtr, handle, commandBuffer, newLayout);
    }

    void command_buffer_state::clear()
    {
        m_transitions.clear();
        m_incompleteTransitions.clear();
    }

    bool command_buffer_state::has_incomplete_transitions() const
    {
        return !m_incompleteTransitions.empty();
    }

    expected<VkImageLayout> command_buffer_state::try_find(h32<texture> handle) const
    {
        if (auto* const layout = m_transitions.try_find(handle))
        {
            return *layout;
        }

        return unspecified_error;
    }
}