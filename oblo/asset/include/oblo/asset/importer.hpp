#pragma once

#include <oblo/asset/import_preview.hpp>
#include <oblo/core/type_id.hpp>

#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

namespace oblo::asset
{
    class asset_registry;

    struct import_context;

    struct importer_config
    {
        asset_registry* assetManager;
        std::filesystem::path sourceFile;
    };

    class file_importer
    {
    public:
        virtual ~file_importer() = default;

        virtual void init(const importer_config& config, import_preview& preview) = 0;
        virtual bool import(const import_context& context) = 0;
    };

    class importer
    {
    public:
        importer() = default;

        importer(importer_config config, std::unique_ptr<file_importer> fileImporter) :
            m_config{std::move(config)}, m_importer{std::move(fileImporter)}
        {
        }

        bool init();

        bool execute(const std::filesystem::path& destinationDir);

        bool is_valid() const noexcept
        {
            return m_importer != nullptr;
        }

    private:
        importer_config m_config;
        std::unique_ptr<file_importer> m_importer;
        import_preview m_preview;
        std::vector<import_node_config> m_importNodesConfig;
    };

    using create_file_importer_fn = std::unique_ptr<file_importer> (*)();

    struct file_importer_desc
    {
        type_id type;
        create_file_importer_fn create;
        std::span<const std::string_view> extensions;
    };
}