#pragma once

namespace oblo::vk
{
    struct sandbox_app_config
    {
        const char* appName = "oblo";
        const char* appMainWindowTitle = "oblo";
        const char* imguiIniFile = nullptr;
        bool uiUseDocking = false;
        bool uiUseMultiViewport = false;
        bool vkUseValidationLayers = false;
    };
}