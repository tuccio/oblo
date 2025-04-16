#include <oblo/dotnet/dotnet_module.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/dotnet/dotnet_runtime.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/providers/resource_types_provider.hpp>

namespace oblo::barriers
{
    struct transform_update;
}

namespace oblo
{
    bool dotnet_module::startup(const module_initializer&)
    {
        return m_runtime.init().has_value();
    }

    bool dotnet_module::finalize()
    {
        return true;
    }

    void dotnet_module::shutdown()
    {
        m_runtime.shutdown();
    }

    const dotnet_runtime& dotnet_module::get_runtime() const
    {
        return m_runtime;
    }
}

OBLO_MODULE_REGISTER(oblo::dotnet_module)