#include <oblo/core/service_registry_builder.hpp>

#include <algorithm>

namespace oblo
{
    expected<success_tag, service_build_error> service_registry_builder::build(service_registry& serviceRegistry)
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
}