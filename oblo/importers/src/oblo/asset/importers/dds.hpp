#pragma once

#include <oblo/asset/import/file_importer.hpp>
#include <oblo/asset/import/import_artifact.hpp>

namespace oblo::importers
{
    class dds final : public file_importer
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