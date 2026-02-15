#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <oblo/gpu/vulkan/error.hpp>

#include <span>
#include <unordered_map>

#include <vulkan/vulkan_core.h>

namespace oblo
{
    class string;
}

namespace oblo::gpu::vk
{
    struct descriptor_binding
    {
        h32<string> name;
        u32 binding;
        VkDescriptorType descriptorType;
        VkShaderStageFlags stageFlags;
        bool readOnly;
    };

    class descriptor_set_pool
    {
    public:
        void init(VkDevice device,
            const VkAllocationCallbacks* allocator,
            u32 maxSetsPerPool,
            VkDescriptorPoolCreateFlags flags,
            const std::span<const VkDescriptorPoolSize> poolSizes);

        void shutdown();

        void on_submit(u64 submitIndex);

        result<VkDescriptorSetLayout> get_or_add_layout(std::span<const descriptor_binding> bindings);

        result<VkDescriptorSet> acquire(u64 lastFinishedSubmit, VkDescriptorSetLayout layout, void* pNext = nullptr);

    private:
        result<VkDescriptorPool> create_pool();
        result<VkDescriptorPool> acquire_pool(u64 lastFinishedSubmit);

    private:
        struct used_pool
        {
            VkDescriptorPool pool;
            u64 frameIndex;
        };

    private:
        VkDevice m_device{};
        const VkAllocationCallbacks* m_allocator{};
        VkDescriptorPool m_current{};
        u64 m_submitIndex{};
        u32 m_maxSetsPerPool{};
        VkDescriptorPoolCreateFlags m_poolCreateFlags{};
        deque<used_pool> m_used;
        std::unordered_map<usize, VkDescriptorSetLayout> m_pool;
        dynamic_array<VkDescriptorPoolSize> m_poolSizes;
    };
}