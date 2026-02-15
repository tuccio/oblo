#include <oblo/gpu/vulkan/descriptor_set_pool.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/handle_hash.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/gpu/vulkan/gpu_allocator.hpp>

namespace oblo::gpu::vk
{
    void descriptor_set_pool::init(VkDevice device,
        const VkAllocationCallbacks* allocator,
        u32 maxSetsPerPool,
        VkDescriptorPoolCreateFlags flags,
        const std::span<const VkDescriptorPoolSize> poolSizes)
    {
        m_device = device;
        m_allocator = allocator;

        m_poolSizes.assign(poolSizes.begin(), poolSizes.end());

        m_maxSetsPerPool = maxSetsPerPool;
        m_poolCreateFlags = flags;
    }

    void descriptor_set_pool::shutdown()
    {
        if (m_current)
        {
            vkDestroyDescriptorPool(m_device, m_current, m_allocator);
            m_current = {};
        }

        for (const auto& [h, layout] : m_pool)
        {
            vkDestroyDescriptorSetLayout(m_device, layout, m_allocator);
        }

        m_pool.clear();

        for (const auto& [pool, index] : m_used)
        {
            vkDestroyDescriptorPool(m_device, pool, m_allocator);
        }

        m_used.clear();
    }

    void descriptor_set_pool::on_submit(u64 submitIndex)
    {
        if (m_current)
        {
            m_used.emplace_back(m_current, m_submitIndex);
            m_current = nullptr;
        }

        m_submitIndex = submitIndex;
    }

    result<VkDescriptorSetLayout> descriptor_set_pool::get_or_add_layout(std::span<const descriptor_binding> bindings)
    {
        if (bindings.empty())
        {
            return error::invalid_usage;
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
            buffered_array<VkDescriptorSetLayoutBinding, 32> vkBindings;
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

            const VkResult r = vkCreateDescriptorSetLayout(m_device, &createInfo, m_allocator, &layout);

            if (r != VK_SUCCESS)
            {
                return translate_error(r);
            }

            [[maybe_unused]] const auto [newIt, ok] = m_pool.emplace(h, layout);
            OBLO_ASSERT(ok);
        }
        else
        {
            layout = it->second;
        }

        return layout;
    }

    result<VkDescriptorSet> descriptor_set_pool::acquire(u64 lastFinishedSubmit, const VkDescriptorSetLayout layout, void* pNext)
    {
        const result<VkDescriptorPool> pool = acquire_pool(lastFinishedSubmit);

        if (!pool)
        {
            return pool.error();
        }

        OBLO_ASSERT(*pool);

        const VkDescriptorSetAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = pNext,
            .descriptorPool = *pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout,
        };

        // We should handle VK_ERROR_OUT_OF_POOL_MEMORY here, and create a new pool if necessary.
        VkDescriptorSet descriptorSet;

        const VkResult r = vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        return descriptorSet;
    }

    result<VkDescriptorPool> descriptor_set_pool::create_pool()
    {
        const VkDescriptorPoolCreateInfo poolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = m_poolCreateFlags,
            .maxSets = m_maxSetsPerPool,
            .poolSizeCount = u32(m_poolSizes.size()),
            .pPoolSizes = m_poolSizes.data(),
        };

        VkDescriptorPool pool;

        const VkResult r = vkCreateDescriptorPool(m_device, &poolCreateInfo, m_allocator, &pool);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        return pool;
    }

    result<VkDescriptorPool> descriptor_set_pool::acquire_pool(u64 lastFinishedSubmit)
    {
        if (m_current)
        {
            // Nothing to do, we will return the current one
        }
        else if (!m_used.empty() && lastFinishedSubmit >= m_used.front().frameIndex)
        {
            // The pool is no longer in use since the submit was executed, we can reuse it.
            auto* const next = m_used.front().pool;

            const VkResult r = vkResetDescriptorPool(m_device, next, 0);

            if (r != VK_SUCCESS)
            {
                return translate_error(r);
            }

            m_current = next;
            m_used.pop_front();
        }
        else
        {
            // No pool available, we create one
            const result r = create_pool();

            if (!r)
            {
                return r.error();
            }

            m_current = *r;
        }

        return m_current;
    }
}