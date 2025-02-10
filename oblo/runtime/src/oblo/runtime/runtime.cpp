#include <oblo/runtime/runtime.hpp>

#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/ecs/systems/system_seq_executor.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/graphics/graphics_module.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/scene/components/name_component.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo
{
    namespace
    {
        expected<ecs::system_seq_executor> create_system_executor(std::span<ecs::world_builder* const> worldBuilders)
        {
            ecs::system_graph_builder builder;

            for (const auto& worldBuilder : worldBuilders)
            {
                if (!worldBuilder->systems)
                {
                    continue;
                }

                (worldBuilder->systems)(builder);
            }

            auto g = builder.build();

            if (!g)
            {
                return unspecified_error;
            }

            return g->instantiate();
        }
    }

    struct runtime::impl
    {
        frame_allocator frameAllocator;
        ecs::system_seq_executor executor;
        ecs::type_registry typeRegistry;
        ecs::entity_registry entities;
        service_registry services;
        vk::vulkan_context* vulkanContext;
        vk::draw_registry drawRegistry;
        bool isRayTracingEnabled;
    };

    runtime::runtime() = default;

    runtime::runtime(runtime&&) noexcept = default;

    runtime& runtime::operator=(runtime&&) noexcept = default;

    runtime::~runtime() = default;

    bool runtime::init(const runtime_initializer& initializer)
    {
        auto executor = create_system_executor(initializer.worldBuilders);

        if (!executor)
        {
            return false;
        }

        m_impl = std::make_unique<impl>();

        if (!m_impl->frameAllocator.init(initializer.frameAllocatorMaxSize))
        {
            m_impl.reset();
            return false;
        }

        ecs_utility::register_reflected_component_and_tag_types(*initializer.reflectionRegistry,
            &m_impl->typeRegistry,
            nullptr);

        m_impl->entities.init(&m_impl->typeRegistry);

        // We should probably move this to a world builder in a module if it's necessary
        auto& vulkanContext = initializer.renderer.get_vulkan_context();
        m_impl->services.add<vk::vulkan_context>().externally_owned(&vulkanContext);
        m_impl->services.add<vk::renderer>().externally_owned(&initializer.renderer);

        m_impl->isRayTracingEnabled = initializer.renderer.is_ray_tracing_enabled();

        m_impl->drawRegistry.init(vulkanContext,
            initializer.renderer.get_staging_buffer(),
            initializer.renderer.get_string_interner(),
            m_impl->entities,
            *initializer.resourceRegistry,
            initializer.renderer.get_instance_data_type_registry());

        m_impl->services.add<vk::draw_registry>().externally_owned(&m_impl->drawRegistry);

        m_impl->services.add<const resource_registry>().externally_owned(initializer.resourceRegistry);
        m_impl->services.add<const property_registry>().externally_owned(initializer.propertyRegistry);

        for (const auto* worldBuilder : initializer.worldBuilders)
        {
            if (!worldBuilder->services)
            {
                continue;
            }

            (worldBuilder->services)(m_impl->services);
        }

        m_impl->services.add<vk::resource_cache>().externally_owned(&initializer.renderer.get_resource_cache());

        m_impl->executor = std::move(*executor);

        m_impl->vulkanContext = &vulkanContext;

        return true;
    }

    void runtime::shutdown()
    {
        if (m_impl)
        {
            m_impl->executor.shutdown();
            m_impl->drawRegistry.shutdown();
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

        // Just temporarily here, should probably be in a render graph node
        auto&& commandBuffer = m_impl->vulkanContext->get_active_command_buffer();
        m_impl->drawRegistry.flush_uploads(commandBuffer.get());

        m_impl->drawRegistry.generate_mesh_database(m_impl->frameAllocator);
        m_impl->drawRegistry.generate_draw_calls(m_impl->frameAllocator);

        if (m_impl->isRayTracingEnabled)
        {
            m_impl->drawRegistry.generate_raytracing_structures(m_impl->frameAllocator, commandBuffer.get());
        }
    }

    ecs::entity_registry& runtime::get_entity_registry() const
    {
        return m_impl->entities;
    }

    const service_registry& runtime::get_service_registry() const
    {
        return m_impl->services;
    }
}