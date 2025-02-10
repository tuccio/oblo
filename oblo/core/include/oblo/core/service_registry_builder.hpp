#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/service_registry.hpp>

#include <algorithm>
#include <tuple>

namespace oblo
{
    template <typename T>
    struct service_requires
    {
    };

    enum class service_build_error
    {
        missing_dependency,
    };

    class service_registry_builder
    {
    public:
        template <typename Builder, typename... Dependencies>
        void add(void (*build)(Builder, Dependencies&...))
        {
            const auto fn = +[](service_registry& registry, void* userdata)
            {
                auto cb = reinterpret_cast<void (*)(Builder, Dependencies&...)>(userdata);

                const std::tuple requirements{registry.find<Dependencies>()...};

                const bool allSatisfied = ((std::get<Dependencies*>(requirements) != nullptr) && ...);

                if (!allSatisfied)
                {
                    return false;
                }

                cb(registry.add<typename Builder::service_type>(), (*std::get<Dependencies*>(requirements))...);
                return true;
            };

            m_builders.emplace_back(fn, build, sizeof...(Dependencies));
        }

        expected<success_tag, service_build_error> build(service_registry& serviceRegistry)
        {
            std::sort(m_builders.begin(),
                m_builders.end(),
                [](const builder_info& lhs, const builder_info& rhs) { return lhs.requirements < rhs.requirements; });

            for (auto& b : m_builders)
            {
                if (!b.build(serviceRegistry, b.userdata))
                {
                    return service_build_error::missing_dependency;
                }
            }

            return no_error;
        }

    private:
        struct builder_info
        {
            bool (*build)(service_registry&, void* userdata);
            void* userdata;
            usize requirements;
        };

    private:
        deque<builder_info> m_builders;
    };
}