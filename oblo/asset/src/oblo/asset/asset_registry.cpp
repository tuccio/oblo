#include <oblo/asset/asset_registry.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_type_desc.hpp>
#include <oblo/asset/import_artifact.hpp>
#include <oblo/asset/import_preview.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/asset/meta.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/core/uuid_generator.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace oblo::asset
{
    namespace
    {
        struct asset_type_info : asset_type_desc
        {
        };

        struct file_importer_info
        {
            create_file_importer_fn create;
            std::vector<std::string> extensions;
        };

        void save_asset_meta(const asset_meta& meta, const std::filesystem::path& destination)
        {
            nlohmann::json json;
            json["type"] = meta.type.name;

            auto artifacts = nlohmann::json::array();

            char uuidBuffer[36];

            for (auto& artifact : meta.artifacts)
            {
                nlohmann::json jsonArtifact;

                artifact.id.format_to(uuidBuffer);

                jsonArtifact["id"] = std::string_view{uuidBuffer, array_size(uuidBuffer)};
                jsonArtifact["name"] = artifact.name;

                artifacts.emplace_back(std::move(jsonArtifact));
            }

            json["artifacts"] = std::move(artifacts);

            std::ofstream ofs{destination};
            ofs << json.dump(4);
        }

        bool ensure_directories(const std::filesystem::path& directory)
        {
            std::error_code ec;
            std::filesystem::create_directories(directory, ec);
            return std::filesystem::is_directory(directory, ec);
        }

        constexpr std::string_view assetExt{".oasset"};
    }

    struct asset_registry::impl
    {
        uuid_random_generator uuidGenerator;
        std::unordered_map<type_id, asset_type_info> assetTypes;
        std::unordered_map<type_id, file_importer_info> importers;
        std::unordered_map<uuid, asset_meta> assets;
        std::filesystem::path assetsDir;
        std::filesystem::path artifactsDir;
    };

    asset_registry::asset_registry() = default;

    asset_registry::~asset_registry()
    {
        shutdown();
    }

    bool asset_registry::initialize(const std::filesystem::path& assetsDir, const std::filesystem::path& artifactsDir)
    {
        if (!ensure_directories(assetsDir) || !ensure_directories(artifactsDir))
        {
            return false;
        }

        m_impl = std::make_unique<impl>();
        m_impl->assetsDir = assetsDir;
        m_impl->artifactsDir = artifactsDir;

        return true;
    }

    void asset_registry::shutdown()
    {
        m_impl.reset();
    }

    void asset_registry::register_type(const asset_type_desc& desc)
    {
        m_impl->assetTypes.emplace(desc.type, desc);
    }

    bool asset_registry::has_asset_type(type_id type) const
    {
        return m_impl->assetTypes.contains(type);
    }

    bool asset_registry::create_directories(const std::filesystem::path& directory)
    {
        return ensure_directories(m_impl->assetsDir / directory);
    }

    void asset_registry::register_file_importer(const file_importer_desc& desc)
    {
        const auto [it, inserted] = m_impl->importers.emplace(desc.type, file_importer_info{});

        if (inserted)
        {
            auto& info = it->second;
            info.create = desc.create;
            info.extensions.reserve(desc.extensions.size());

            for (const std::string_view ext : desc.extensions)
            {
                info.extensions.emplace_back(ext);
            }
        }
    }

    importer asset_registry::create_importer(const std::filesystem::path& sourceFile)
    {
        const auto ext = sourceFile.extension();

        for (auto& [type, assetImporter] : m_impl->importers)
        {
            for (const auto& importerExt : assetImporter.extensions)
            {
                if (importerExt == ext)
                {
                    return importer{
                        importer_config{
                            .registry = this,
                            .sourceFile = sourceFile,
                        },
                        assetImporter.create(),
                    };
                }
            }
        }

        return {};
    }

    uuid asset_registry::generate_uuid()
    {
        return m_impl->uuidGenerator.generate();
    }

    bool asset_registry::save_artifact(const uuid& id, const type_id& type, const void* dataPtr, write_policy policy)
    {
        char uuidBuffer[36];
        id.format_to(uuidBuffer);

        std::string_view filename{uuidBuffer, array_size(uuidBuffer)};
        const auto artifactPath = m_impl->artifactsDir / filename;

        std::error_code ec;

        if (policy == write_policy::no_overwrite && (std::filesystem::exists(artifactPath, ec) || ec))
        {
            return false;
        }

        const auto typeIt = m_impl->assetTypes.find(type);

        if (typeIt == m_impl->assetTypes.end())
        {
            return false;
        }

        const auto saveFunction = typeIt->second.save;
        saveFunction(dataPtr, artifactPath);

        return true;
    }

    bool asset_registry::save_asset(const uuid& id,
                                    const std::filesystem::path& destination,
                                    std::string_view filename,
                                    asset_meta meta,
                                    write_policy policy)
    {
        const auto [assetIt, insertedAsset] = m_impl->assets.emplace(id, std::move(meta));

        if (!insertedAsset)
        {
            return false;
        }

        auto fullPath = m_impl->assetsDir / destination / filename;
        fullPath.concat(assetExt);

        std::error_code ec;

        if (policy == write_policy::no_overwrite && (std::filesystem::exists(fullPath, ec) || ec))
        {
            return false;
        }

        // TODO: Copy source files
        save_asset_meta(assetIt->second, fullPath);

        return true;
    }
}