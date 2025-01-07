#include <oblo/options/options_module.hpp>

namespace oblo
{
    bool options_module::startup(const module_initializer&)
    {
        // Nothing to do for now, but we could consider registering the layers automatically at some point
        return true;
    }

    void options_module::shutdown() {}
}