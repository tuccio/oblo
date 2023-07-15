#include <oblo/vulkan/error.hpp>

#include <cstdio>
#include <exception>

namespace oblo::vk
{
    void panic(const char* file, int line, const char* call, VkResult result)
    {
        std::fprintf(stderr, "[Vulkan Error] %s:%d %s (%d)", file, line, call, result);
        std::terminate();
    }
}