#include <oblo/core/array_size.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/reflection/codegen/registration.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/providers/resource_types_provider.hpp>
#include <oblo/script/resources/compiled_script.hpp>
#include <oblo/script/resources/traits.hpp>

#include <array>

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
                    .load = [](void* r, cstring_view source, const any&) -> expected<>
                    {
                        if (!load(*static_cast<compiled_script*>(r), source))
                        {
                            return "Failed to load"_err;
                        }

                        return no_error;
                    },
                });

                outResourceTypes.push_back({
                    .typeId = get_type_id<compiled_native_module>(),
                    .typeUuid = resource_type<compiled_native_module>,
                    .create = []() -> void* { return new compiled_native_module{}; },
                    .destroy = [](void* ptr) { delete static_cast<compiled_native_module*>(ptr); },
                    .load = [](void* r, cstring_view source, const any&) -> expected<>
                    {
                        if (!load(*static_cast<compiled_native_module*>(r), source))
                        {
                            return "Failed to load"_err;
                        }

                        return no_error;
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

    bool save(const compiled_script& script, cstring_view destination)
    {
        data_document doc;
        doc.init();

        doc.child_value(doc.get_root(), "x86_64_avx2"_hsv, property_value_wrapper{script.x86_64_avx2.id});

        return json::write(doc, destination).has_value();
    }

    bool load(compiled_script& script, cstring_view source)
    {
        data_document doc;
        doc.init();

        if (!json::read(doc, source))
        {
            return false;
        }

        const u32 x86_64_avx2 = doc.find_child(doc.get_root(), "x86_64_avx2"_hsv);

        doc.child_value(doc.get_root(), "x86_64_avx2"_hsv, property_value_wrapper{script.x86_64_avx2.id});

        script = {
            .x86_64_avx2 = {doc.read_uuid(x86_64_avx2).value_or(uuid{})},
        };

        return true;
    }

    bool load(compiled_native_module& script, cstring_view source)
    {
        return script.module.open(source, platform::shared_library::open_flags::exact_name);
    }
}

OBLO_MODULE_REGISTER(oblo::script_resource_module)