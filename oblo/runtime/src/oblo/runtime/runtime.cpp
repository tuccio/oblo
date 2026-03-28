#include <oblo/runtime/runtime.hpp>

#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/service_registry_builder.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/ecs/systems/system_seq_executor.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/trace/profile.hpp>

namespace oblo
{
    namespace
    {
        constexpr bool g_InjectHotreloadTrampoline = true;

        struct system_trampoline
        {
            static void* create(void* userdata)
            {
                auto* const self = reinterpret_cast<system_trampoline*>(userdata);
                self->system = self->wrappedSystem.create(self->wrappedSystem.userdata);
                return self;
            }

            static void destroy(void*, void* ptr)
            {
                auto* const self = reinterpret_cast<system_trampoline*>(ptr);

                if (self->system)
                {
                    self->wrappedSystem.destroy(self->wrappedSystem.userdata, self->system);
                }
            }

            static void first_update(void*, void* ptr, const ecs::system_update_context* ctx)
            {
                auto* const self = reinterpret_cast<system_trampoline*>(ptr);

                if (self->wrappedSystem.firstUpdate)
                {
                    self->wrappedSystem.firstUpdate(self->wrappedSystem.userdata, self->system, ctx);
                }
                else if (self->wrappedSystem.update)
                {
                    self->wrappedSystem.update(self->wrappedSystem.userdata, self->system, ctx);
                }
            }

            static void update(void*, void* ptr, const ecs::system_update_context* ctx)
            {
                auto* const self = reinterpret_cast<system_trampoline*>(ptr);

                if (self->wrappedSystem.update)
                {
                    self->wrappedSystem.update(self->wrappedSystem.userdata, self->system, ctx);
                }
            }

            void* system;
            ecs::system_descriptor wrappedSystem;
        };

        expected<ecs::system_seq_executor> create_system_executor(std::span<ecs::world_builder* const> worldBuilders,
            ecs::system_graph_usages usages,
            dynamic_array<system_trampoline>& outTrampolines)
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
                return g.error();
            }

            if constexpr (g_InjectHotreloadTrampoline)
            {
                dynamic_array<h32<ecs::system>> handles;
                g->fetch_systems(handles);

                OBLO_ASSERT(outTrampolines.empty(), "This shouldn't really be growing, we give out pointers to this");
                outTrampolines.resize(handles.size());

                auto* trampolineIt = outTrampolines.data();

                for (const h32 system : handles)
                {
                    auto& desc = g->get_system_descriptor(system);

                    // Barriers will appear in this list, which are not real systems, we don't need to do anything on
                    // those
                    if (desc.create)
                    {
                        trampolineIt->wrappedSystem = desc;

                        desc.create = system_trampoline::create;
                        desc.destroy = system_trampoline::destroy;
                        desc.firstUpdate = system_trampoline::first_update;
                        desc.update = system_trampoline::update;
                        desc.userdata = trampolineIt;

                        ++trampolineIt;
                    }
                }
            }

            return g->instantiate();
        }
    }

    struct runtime::impl
    {
        // This array has to be destroyed after the world and executor, since we use this to replace systems
        dynamic_array<system_trampoline> trampolines;

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

        dynamic_array<system_trampoline> trampolines;
        auto executor = create_system_executor(initializer.worldBuilders, std::move(usages), trampolines);

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
        m_impl->trampolines = std::move(trampolines);

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