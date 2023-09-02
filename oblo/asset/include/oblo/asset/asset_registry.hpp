#pragma once

#include <oblo/core/type_id.hpp>

#include <filesystem>
#include <memory>
#include <span>

namespace oblo
{
    struct uuid;
}

namespace oblo::asset
{
    class importer;
    struct asset_meta;
    struct asset_type_desc;
    struct file_importer_desc;
    struct import_preview;
    struct import_node_config;

    class asset_registry
    {
    public:
        asset_registry();
        asset_registry(const asset_registry&) = delete;
        asset_registry(asset_registry&&) noexcept = delete;
        asset_registry& operator=(const asset_registry&) = delete;
        asset_registry& operator=(asset_registry&&) noexcept = delete;
        ~asset_registry();

        [[nodiscard]] bool initialize(const std::filesystem::path& assetsDir,
                                      const std::filesystem::path& artifactsDir);

        void shutdown();

        void register_type(const asset_type_desc& desc);
        bool has_asset_type(type_id type) const;

        void register_file_importer(const file_importer_desc& desc);

        bool create_directories(const std::filesystem::path& directory);

        [[nodiscard]] importer create_importer(const std::filesystem::path& sourceFile);

        const asset_meta& find_asset_by_path(const std::filesystem::path& path) const;

    private:
        struct impl;
        friend class importer;

        enum class write_policy
        {
            no_overwrite,
            overwrite,
        };

    private:
        uuid generate_uuid();

        bool save_artifact(const uuid& id,
                           const type_id& type,
                           const void* dataPtr,
                           write_policy policy = write_policy::no_overwrite);

        bool save_asset(const uuid& id,
                        const std::filesystem::path& destination,
                        std::string_view fileNameBuffer,
                        asset_meta meta,
                        write_policy policy = write_policy::no_overwrite);

    private:
        std::unique_ptr<impl> m_impl;
    };
}