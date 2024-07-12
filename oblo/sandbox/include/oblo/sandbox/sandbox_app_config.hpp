#pragma once

namespace oblo::vk
{
    struct sandbox_app_config
    {
        const char* appName = "oblo";
        const char* appMainWindowTitle = "oblo";
        const char* imguiIniFile = nullptr;
        i32 uiWindowWidth = -1;
        i32 uiWindowHeight = -1;
        bool uiWindowMaximized = false;
        bool uiUseDocking = false;
        bool uiUseMultiViewport = false;
        bool uiUseKeyboardNavigation = true;
        bool vkUseValidationLayers = false;
    };
}