#include <oblo/scene/scene_module.hpp>

#include <oblo/engine/engine_module.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/scene/assets/registration.hpp>

namespace oblo::scene
{
    bool scene_module::startup()
    {
        auto* const engineModule = module_manager::get().load<engine::engine_module>();
        register_asset_types(engineModule->get_asset_registry());
        register_resource_types(engineModule->get_resource_registry());
        return true;
    }

    void scene_module::shutdown()
    {
        auto* const engineModule = module_manager::get().find<engine::engine_module>();
        unregister_asset_types(engineModule->get_asset_registry());
        unregister_resource_types(engineModule->get_resource_registry());
    }
}