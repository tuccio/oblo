#pragma once

namespace oblo::platform
{
    bool init();
    void shutdown();

    void debug_output(const char* str);
}