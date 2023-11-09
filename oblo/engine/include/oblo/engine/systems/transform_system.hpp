#pragma once

namespace oblo
{
    namespace ecs
    {
        struct system_update_context;
    }

    class ENGINE_API transform_system
    {
    public:
        void update(const ecs::system_update_context& ctx);
    };
}