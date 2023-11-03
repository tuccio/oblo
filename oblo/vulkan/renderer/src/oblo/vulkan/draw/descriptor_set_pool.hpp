#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>

#include <deque>
#include <span>
#include <unordered_map>

#include <vulkan/vulkan.h>

namespace oblo
{
    struct string;
}

namespace oblo::vk
{
    class vulkan_context;

    struct descriptor_binding
    {
        h32<string> name;
        u32 binding;
        VkDescriptorType descriptorType;
        VkShaderStageFlags stageFlags;
    };

    class descriptor_set_pool
    {
    public:
        void init(vulkan_context& context);
        void shutdown();

        void begin_frame();
        void end_frame();

        VkDescriptorSetLayout get_or_add_layout(std::span<const descriptor_binding> bindings);

        VkDescriptorSet acquire(VkDescriptorSetLayout layout);

    private:
        VkDescriptorPool create_pool();
        VkDescriptorPool acquire_pool();

    private:
        struct used_pool
        {
            VkDescriptorPool pool;
            u64 frameIndex;
        };

    private:
        vulkan_context* m_ctx{};
        VkDescriptorPool m_current{};
        u64 m_submitIndex{};
        std::deque<used_pool> m_used;
        std::unordered_map<usize, VkDescriptorSetLayout> m_pool;
    };
}