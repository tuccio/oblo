#pragma once

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    [[noreturn]] void panic(const char* file, int line, const char* call, VkResult result);

    using panic_handler = void (*)(const char* file, int line, const char* call, VkResult result, void* userdata);

    void set_panic_handler(panic_handler handler, void* userdata);
}

#define OBLO_VK_PANIC(Call)                                                                                            \
    if (const VkResult _result = Call; _result != VK_SUCCESS)                                                          \
    {                                                                                                                  \
        ::oblo::vk::panic(__FILE__, __LINE__, #Call, _result);                                                         \
    }

#define OBLO_VK_PANIC_MSG(Msg, Result)                                                                                 \
    if (const VkResult _result = Result; _result != VK_SUCCESS)                                                        \
    {                                                                                                                  \
        ::oblo::vk::panic(__FILE__, __LINE__, Msg, _result);                                                           \
    }

#define OBLO_VK_PANIC_EXCEPT(Call, Except)                                                                             \
    if (const VkResult _result = Call; _result != VK_SUCCESS && _result != Except)                                     \
    {                                                                                                                  \
        ::oblo::vk::panic(__FILE__, __LINE__, #Call, _result);                                                         \
    }