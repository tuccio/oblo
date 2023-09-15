#include <oblo/scene/module.hpp>

#include <oblo/engine/module.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/scene/assets/registration.hpp>

namespace oblo::scene
{
    bool module::startup()
    {
        auto* const engineModule = module_manager::get().load<engine::module>();
        register_asset_types(engineModule->get_asset_registry());
        register_resource_types(engineModule->get_resource_registry());
        return true;
    }

    void module::shutdown()
    {
        auto* const engineModule = module_manager::get().find<engine::module>();
        unregister_asset_types(engineModule->get_asset_registry());
        unregister_resource_types(engineModule->get_resource_registry());
    }
}