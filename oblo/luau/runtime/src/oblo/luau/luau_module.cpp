#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/luau/resources/luau_bytecode.hpp>
#include <oblo/luau/systems/luau_behaviour_system.hpp>
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
        class luau_resource_types_provider final : public resource_types_provider
        {
        public:
            void fetch_resource_types(deque<resource_type_descriptor>& outResourceTypes) const
            {
                outResourceTypes.push_back({
                    .typeId = get_type_id<luau_bytecode>(),
                    .typeUuid = resource_type<luau_bytecode>,
                    .create = []() -> void* { return new luau_bytecode{}; },
                    .destroy = [](void* ptr) { delete static_cast<luau_bytecode*>(ptr); },
                    .load =
                        [](void* ptr, cstring_view source)
                    {
                        auto* const bc = static_cast<luau_bytecode*>(ptr);

                        if (!filesystem::load_binary_file_into_memory(bc->byteCode, source))
                        {
                            return false;
                        }

                        return true;
                    },
                });
            }
        };
    }

    class luau_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            reflection::gen::load_module_and_register();

            initializer.services->add<ecs::world_builder>().unique({
                .systems = [](ecs::system_graph_builder& b)
                { b.add_system<luau_behaviour_system>().before<barriers::transform_update>(); },
            });

            initializer.services->add<luau_resource_types_provider>().as<resource_types_provider>().unique();

            return true;
        }

        bool finalize() override
        {
            return true;
        }

        void shutdown() {}
    };

}

OBLO_MODULE_REGISTER(oblo::luau_module)