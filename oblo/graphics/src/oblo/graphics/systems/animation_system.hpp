#pragma once

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo
{
    class property_registry;
    class resource_registry;
    class resource_cache;

    class animation_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        const resource_registry* m_resourceRegistry{};
        const property_registry* m_propertyRegistry{};
    };
}