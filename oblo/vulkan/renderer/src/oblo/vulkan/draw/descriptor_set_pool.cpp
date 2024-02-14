#include <oblo/vulkan/draw/descriptor_set_pool.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/handle_hash.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/small_vector.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    void descriptor_set_pool::init(const vulkan_context& context,
        u32 maxSetsPerPool,
        VkDescriptorPoolCreateFlags flags,
        const std::span<const VkDescriptorPoolSize> poolSizes)
    {
        m_ctx = &context;
        m_poolSizes.assign(poolSizes.begin(), poolSizes.end());

        m_maxSetsPerPool = maxSetsPerPool;
        m_poolCreateFlags = flags;
    }

    void descriptor_set_pool::shutdown(vulkan_context& vkCtx)
    {
        if (m_current)
        {
            vkCtx.destroy_deferred(m_current, m_submitIndex);
        }

        for (const auto& [h, layout] : m_pool)
        {
            vkCtx.destroy_deferred(layout, m_submitIndex);
        }

        for (const auto& [pool, index] : m_used)
        {
            vkCtx.destroy_deferred(pool, index);
        }
    }

    void descriptor_set_pool::begin_frame()
    {
        m_submitIndex = m_ctx->get_submit_index();
    }

    void descriptor_set_pool::end_frame()
    {
        if (!m_current)
        {
            return;
        }

        m_used.emplace_back(m_current, m_submitIndex);
        m_current = nullptr;
    }

    VkDescriptorSetLayout descriptor_set_pool::get_or_add_layout(std::span<const descriptor_binding> bindings)
    {
        if (bindings.empty())
        {
            return nullptr;
        }

        usize h = 0;

        for (auto& binding : bindings)
        {
            struct_apply([&h]<typename... T>(const T&... value) { ((h = hash_mix(h, std::hash<T>{}(value))), ...); },
                binding);
        }

        VkDescriptorSetLayout layout;

        if (const auto it = m_pool.find(h); it == m_pool.end())
        {
            const VkDevice device = m_ctx->get_device();
            auto* const allocationCbs = m_ctx->get_allocator().get_allocation_callbacks();

            small_vector<VkDescriptorSetLayoutBinding, 32> vkBindings;
            vkBindings.resize(bindings.size());

            for (usize i = 0; i < bindings.size(); ++i)
            {
                const auto& current = bindings[i];

                vkBindings[i] = {
                    .binding = current.binding,
                    .descriptorType = current.descriptorType,
                    .descriptorCount = 1u,
                    .stageFlags = current.stageFlags,
                };
            }

            const VkDescriptorSetLayoutCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = u32(vkBindings.size()),
                .pBindings = vkBindings.data(),
            };

            OBLO_VK_PANIC(vkCreateDescriptorSetLayout(device, &createInfo, allocationCbs, &layout));

            [[maybe_unused]] const auto [newIt, ok] = m_pool.emplace(h, layout);
            OBLO_ASSERT(ok);
        }
        else
        {
            layout = it->second;
        }

        return layout;
    }

    VkDescriptorSet descriptor_set_pool::acquire(const VkDescriptorSetLayout layout, void* pNext)
    {
        const VkDevice device = m_ctx->get_device();

        const VkDescriptorPool pool = acquire_pool();
        OBLO_ASSERT(pool);

        const VkDescriptorSetAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = pNext,
            .descriptorPool = pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout,
        };

        // We should handle VK_ERROR_OUT_OF_POOL_MEMORY here, and create a new pool if necessary.
        VkDescriptorSet descriptorSet;
        OBLO_VK_PANIC(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

        return descriptorSet;
    }

    VkDescriptorPool descriptor_set_pool::create_pool()
    {
        auto* const allocationCbs = m_ctx->get_allocator().get_allocation_callbacks();
        const VkDevice device = m_ctx->get_device();

        const VkDescriptorPoolCreateInfo poolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = m_poolCreateFlags,
            .maxSets = m_maxSetsPerPool,
            .poolSizeCount = u32(m_poolSizes.size()),
            .pPoolSizes = m_poolSizes.data(),
        };

        VkDescriptorPool pool;
        OBLO_VK_PANIC(vkCreateDescriptorPool(device, &poolCreateInfo, allocationCbs, &pool));
        return pool;
    }

    VkDescriptorPool descriptor_set_pool::acquire_pool()
    {
        if (m_current)
        {
            // Nothing to do, we will return the current one
        }
        else if (!m_used.empty() && m_ctx->is_submit_done(m_used.front().frameIndex))
        {
            // The pool is no longer in use since the submit was executed, we can reuse it.
            m_current = m_used.front().pool;
            m_used.pop_front();

            OBLO_VK_PANIC(vkResetDescriptorPool(m_ctx->get_device(), m_current, 0));
        }
        else
        {
            // No pool available, we create one
            m_current = create_pool();
        }

        return m_current;
    }
}