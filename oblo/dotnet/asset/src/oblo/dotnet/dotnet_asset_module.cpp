#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/descriptors/file_importer_descriptor.hpp>
#include <oblo/asset/import/file_importer.hpp>
#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/import_config.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/asset/import/import_preview.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/platform/file.hpp>
#include <oblo/core/platform/process.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/dotnet/assets/dotnet_script_asset.hpp>
#include <oblo/dotnet/resources/dotnet_assembly.hpp>
#include <oblo/dotnet/utility/dotnet_utility.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>

namespace oblo
{
    namespace
    {
        class dotnet_script_importer : public file_importer
        {
            static constexpr cstring_view g_ArtifactName = "ScriptAssembly.dll";

        public:
            static constexpr string_view extensions[] = {".cs"};

            bool init(const import_config& config, import_preview& preview)
            {
                m_source = config.sourceFile;

                auto& n = preview.nodes.emplace_back();
                n.artifactType = resource_type<dotnet_assembly>;
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

                string_builder destination;
                ctx.get_output_path(nodeConfig.id, destination);

                if (!filesystem::create_directories(destination))
                {
                    return false;
                }

                string_builder sourceFile = destination;
                const auto sourceFileName = filesystem::filename(m_source.as<string_view>());
                sourceFile.append_path(sourceFileName);

                if (!filesystem::copy_file(m_source, sourceFile))
                {
                    return false;
                }

                string_builder csproj = destination;
                csproj.append_path("ScriptAssembly.csproj");

                if (!dotnet_utility::generate_csproj(csproj))
                {
                    return false;
                }

                {
#if OBLO_DEBUG
                    constexpr cstring_view buildConfig = "Debug";
#else
                    constexpr cstring_view buildConfig = "Release";
#endif

                    constexpr cstring_view buildArgs[] = {
                        "build",
                        "./ScriptAssembly.csproj",
                        "-o",
                        "./bin",
                        "-c",
                        buildConfig,
                    };

                    string_builder compilerOutput;

                    bool usePipe = true;
                    platform::file rPipe, wPipe;

                    constexpr u32 pipeSize = 1u << 12;

                    if (!platform::file::create_pipe(rPipe, wPipe, pipeSize))
                    {
                        log::warn("Failed to open pipe, .NET compiler errors will not be logged");
                        usePipe = false;
                    }

                    platform::process buildProcess;

                    if (!buildProcess.start({
                            .path = "dotnet",
                            .arguments = buildArgs,
                            .workDir = destination,
                            .outputStream = usePipe ? &wPipe : nullptr,
                            .errorStream = usePipe ? &wPipe : nullptr,
                        }))
                    {
                        return false;
                    }

                    // This was inherited by the process we started, we can close it
                    wPipe.close();

                    string_builder errors;

                    for (char readBuffer[pipeSize];;)
                    {
                        const bool processDone = buildProcess.is_done();

                        const auto res = rPipe.read(readBuffer, pipeSize);

                        if (!res)
                        {
                            break;
                        }

                        if (const auto bytes = *res; bytes > 0)
                        {
                            errors.append(string_view{readBuffer, bytes});
                        }

                        if (processDone)
                        {
                            break;
                        }
                    }

                    if (!buildProcess.wait())
                    {
                        return false;
                    }

                    if (const auto r = buildProcess.get_exit_code(); !r || *r != 0)
                    {
                        log::error("Failed to compile {}:\n{}", m_source, errors);
                        return false;
                    }
                }

                string_builder assemblyPath = destination;
                assemblyPath.append_path("bin").append_path(g_ArtifactName);

                m_artifact.id = nodeConfig.id;
                m_artifact.name = g_ArtifactName;
                m_artifact.path = assemblyPath.as<string>();
                m_artifact.type = resource_type<dotnet_assembly>;

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

        class dotnet_asset_provider final : public native_asset_provider
        {
            void fetch(deque<native_asset_descriptor>& out) const override
            {
                out.push_back({
                    .typeUuid = asset_type<dotnet_script_asset>,
                    .typeId = get_type_id<dotnet_script_asset>(),
                    .fileExtension = ".cs",
                    .load =
                        [](any_asset& asset, cstring_view source)
                    {
                        auto& m = asset.emplace<dotnet_script_asset>();
                        return filesystem::load_text_file_into_memory(m.code(), source).has_value();
                    },
                    .save =
                        [](const any_asset& asset, cstring_view destination, cstring_view)
                    {
                        auto* const s = asset.as<dotnet_script_asset>();

                        if (!s)
                        {
                            return false;
                        }

                        return filesystem::write_file(destination, as_bytes(std::span{s->code()}), {}).has_value();
                    },
                    .createImporter = []() -> unique_ptr<file_importer>
                    { return allocate_unique<dotnet_script_importer>(); },
                });
            }
        };
    }

    class dotnet_asset_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            initializer.services->add<dotnet_asset_provider>().as<native_asset_provider>().unique();
            return true;
        }

        bool finalize() override
        {
            return true;
        }

        void shutdown() {}
    };

}

OBLO_MODULE_REGISTER(oblo::dotnet_asset_module)