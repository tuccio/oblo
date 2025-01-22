#pragma once

#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/importer.hpp>

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
        bool init(const import_config& config, import_preview& preview);
        bool import(import_context context);
        file_import_results get_results();

    private:
        string m_source;
        import_artifact m_result{};
    };
}