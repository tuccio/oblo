#pragma once

#include <oblo/asset/import_artifact.hpp>
#include <oblo/asset/importer.hpp>

#include <filesystem>
#include <span>

namespace oblo
{
    class texture;
}

namespace oblo::importers
{
    class stb_image final : public file_importer
    {
    public:
        enum class error : u8;

        static bool load_from_file(texture& out, const std::filesystem::path& path);
        static bool load_from_memory(texture& out, const std::span<const std::byte> data);

    public:
        bool init(const importer_config& config, import_preview& preview);
        bool import(const import_context& context);
        file_import_results get_results();

    private:
        std::filesystem::path m_source;
        import_artifact m_result{};
    };

    enum class error : u8
    {
        unrecognized_format,
    };
}