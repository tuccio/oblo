#pragma once

#include <coroutine>

#include <oblo/ecs/forward.hpp>

namespace oblo
{
    class asset_registry;
    class resource_registry;
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

        const resource_registry& get_resource_registry() const;

        ecs::entity_registry& get_entity_registry() const;

        ecs::entity get_camera_entity() const;

        std::suspend_always next_frame() const;

        void request_renderdoc_capture() const;

    private:
        friend class test_fixture;

    private:
        explicit test_context(test_context_impl* impl);

    private:
        test_context_impl* m_impl{};
    };
}