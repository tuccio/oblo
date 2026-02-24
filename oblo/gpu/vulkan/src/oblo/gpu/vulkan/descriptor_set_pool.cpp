#include <oblo/gpu/vulkan/descriptor_set_pool.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/gpu/vulkan/gpu_allocator.hpp>

namespace oblo::gpu::vk
{
    void descriptor_set_pool::init(VkDevice device, const VkAllocationCallbacks* allocator)
    {
        m_device = device;
        m_allocator = allocator;
    }

    void descriptor_set_pool::add_pool_kind(flags<gpu::resource_binding_kind> kinds,
        u32 maxSetsPerPool,
        VkDescriptorPoolCreateFlags createFlags,
        const std::span<const VkDescriptorPoolSize> poolSizes)
    {
        auto& impl = m_pools.emplace_back();

        impl.maxSetsPerPool = maxSetsPerPool;
        impl.kinds = kinds;
        impl.createFlags = createFlags;
        impl.poolSizes.assign(poolSizes.begin(), poolSizes.end());
    }

    void descriptor_set_pool::shutdown()
    {
        for (auto& impl : m_pools)
        {
            if (impl.current)
            {
                vkDestroyDescriptorPool(m_device, impl.current, m_allocator);
            }

            for (const auto& [pool, index] : impl.usedPools)
            {
                vkDestroyDescriptorPool(m_device, pool, m_allocator);
            }
        }

        m_pools.clear();
    }

    void descriptor_set_pool::on_submit(u64 submitIndex)
    {
        for (auto& impl : m_pools)
        {
            if (impl.current)
            {
                impl.usedPools.emplace_back(impl.current, m_submitIndex);
                impl.current = nullptr;
            }

            for (const auto& [pool, index] : impl.usedPools)
            {
                vkDestroyDescriptorPool(m_device, pool, m_allocator);
            }
        }

        m_submitIndex = submitIndex;
    }

    result<VkDescriptorSet> descriptor_set_pool::acquire(flags<gpu::resource_binding_kind> kinds,
        u64 lastFinishedSubmit,
        const VkDescriptorSetLayout layout,
        void* pNext)
    {
        pool_impl* impl{};

        for (pool_impl& pool : m_pools)
        {
            if (pool.kinds == kinds)
            {
                impl = &pool;
                break;
            }
        }

        if (!impl)
        {
            OBLO_ASSERT(false);
            return error::invalid_usage;
        }

        const result<VkDescriptorPool> pool = acquire_pool(*impl, lastFinishedSubmit);

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

        VkDescriptorSet descriptorSet;
        const VkResult r = vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet);

        // We should handle VK_ERROR_OUT_OF_POOL_MEMORY here, and create a new pool if necessary.
        OBLO_ASSERT(r != VK_ERROR_OUT_OF_POOL_MEMORY);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        return descriptorSet;
    }

    result<VkDescriptorPool> descriptor_set_pool::create_pool(pool_impl& impl)
    {
        const VkDescriptorPoolCreateInfo poolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = impl.createFlags,
            .maxSets = impl.maxSetsPerPool,
            .poolSizeCount = impl.poolSizes.size32(),
            .pPoolSizes = impl.poolSizes.data(),
        };

        VkDescriptorPool pool;

        const VkResult r = vkCreateDescriptorPool(m_device, &poolCreateInfo, m_allocator, &pool);

        if (r != VK_SUCCESS)
        {
            return translate_error(r);
        }

        return pool;
    }

    result<VkDescriptorPool> descriptor_set_pool::acquire_pool(pool_impl& impl, u64 lastFinishedSubmit)
    {
        if (impl.current)
        {
            // Nothing to do, we will return the current one
        }
        else if (!impl.usedPools.empty() && lastFinishedSubmit >= impl.usedPools.front().frameIndex)
        {
            // The pool is no longer in use since the submit was executed, we can reuse it.
            auto* const next = impl.usedPools.front().pool;

            const VkResult r = vkResetDescriptorPool(m_device, next, 0);

            if (r != VK_SUCCESS)
            {
                return translate_error(r);
            }

            impl.current = next;
            impl.usedPools.pop_front();
        }
        else
        {
            // No pool available, we create one
            const result r = create_pool(impl);

            if (!r)
            {
                return r.error();
            }

            impl.current = *r;
        }

        return impl.current;
    }
}