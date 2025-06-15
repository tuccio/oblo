#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/reflection/codegen/registration.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/providers/resource_types_provider.hpp>
#include <oblo/script/resources/compiled_script.hpp>
#include <oblo/script/resources/traits.hpp>

namespace oblo::script
{
    namespace
    {
        class script_resource_types_provider final : public resource_types_provider
        {
        public:
            void fetch_resource_types(deque<resource_type_descriptor>& outResourceTypes) const
            {
                outResourceTypes.push_back({
                    .typeId = get_type_id<compiled_script>(),
                    .typeUuid = resource_type<compiled_script>,
                    .create = []() -> void* { return new compiled_script{}; },
                    .destroy = [](void* ptr) { delete static_cast<compiled_script*>(ptr); },
                    .load =
                        [](void*, cstring_view, const any&)
                    {
                        // TODO

                        return true;
                    },
                });
            }
        };
    }

    class script_resource_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            reflection::gen::load_module_and_register();

            initializer.services->add<script_resource_types_provider>().as<resource_types_provider>().unique();
            return true;
        }

        bool finalize() override
        {
            return true;
        }

        void shutdown() override {}
    };
}

OBLO_MODULE_REGISTER(oblo::script::script_resource_module)