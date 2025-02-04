#include <oblo/ecs/systems/system_seq_executor.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/systems/system_descriptor.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/trace/profile.hpp>

namespace oblo::ecs
{
    struct system_seq_executor::system_info
    {
        system_descriptor desc;
        void* system;
    };

    system_seq_executor::system_seq_executor() = default;

    system_seq_executor::system_seq_executor(system_seq_executor&&) noexcept = default;

    system_seq_executor::~system_seq_executor()
    {
        shutdown();
    }

    system_seq_executor& system_seq_executor::operator=(system_seq_executor&&) noexcept = default;

    void system_seq_executor::update(const system_update_context& ctx)
    {
        const auto firstUpdate = m_modificationId == 0;
        const auto updateFunc = firstUpdate ? &system_descriptor::firstUpdate : &system_descriptor::update;

        OBLO_PROFILE_SCOPE();

        for (const auto& [desc, system] : m_systems)
        {
            OBLO_PROFILE_SCOPE("Update");
            OBLO_PROFILE_TAG(desc.name)

            ctx.entities->set_modification_id(++m_modificationId);

            (desc.*updateFunc)(system, &ctx);
        }
    }

    void system_seq_executor::shutdown()
    {
        for (const auto& [desc, system] : m_systems)
        {
            desc.destroy(system);
        }

        m_systems.clear();
    }

    void system_seq_executor::push(const system_descriptor& desc)
    {
        void* const system = desc.create();
        m_systems.emplace_back(desc, system);
    }

    void system_seq_executor::reserve(usize capacity)
    {
        m_systems.reserve(capacity);
    }
}
