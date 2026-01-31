#include <oblo/options/options_module.hpp>

#include <oblo/modules/module_manager.hpp>
#include <oblo/options/options_provider.hpp>
#include <oblo/properties/serialization/data_document.hpp>

namespace oblo
{
    bool options_module::startup(const module_initializer&)
    {
        return true;
    }

    void options_module::shutdown() {}

    bool options_module::finalize()
    {
        dynamic_array<options_layer_descriptor> layers;
        deque<options_layer_provider_descriptor> moduleLayers;

        // Gather all layers
        const std::span layersProviders = module_manager::get().find_services<options_layer_provider>();

        for (const auto& p : layersProviders)
        {
            moduleLayers.clear();
            p->fetch(moduleLayers);

            for (auto& layerDesc : moduleLayers)
            {
                layers.push_back(layerDesc.layer);
            }
        }

        // Init with all gathered layers
        m_manager.init(layers);

        // Now register all options
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

        // Finally load all layers
        for (const auto& p : layersProviders)
        {
            moduleLayers.clear();
            p->fetch(moduleLayers);

            for (auto& layerDesc : moduleLayers)
            {
                if (layerDesc.load)
                {
                    data_document doc;

                    if (layerDesc.load(doc, layerDesc.userdata))
                    {
                        const auto layer = m_manager.find_layer(layerDesc.layer.id);
                        m_manager.load_layer(doc, doc.get_root(), layer);
                    }
                }

                layers.push_back(layerDesc.layer);
            }
        }

        return true;
    }
}