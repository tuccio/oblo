#include <oblo/vulkan/error.hpp>

#include <cstdio>
#include <exception>

namespace oblo
{
    void vulkan_fatal_error(const char* file, int line, const char* call, VkResult result)
    {
        std::fprintf(stderr, "[Vulkan Error] %s:%d %s (%d)", file, line, call, result);
        std::terminate();
    }
}