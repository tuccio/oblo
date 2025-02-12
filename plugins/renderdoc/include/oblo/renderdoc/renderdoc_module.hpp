#pragma once

#include <oblo/core/platform/shared_library.hpp>
#include <oblo/modules/module_interface.hpp>

#include <renderdoc_app.h>

namespace oblo
{
    class renderdoc_module final : public module_interface
    {
    public:
        using api = RENDERDOC_API_1_1_2;

    public:
        bool startup(const module_initializer& initializer) override;
        void shutdown() override;
        bool finalize() override { return true; }

        api* get_api() const
        {
            return m_api;
        }

    private:
        platform::shared_library m_library;
        api* m_api{};
    };
}