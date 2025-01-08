#include <oblo/options/options_module.hpp>

#include <oblo/modules/module_manager.hpp>
#include <oblo/options/options_provider.hpp>

namespace oblo
{
    bool options_module::startup(const module_initializer&)
    {
        return true;
    }

    void options_module::shutdown() {}

    void options_module::finalize()
    {
        dynamic_array<options_layer_descriptor> layers;
        deque<options_layer_descriptor> moduleLayers;

        const std::span layersProviders = module_manager::get().find_services<options_layer_provider>();

        for (const auto& p : layersProviders)
        {
            moduleLayers.clear();
            p->fetch(moduleLayers);

            layers.append(moduleLayers.begin(), moduleLayers.end());
        }

        m_manager.init(layers);

        deque<option_descriptor> moduleOptions;

        const std::span optionsProviders = module_manager::get().find_services<options_provider>();

        for (const auto& p : optionsProviders)
        {
            moduleOptions.clear();
            p->fetch(moduleOptions);

            for (auto& opt : moduleOptions)
            {
                m_manager.register_option(opt);
            }
        }
    }
}