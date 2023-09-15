#include <oblo/engine/module.hpp>

#include <oblo/asset/registry.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/resource/registry.hpp>

namespace oblo::engine
{
    namespace
    {
        module* g_instance{};
    }

    module::module() = default;

    module::~module() = default;

    bool module::startup()
    {
        OBLO_ASSERT(!g_instance);

        m_assetRegistry = std::make_unique<asset::registry>();

        // TODO: Load a project instead
        if (!m_assetRegistry->initialize("./project/assets", "./project/artifacts", "./project/sources"))
        {
            return false;
        }

        m_resourceregistry = std::make_unique<resource::registry>();
        m_resourceregistry->register_provider(&asset::registry::find_artifact_resource, m_assetRegistry.get());

        g_instance = this;
        return true;
    }

    void module::shutdown()
    {
        OBLO_ASSERT(g_instance == this);

        m_resourceregistry.reset();
        m_assetRegistry.reset();
    }
}