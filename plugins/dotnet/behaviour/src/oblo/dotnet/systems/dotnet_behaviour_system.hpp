#pragma once

#include <oblo/ecs/forward.hpp>

namespace oblo
{
    class resource_registry;

    class dotnet_behaviour_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);
        void shutdown();

    private:
        struct update_ctx;

    private:
        using create_system_fn = void* (*) ();
        using destroy_system_fn = void (*)(void*);
        using register_behaviour_fn = void (*)(void*, u32, const void*, u32);
        using update_system_fn = void (*)(void*, update_ctx);

    private:
        const resource_registry* m_resourceRegistry{};
        create_system_fn m_create{};
        destroy_system_fn m_destroy{};
        register_behaviour_fn m_registerBehaviour{};
        update_system_fn m_update{};
        void* m_managedSystem{};
    };
}