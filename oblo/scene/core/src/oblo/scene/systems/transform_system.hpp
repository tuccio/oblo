#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    namespace ecs
    {
        struct system_update_context;
    }

    class transform_system
    {
    public:
        void update(const ecs::system_update_context& ctx);

    private:
        // We initialize to 1 to avoid underflow
        u64 m_lastModificationId{0};
    };
}