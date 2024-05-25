#pragma once

#include <oblo/core/finally.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/renderdoc/renderdoc_module.hpp>

namespace oblo
{
    inline auto renderdoc_scoped_capture()
    {
        auto* const renderdoc = module_manager::get().load<renderdoc_module>();

        renderdoc_module::api* api{};

        if (renderdoc)
        {
            api = renderdoc->get_api();
            api->StartFrameCapture(nullptr, nullptr);
        }

        return finally(
            [api]
            {
                if (api)
                {
                    api->EndFrameCapture(nullptr, nullptr);
                }
            });
    }
}