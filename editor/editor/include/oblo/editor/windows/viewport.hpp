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
    class allocator;
    class resource_manager;
    class single_queue_engine;
    struct texture;
}

namespace oblo::editor
{
    class viewport final
    {
    public:
        viewport() = delete;
        viewport(const vk::allocator& allocator,
                 const vk::single_queue_engine& engine,
                 vk::resource_manager& resourceManager,
                 ecs::entity_registry& entities);
        viewport(const viewport&) = delete;
        viewport(viewport&&) noexcept = delete;
        ~viewport();

        viewport& operator=(const viewport&) = delete;
        viewport& operator=(viewport&&) noexcept = delete;

        bool update();

    private:
        const vk::allocator* m_allocator;
        vk::resource_manager* m_resourceManager;
        void* m_imguiImage{};
        VkSampler m_sampler;
        h32<vk::texture> m_texture{};
        ecs::entity_registry* m_entities;
        ecs::entity m_entity;
    };
}