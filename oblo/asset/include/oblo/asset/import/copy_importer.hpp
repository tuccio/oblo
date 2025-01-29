#pragma once

#include <oblo/asset/import/file_importer.hpp>
#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/core/string/string.hpp>

namespace oblo
{
    class copy_importer : public file_importer
    {
    public:
        explicit copy_importer(uuid artifactType, string artifactName);

        bool init(const import_config& config, import_preview& preview) override;

        bool import(import_context ctx) override;

        file_import_results get_results() override;

    private:
        uuid m_artifactType;
        string m_artifactName;
        import_artifact m_artifact;
        string m_source;
    };
}