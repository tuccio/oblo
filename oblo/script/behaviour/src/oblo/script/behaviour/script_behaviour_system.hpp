#pragma once

#include <oblo/core/unique_ptr.hpp>
#include <oblo/ecs/forward.hpp>

namespace oblo
{
    class resource_registry;

    class script_behaviour_system
    {
    public:
        script_behaviour_system();
        script_behaviour_system(const script_behaviour_system&) = delete;
        script_behaviour_system(script_behaviour_system&&) noexcept = delete;
        ~script_behaviour_system();

        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);
        void shutdown();

    private:
        class script_api_impl;

    private:
        const resource_registry* m_resourceRegistry{};
        unique_ptr<script_api_impl> m_scriptApi;
        bool useNativeRuntime{};
    };
}