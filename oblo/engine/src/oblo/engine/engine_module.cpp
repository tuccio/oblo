#include <oblo/engine/engine_module.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/resource/resource_registry.hpp>

namespace oblo::engine
{
    namespace
    {
        engine_module* g_instance{};
    }

    engine_module::engine_module() = default;

    engine_module::~engine_module() = default;

    bool engine_module::startup()
    {
        OBLO_ASSERT(!g_instance);

        m_assetRegistry = std::make_unique<asset::asset_registry>();

        // TODO: Load a project instead
        if (!m_assetRegistry->initialize("./project/assets", "./project/artifacts", "./project/sources"))
        {
            return false;
        }

        m_resourceRegistry = std::make_unique<resource_registry>();
        m_resourceRegistry->register_provider(&asset::asset_registry::find_artifact_resource, m_assetRegistry.get());

        g_instance = this;
        return true;
    }

    void engine_module::shutdown()
    {
        OBLO_ASSERT(g_instance == this);

        m_resourceRegistry.reset();
        m_assetRegistry.reset();
    }
}