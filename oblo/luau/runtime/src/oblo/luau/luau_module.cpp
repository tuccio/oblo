#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/luau/systems/luau_behaviour_system.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>

namespace oblo::barriers
{
    struct transform_update;
}

namespace oblo
{
    class luau_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            initializer.services->add<ecs::world_builder>().unique({
                .systems = [](ecs::system_graph_builder& b)
                { b.add_system<luau_behaviour_system>().before<barriers::transform_update>(); },
            });

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