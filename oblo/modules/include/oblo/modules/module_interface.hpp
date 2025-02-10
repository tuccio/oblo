#pragma once

namespace oblo
{
    struct module_initializer;

    class MODULES_API module_interface
    {
    public:
        virtual ~module_interface() = default;

        [[nodiscard]] virtual bool startup(const module_initializer& initializer) = 0;

        virtual [[nodiscard]] bool finalize() = 0;

        virtual void shutdown() = 0;
    };
}