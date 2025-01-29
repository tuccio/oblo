#include <oblo/asset/import/copy_importer.hpp>

#include <oblo/asset/import/import_config.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    copy_importer::copy_importer(uuid artifactType, string artifactName) :
        m_artifactType{artifactType}, m_artifactName{artifactName}
    {
    }

    bool copy_importer::init(const import_config& config, import_preview& preview)
    {
        m_source = config.sourceFile;

        auto& n = preview.nodes.emplace_back();
        n.artifactType = m_artifactType;
        n.name = m_artifactName;

        return true;
    }

    bool copy_importer::import(import_context ctx)
    {
        const std::span configs = ctx.get_import_node_configs();

        const auto& nodeConfig = configs[0];

        if (!nodeConfig.enabled)
        {
            return true;
        }

        string_builder destination;
        ctx.get_output_path(nodeConfig.id, destination);

        m_artifact.id = nodeConfig.id;
        m_artifact.name = m_artifactName;
        m_artifact.path = destination.as<string>();
        m_artifact.type = m_artifactType;

        return filesystem::copy_file(m_source, destination).value_or(false);
    }

    file_import_results copy_importer::get_results()
    {
        file_import_results r;
        r.artifacts = {&m_artifact, 1};
        r.sourceFiles = {&m_source, 1};
        r.mainArtifactHint = m_artifact.id;
        return r;
    }
}