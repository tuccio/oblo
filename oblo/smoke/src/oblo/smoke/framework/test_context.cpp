#include <oblo/smoke/framework/test_context.hpp>

#include <oblo/smoke/framework/test_context_impl.hpp>

namespace oblo::smoke
{
    test_context::test_context(const test_context_impl* impl) : m_impl{impl} {}

    asset_registry& test_context::get_asset_registry() const
    {
        return *m_impl->assetRegistry;
    }

    resource_registry& test_context::get_resource_registry() const
    {
        return *m_impl->resourceRegistry;
    }

    ecs::entity_registry& test_context::get_entity_registry() const
    {
        return *m_impl->entities;
    }

    std::suspend_always test_context::next_frame() const
    {
        return {};
    }
}