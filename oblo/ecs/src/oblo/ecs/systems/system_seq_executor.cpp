#include <oblo/ecs/systems/system_seq_executor.hpp>

#include <oblo/ecs/systems/system_descriptor.hpp>

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
        const auto updateFunc = m_firstUpdate ? &system_descriptor::firstUpdate : &system_descriptor::update;
        m_firstUpdate = false;

        for (const auto& [desc, system] : m_systems)
        {
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
