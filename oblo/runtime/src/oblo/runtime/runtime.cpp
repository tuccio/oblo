#include <oblo/runtime/runtime.hpp>

#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/services/service_registrant.hpp>
#include <oblo/ecs/systems/system_graph.hpp>
#include <oblo/ecs/systems/system_seq_executor.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/graphics/graphics_module.hpp>
#include <oblo/graphics/systems/lighting_system.hpp>
#include <oblo/graphics/systems/static_mesh_system.hpp>
#include <oblo/graphics/systems/viewport_system.hpp>
#include <oblo/scene/components/name_component.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/scene/systems/transform_system.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/draw/resource_cache.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/required_features.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo
{
    namespace
    {
        ecs::system_seq_executor create_system_executor()
        {
            ecs::system_graph g;

            g.add_system<transform_system>();
            g.add_system<static_mesh_system>();
            g.add_system<lighting_system>();
            g.add_system<viewport_system>();

            return g.instantiate();
        }
    }

    struct runtime::impl
    {
        frame_allocator frameAllocator;
        ecs::system_seq_executor executor;
        ecs::type_registry typeRegistry;
        ecs::entity_registry entities;
        service_registry services;
        vk::renderer renderer;
        vk::vulkan_context* vulkanContext;
    };

    runtime::runtime() = default;

    runtime::runtime(runtime&&) noexcept = default;

    runtime& runtime::operator=(runtime&&) noexcept = default;

    runtime::~runtime() = default;

    bool runtime::init(const runtime_initializer& initializer)
    {
        m_impl = std::make_unique<impl>();

        if (!m_impl->frameAllocator.init(initializer.frameAllocatorMaxSize))
        {
            m_impl.reset();
            return false;
        }

        ecs_utility::register_reflected_component_types(*initializer.reflectionRegistry,
            &m_impl->typeRegistry,
            initializer.propertyRegistry);

        m_impl->entities.init(&m_impl->typeRegistry);

        m_impl->services.add<vk::vulkan_context>().externally_owned(initializer.vulkanContext);
        m_impl->services.add<vk::renderer>().externally_owned(&m_impl->renderer);
        m_impl->services.add<resource_registry>().externally_owned(initializer.resourceRegistry);

        for (const auto* serviceRegistrant : initializer.serviceRegistrants)
        {
            serviceRegistrant->registerServices(m_impl->services);
        }

        auto* const resourceCache = m_impl->services.add<vk::resource_cache>().unique();

        resourceCache->init(*initializer.resourceRegistry, m_impl->renderer.get_texture_registry());

        m_impl->executor = create_system_executor();

        m_impl->vulkanContext = initializer.vulkanContext;

        if (!m_impl->renderer.init({
                .vkContext = *m_impl->vulkanContext,
                .frameAllocator = m_impl->frameAllocator,
                .entities = m_impl->entities,
            }))
        {
            shutdown();
            return false;
        }

        return true;
    }

    void runtime::shutdown()
    {
        if (m_impl)
        {
            m_impl->executor.shutdown();
            m_impl->renderer.shutdown();
            m_impl.reset();
        }
    }

    void runtime::update(const runtime_update_context& ctx)
    {
        OBLO_PROFILE_SCOPE();

        const auto frameAllocatorScope = m_impl->frameAllocator.make_scoped_restore();

        m_impl->executor.update({
            .entities = &m_impl->entities,
            .services = &m_impl->services,
            .frameAllocator = &m_impl->frameAllocator,
            .dt = ctx.dt,
        });

        m_impl->renderer.update(m_impl->frameAllocator);
    }

    vk::renderer& runtime::get_renderer() const
    {
        return m_impl->renderer;
    }

    ecs::entity_registry& runtime::get_entity_registry() const
    {
        return m_impl->entities;
    }

    vk::required_features runtime::get_required_vulkan_features()
    {
        return vk::renderer::get_required_features();
    }
}