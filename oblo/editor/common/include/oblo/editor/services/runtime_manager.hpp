#pragma once

namespace oblo
{
    class runtime;

    class runtime_manager
    {
    public:
        virtual ~runtime_manager() = default;

        virtual runtime* create() = 0;
        virtual void destroy(runtime* r) = 0;
    };
}