#include <oblo/vulkan/error.hpp>

#include <oblo/log/log.hpp>

#include <cstdio>
#include <exception>

namespace oblo::vk
{
    namespace
    {
        void panic_default_handler(const char* file, int line, const char* call, VkResult result, void*)
        {
            log::error("[Vulkan Error] {}:{} {} ({:#x})", file, line, call, i32(result));
        }

        void* g_userdata{};
        panic_handler g_handler{panic_default_handler};
    }

    void panic(const char* file, int line, const char* call, VkResult result)
    {
        if (g_handler)
        {
            g_handler(file, line, call, result, g_userdata);
        }

        std::terminate();
    }

    void set_panic_handler(panic_handler handler, void* userdata)
    {
        g_handler = handler ? handler : panic_default_handler;
        g_userdata = userdata;
    }
}