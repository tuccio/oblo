#include <oblo/dotnet/dotnet_behaviour_module.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/dotnet/resources/dotnet_assembly.hpp>
#include <oblo/dotnet/systems/dotnet_behaviour_system.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/reflection/codegen/registration.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/providers/resource_types_provider.hpp>

namespace oblo::barriers
{
    struct transform_update;
}

namespace oblo
{
    namespace
    {
        class dotnet_resource_types_provider final : public resource_types_provider
        {
        public:
            void fetch_resource_types(deque<resource_type_descriptor>& outResourceTypes) const
            {
                outResourceTypes.push_back({
                    .typeId = get_type_id<dotnet_assembly>(),
                    .typeUuid = resource_type<dotnet_assembly>,
                    .create = []() -> void* { return new dotnet_assembly{}; },
                    .destroy = [](void* ptr) { delete static_cast<dotnet_assembly*>(ptr); },
                    .load =
                        [](void* ptr, cstring_view source, const any&)
                    {
                        auto* const bc = static_cast<dotnet_assembly*>(ptr);

                        if (!filesystem::load_binary_file_into_memory(bc->assembly, source))
                        {
                            return false;
                        }

                        return true;
                    },
                });
            }
        };
    }

    bool dotnet_behaviour_module::startup(const module_initializer& initializer)
    {
        reflection::gen::load_module_and_register();

        initializer.services->add<ecs::world_builder>().unique({
            .systems =
                [](ecs::system_graph_builder& b)
            {
                if (!b.usages().contains("no_scripts"_hsv))
                {
                    b.add_system<dotnet_behaviour_system>().before<barriers::transform_update>();
                }
            },
        });

        initializer.services->add<dotnet_resource_types_provider>().as<resource_types_provider>().unique();

        return true;
    }

    bool dotnet_behaviour_module::finalize()
    {
        return true;
    }

    void dotnet_behaviour_module::shutdown() {}
}

OBLO_MODULE_REGISTER(oblo::dotnet_behaviour_module)