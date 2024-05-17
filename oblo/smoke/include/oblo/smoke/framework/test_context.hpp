#pragma once

#include <coroutine>

namespace oblo
{
    class asset_registry;
    class resource_registry;

    namespace ecs
    {
        class entity_registry;
    }
}

namespace oblo::smoke
{
    struct test_context_impl;

    class test_context
    {
    public:
        test_context() = delete;
        test_context(const test_context&) = delete;
        test_context(test_context&&) noexcept = delete;

        test_context& operator=(const test_context&) = delete;
        test_context& operator=(test_context&&) noexcept = delete;

        ~test_context() = default;

        asset_registry& get_asset_registry() const;

        resource_registry& get_resource_registry() const;

        ecs::entity_registry& get_entity_registry() const;

        std::suspend_always next_frame() const;

    private:
        friend class test_fixture;

    private:
        explicit test_context(const test_context_impl* impl);

    private:
        const test_context_impl* m_impl{};
    };
}