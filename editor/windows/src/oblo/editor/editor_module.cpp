#include <oblo/editor/editor_module.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/editor/modules/gizmo_module.hpp>
#include <oblo/editor/window_module.hpp>
#include <oblo/modules/module_initializer.hpp>

namespace oblo::editor
{
    struct editor_modules_provider final : window_modules_provider
    {
        void fetch_window_modules(dynamic_array<unique_ptr<window_module>>& outWindowModules) const override
        {
            outWindowModules.push_back(allocate_unique<gizmo_module>());
        }
    };

    bool editor_module::startup(const module_initializer& initializer)
    {
        initializer.services->add<editor_modules_provider>().as<window_modules_provider>().unique();
        return true;
    }

    void editor_module::shutdown() {}
}
