#include <oblo/renderdoc/renderdoc_module.hpp>

#include <oblo/core/platform/shell.hpp>

namespace oblo
{
    bool renderdoc_module::startup(const module_initializer&)
    {
        m_api = nullptr;

        if (auto path = platform::search_program_files("./RenderDoc/renderdoc.dll"))
        {
            m_library.open(*path);

            const auto RENDERDOC_GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(m_library.symbol("RENDERDOC_GetAPI"));
            const auto ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, reinterpret_cast<void**>(&m_api));

            if (ret != 1)
            {
                m_library.close();
                m_api = nullptr;
            }
        }

        return m_api != nullptr;
    }

    void renderdoc_module::shutdown()
    {
        m_api = nullptr;
        m_library.close();
    }
}