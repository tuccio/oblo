#include <oblo/runtime/runtime.hpp>

#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/service_registry_builder.hpp>
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

namespace oblo
{
    namespace
    {
        expected<ecs::system_seq_executor> create_system_executor(std::span<ecs::world_builder* const> worldBuilders,
            ecs::system_graph_usages usages)
        {
            ecs::system_graph_builder builder{std::move(usages)};

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
        ecs::entity_registry entities;
        service_registry services;
    };

    runtime::runtime() = default;

    runtime::runtime(runtime&&) noexcept = default;

    runtime& runtime::operator=(runtime&&) noexcept = default;

    runtime::~runtime() = default;

    bool runtime::init(const runtime_initializer& initializer)
    {
        ecs::system_graph_usages usages;

        if (initializer.usages)
        {
            usages = *initializer.usages;
        }

        auto executor = create_system_executor(initializer.worldBuilders, std::move(usages));

        if (!executor)
        {
            return false;
        }

        m_impl = allocate_unique<impl>();

        if (!m_impl->frameAllocator.init(initializer.frameAllocatorMaxSize))
        {
            m_impl.reset();
            return false;
        }

        m_impl->entities.init(initializer.typeRegistry);

        m_impl->services.add<ecs::entity_registry>().externally_owned(&m_impl->entities);
        m_impl->services.add<const resource_registry>().externally_owned(initializer.resourceRegistry);
        m_impl->services.add<const property_registry>().externally_owned(initializer.propertyRegistry);

        service_registry_builder serviceRegistryBuilder;

        for (const auto* worldBuilder : initializer.worldBuilders)
        {
            if (!worldBuilder->services)
            {
                continue;
            }

            (worldBuilder->services)(serviceRegistryBuilder);
        }

        if (!serviceRegistryBuilder.build(m_impl->services))
        {
            return false;
        }

        m_impl->executor = std::move(*executor);

        return true;
    }

    void runtime::shutdown()
    {
        if (m_impl)
        {
            m_impl->executor.shutdown();
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