#pragma once

namespace oblo
{
    class module_interface
    {
    public:
        virtual ~module_interface() = default;

        [[nodiscard]] virtual bool startup() = 0;
        virtual void shutdown() = 0;
    };
}