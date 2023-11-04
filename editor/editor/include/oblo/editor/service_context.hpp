#pragma once

#include <oblo/core/service_registry.hpp>

namespace oblo::editor
{
    class service_context
    {
    public:
        service_context() = default;
        service_context(const service_context&) = default;
        service_context(service_context&&) noexcept = default;

        service_context(const service_context* inherited, service_registry* local) :
            m_inherited{inherited}, m_local{local}
        {
        }

        service_context& operator=(const service_context&) = default;
        service_context& operator=(service_context&&) noexcept = default;

        template <typename T>
        T* find() const;

        service_registry* get_local_registry() const;

    private:
        const service_context* m_inherited{};
        service_registry* m_local{};
    };

    template <typename T>
    T* service_context::find() const
    {
        if (m_local)
        {
            T* const local = m_local->find<T>();

            if (local)
            {
                return local;
            }
        }

        if (m_inherited)
        {
            return m_inherited->find<T>();
        }

        return nullptr;
    }

    inline service_registry* service_context::get_local_registry() const
    {
        return m_local;
    }
}