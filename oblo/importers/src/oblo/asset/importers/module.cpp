#include <oblo/asset/importers/module.hpp>

#include <oblo/asset/importers/registration.hpp>
#include <oblo/engine/module.hpp>
#include <oblo/modules/module_manager.hpp>

namespace oblo::asset::importers
{
    bool module::startup()
    {
        auto* const engineModule = module_manager::get().load<engine::module>();
        register_gltf_importer(engineModule->get_asset_registry());
        return true;
    }

    void module::shutdown()
    {
        auto* const engineModule = module_manager::get().find<engine::module>();
        unregister_gltf_importer(engineModule->get_asset_registry());
    }
}