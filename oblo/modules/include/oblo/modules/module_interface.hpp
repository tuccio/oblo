#pragma once

namespace oblo
{
    struct module_initializer;

    class OBLO_MODULES_API module_interface
    {
    public:
        virtual ~module_interface() = default;

        [[nodiscard]] virtual bool startup(const module_initializer& initializer) = 0;

        [[nodiscard]] virtual bool finalize() = 0;

        virtual void shutdown() = 0;
    };
}