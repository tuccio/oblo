#include <oblo/asset/descriptors/file_importer_descriptor.hpp>
#include <oblo/asset/import/file_importer.hpp>
#include <oblo/asset/import/file_importers_provider.hpp>
#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/import_config.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/asset/import/import_preview.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/luau/resources/luau_bytecode.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>

#include <luacode.h>

namespace oblo
{
    namespace
    {
        class luau_script_importer : public file_importer
        {
            static constexpr cstring_view g_ArtifactName = "script";

        public:
            static constexpr string_view extensions[] = {".luau"};

            bool init(const import_config& config, import_preview& preview)
            {
                m_source = config.sourceFile;

                auto& n = preview.nodes.emplace_back();
                n.artifactType = resource_type<luau_bytecode>;
                n.name = g_ArtifactName;

                return true;
            }

            bool import(import_context ctx)
            {
                const std::span configs = ctx.get_import_node_configs();

                const auto& nodeConfig = configs[0];

                if (!nodeConfig.enabled)
                {
                    return true;
                }

                string_builder sourceCode;

                if (!filesystem::load_text_file_into_memory(sourceCode, m_source))
                {
                    return false;
                }

                using bytecode_ptr = unique_ptr<char, decltype([](char* bc) { free(bc); })>;

                usize byteCodeSize;
                const bytecode_ptr byteCode{luau_compile(sourceCode.data(), sourceCode.size(), nullptr, &byteCodeSize)};

                if (!byteCode)
                {
                    return false;
                }

                string_builder destination;
                ctx.get_output_path(nodeConfig.id, destination);

                if (!filesystem::write_file(destination, as_bytes(std::span{byteCode.get(), byteCodeSize}), {}))
                {
                    return false;
                }

                m_artifact.id = nodeConfig.id;
                m_artifact.name = g_ArtifactName;
                m_artifact.path = destination.as<string>();
                m_artifact.type = resource_type<luau_bytecode>;

                return true;
            }

            file_import_results get_results()
            {
                file_import_results r;
                r.artifacts = {&m_artifact, 1};
                r.sourceFiles = {&m_source, 1};
                r.mainArtifactHint = m_artifact.id;
                return r;
            }

        private:
            import_artifact m_artifact{};
            string m_source;
        };

        class importers_provider final : public file_importers_provider
        {
        public:
            void fetch_importers(dynamic_array<file_importer_descriptor>& outImporters) const override
            {
                outImporters.push_back(file_importer_descriptor{
                    .type = get_type_id<luau_script_importer>(),
                    .create = []() -> unique_ptr<file_importer> { return allocate_unique<luau_script_importer>(); },
                    .extensions = luau_script_importer::extensions,
                });
            }
        };
    }

    class luau_asset_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            initializer.services->add<importers_provider>().as<file_importers_provider>().unique();
            return true;
        }

        bool finalize() override
        {
            return true;
        }

        void shutdown() {}
    };

}

OBLO_MODULE_REGISTER(oblo::luau_asset_module)