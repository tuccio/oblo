#include <oblo/asset/importer.hpp>

#include <oblo/asset/asset_registry.hpp>

namespace oblo::asset
{
    bool importer::init()
    {
        if (!m_importer)
        {
            return false;
        }

        m_importer->init(m_config, m_preview);
        m_importNodesConfig.assign(m_preview.nodes.size(), {.enabled = true});
        return true;
    }

    bool importer::execute(const std::filesystem::path& destinationDir)
    {
        const uuid importUuid = m_config.assetManager->begin_import(m_preview, m_importNodesConfig);

        if (importUuid.is_nil())
        {
            return false;
        }

        const import_context context{
            .assetManager = m_config.assetManager,
            .preview = &m_preview,
            .importNodesConfig = m_importNodesConfig,
            .importUuid = importUuid,
        };

        if (!m_importer->import(context))
        {
            // TODO: Cleanup
            return false;
        }

        return m_config.assetManager->finalize_import(importUuid, destinationDir);
    }
}