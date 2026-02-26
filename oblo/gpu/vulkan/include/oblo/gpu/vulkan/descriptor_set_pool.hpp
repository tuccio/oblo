#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/vulkan/error.hpp>

#include <span>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
{
    class descriptor_set_pool
    {
    public:
        void init(VkDevice device, const VkAllocationCallbacks* allocator);

        void add_pool_kind(flags<gpu::resource_binding_kind> kinds,
            u32 maxSetsPerPool,
            VkDescriptorPoolCreateFlags createFlags,
            const std::span<const VkDescriptorPoolSize> poolSizes);

        void shutdown();

        void on_submit(u64 submitIndex);

        result<VkDescriptorSet> acquire(
            flags<gpu::resource_binding_kind> kinds, u64 lastFinishedSubmit, VkDescriptorSetLayout layout, void* pNext);

    private:
        struct used_pool
        {
            VkDescriptorPool pool;
            u64 submitIndex;
        };

        struct pool_impl
        {
            VkDescriptorPool current{};
            deque<used_pool> usedPools;
            dynamic_array<VkDescriptorPoolSize> poolSizes;
            VkDescriptorPoolCreateFlags createFlags;
            u32 maxSetsPerPool;
            flags<gpu::resource_binding_kind> kinds;
        };

    private:
        result<VkDescriptorPool> create_pool(pool_impl& impl);
        result<VkDescriptorPool> acquire_pool(pool_impl& impl, u64 lastFinishedSubmit);

    private:
        VkDevice m_device{};
        const VkAllocationCallbacks* m_allocator{};

        u64 m_submitIndex{};
        dynamic_array<pool_impl> m_pools;
    };
}