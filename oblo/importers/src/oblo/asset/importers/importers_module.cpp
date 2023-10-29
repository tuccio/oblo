#include <oblo/asset/importers/importers_module.hpp>

#include <oblo/asset/importers/registration.hpp>
#include <oblo/engine/engine_module.hpp>
#include <oblo/modules/module_manager.hpp>

namespace oblo::importers
{
    bool importers_module::startup()
    {
        auto* const engineModule = module_manager::get().load<engine::engine_module>();
        register_gltf_importer(engineModule->get_asset_registry());
        return true;
    }

    void importers_module::shutdown()
    {
        auto* const engineModule = module_manager::get().find<engine::engine_module>();
        unregister_gltf_importer(engineModule->get_asset_registry());
    }
}