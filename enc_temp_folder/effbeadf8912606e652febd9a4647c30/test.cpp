#include <oblo/core/service_registry_builder.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/scene/systems/usages.hpp>

#include <oblo/reflection/codegen/registration.hpp>

namespace oblo
{
    namespace
    {
        struct test_system
        {
            void update(const ecs::system_update_context&)
            {
                log::debug("System update 42!!!");
            }
        };
    }

    class default_project : public module_interface
    {
    public:
        intrusive_list<ecs::system_trampoline> test_system_instances;

    public:
        [[nodiscard]] bool startup(const module_initializer& initializer) override
        {
            reflection::gen::load_module_and_register();

            initializer.services->add<ecs::world_builder>().unique({
                .systems =
                    [](ecs::system_graph_builder& builder)
                {
                    if (builder.usages().contains(system_graph_usages::play_mode))
                    {
                        auto* const self = static_cast<default_project*>(
                            module_manager::get().find(OBLO_STRINGIZE(OBLO_PROJECT_NAME)));

                        builder.add_system_trampoline<test_system>(&self->test_system_instances);
                    }
                },
            });

            return true;
        }

        [[nodiscard]] bool finalize() override
        {
            return true;
        }

        void shutdown() override {}
    };
}

OBLO_MODULE_REGISTER(oblo::default_project);

OBLO_MODULE_HOTRELOAD_FN(oblo::module_interface* module)
{
    using namespace oblo;

    auto* const self = static_cast<default_project*>(module);
    ecs::swap_system_trampolines(self->test_system_instances, ecs::make_system_descriptor<test_system>());
}