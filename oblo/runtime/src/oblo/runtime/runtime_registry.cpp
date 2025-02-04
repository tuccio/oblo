#include <oblo/runtime/runtime_registry.hpp>

#include <oblo/resource/resource_registry.hpp>

namespace oblo
{
    struct runtime_registry::impl
    {
        property_registry* propertyRegistry;
        resource_registry resourceRegistry;
    };

    runtime_registry::runtime_registry(property_registry* propertyRegistry) : m_impl{std::make_unique<impl>()}
    {
        m_impl->propertyRegistry = propertyRegistry;
    }

    runtime_registry::runtime_registry() = default;

    runtime_registry::runtime_registry(runtime_registry&&) noexcept = default;

    runtime_registry::~runtime_registry() = default;

    runtime_registry& runtime_registry::operator=(runtime_registry&&) noexcept = default;

    property_registry& runtime_registry::get_property_registry()
    {
        return *m_impl->propertyRegistry;
    }

    resource_registry& runtime_registry::get_resource_registry()
    {
        return m_impl->resourceRegistry;
    }
}