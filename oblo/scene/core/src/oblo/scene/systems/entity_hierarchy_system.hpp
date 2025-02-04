#pragma once

namespace oblo
{
    namespace ecs
    {
        struct system_update_context;
    }

    class resource_registry;

    class entity_hierarchy_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        const resource_registry* m_resourceRegistry{};
    };
}