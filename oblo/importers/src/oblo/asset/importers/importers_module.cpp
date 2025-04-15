#include <oblo/asset/importers/importers_module.hpp>

#include <oblo/asset/descriptors/file_importer_descriptor.hpp>
#include <oblo/asset/import/file_importers_provider.hpp>
#include <oblo/asset/importers/registration.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/modules/utility/registration.hpp>

namespace oblo::importers
{
    namespace
    {
        class importers_provider final : public file_importers_provider
        {
        public:
            void fetch_importers(dynamic_array<file_importer_descriptor>& outImporters) const override
            {
                oblo::importers::fetch_importers(outImporters);
            }
        };
    }

    bool importers_module::startup(const module_initializer& initializer)
    {
        initializer.services->add<importers_provider>().as<file_importers_provider>().unique();
        return true;
    }

    void importers_module::shutdown() {}
}

OBLO_MODULE_REGISTER(oblo::importers::importers_module)