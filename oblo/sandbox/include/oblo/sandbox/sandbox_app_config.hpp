#pragma once

namespace oblo::vk
{
    struct sandbox_app_config
    {
        const char* appName = "oblo";
        const char* appMainWindowTitle = "oblo";
        bool uiUseDocking = false;
        bool uiUseMultiViewport = false;
        bool vkUseValidationLayers = false;
    };
}