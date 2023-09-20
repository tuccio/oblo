#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/ecs/handles.hpp>

#include <vulkan/vulkan.h>

namespace oblo
{
    class service_registry;
}

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::vk
{
    class vulkan_context;
    struct texture;
}

namespace oblo::editor
{
    class viewport final
    {
    public:
        viewport() = delete;
        viewport(vk::vulkan_context& context, ecs::entity_registry& entities);
        viewport(const viewport&) = delete;
        viewport(viewport&&) noexcept = delete;
        ~viewport();

        viewport& operator=(const viewport&) = delete;
        viewport& operator=(viewport&&) noexcept = delete;

        bool update();

    private:
        vk::vulkan_context* m_ctx;
        VkDescriptorSet m_descriptorSet{};
        VkSampler m_sampler;
        h32<vk::texture> m_texture{};
        ecs::entity_registry* m_entities;
        ecs::entity m_entity;
    };
}