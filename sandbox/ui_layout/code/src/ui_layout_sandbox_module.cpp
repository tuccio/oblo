#include <ui_layout_sandbox_system.hpp>

#include <oblo/core/service_registry_builder.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/ecs/systems/system_graph_usages.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/reflection/codegen/registration.hpp>
#include <oblo/scene/systems/barriers.hpp>
#include <oblo/scene/systems/usages.hpp>

namespace oblo
{
    class viewport_system;
}

namespace oblo::sandbox
{
    class ui_layout_sandbox_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            reflection::gen::load_module_and_register();

            initializer.services->add<ecs::world_builder>().unique({
                .systems =
                    [](ecs::system_graph_builder& builder)
                {
                    if (!builder.usages().contains(system_graph_usages::no_scripts))
                    {
                        builder.add_system<ui_layout_sandbox_system>()
                            .after<barriers::renderer_extract>()
                            .after<viewport_system>()
                            .before<barriers::renderer_update>();
                    }
                },
            });

            log::info("UI Layout sandbox module loaded");
            return true;
        }

        bool finalize() override
        {
            return true;
        }

        void shutdown() override {}
    };
}

OBLO_MODULE_REGISTER(oblo::sandbox::ui_layout_sandbox_module)