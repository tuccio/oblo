#pragma once

#include <oblo/core/time/time.hpp>
#include <oblo/core/types.hpp>
#include <oblo/ecs/forward.hpp>

#include <memory>
#include <span>

namespace oblo::reflection
{
    class reflection_registry;
}

namespace oblo::vk
{
    class renderer;
    class vulkan_context;
}

namespace oblo
{
    class frame_allocator;
    class property_registry;
    class resource_registry;

    struct runtime_initializer
    {
        const reflection::reflection_registry* reflectionRegistry;
        const property_registry* propertyRegistry;
        const resource_registry* resourceRegistry;
        vk::vulkan_context* vulkanContext;
        std::span<ecs::world_builder* const> worldBuilders;
        usize frameAllocatorMaxSize{1u << 28};
    };

    struct runtime_update_context
    {
        time dt;
    };

    class runtime
    {
    public:
        RUNTIME_API runtime();
        runtime(const runtime&) = delete;
        RUNTIME_API runtime(runtime&&) noexcept;

        RUNTIME_API ~runtime();

        runtime& operator=(const runtime&) = delete;
        RUNTIME_API runtime& operator=(runtime&&) noexcept;

        [[nodiscard]] RUNTIME_API bool init(const runtime_initializer& initializer);
        RUNTIME_API void shutdown();

        RUNTIME_API void update(const runtime_update_context& ctx);

        RUNTIME_API vk::renderer& get_renderer() const;

        RUNTIME_API ecs::entity_registry& get_entity_registry() const;

    private:
        struct impl;

    private:
        std::unique_ptr<impl> m_impl;
    };
}