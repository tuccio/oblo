#pragma once

namespace oblo
{
    class service_registry;

    struct module_initializer
    {
        service_registry* services;
    };
}