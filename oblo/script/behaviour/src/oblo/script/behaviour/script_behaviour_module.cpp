#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/ecs/services/world_builder.hpp>
#include <oblo/ecs/systems/system_graph_builder.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/options/options_provider.hpp>
#include <oblo/reflection/codegen/registration.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/providers/resource_types_provider.hpp>
#include <oblo/script/behaviour/options.hpp>
#include <oblo/script/behaviour/script_behaviour_system.hpp>
#include <oblo/script/resources/compiled_script.hpp>
#include <oblo/script/resources/traits.hpp>

namespace oblo::barriers
{
    struct transform_update;
}

namespace oblo
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
                        [](void* r, cstring_view source, const any&)
                    {
                        auto* script = reinterpret_cast<compiled_script*>(r);
                        return load(*script, source);
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

            initializer.services->add<ecs::world_builder>().unique({
                .systems =
                    [](ecs::system_graph_builder& b)
                {
                    if (!b.usages().contains("no_scripts"_hsv))
                    {
                        b.add_system<script_behaviour_system>().before<barriers::transform_update>();
                    }
                },
            });

            option_proxy_struct<script_behaviour_options>::register_options(*initializer.services);

            return true;
        }

        bool finalize() override
        {
            return true;
        }

        void shutdown() override {}
    };
}

OBLO_MODULE_REGISTER(oblo::script_resource_module)