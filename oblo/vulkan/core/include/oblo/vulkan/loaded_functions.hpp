#pragma once

#include <vulkan/vulkan_core.h>

namespace oblo::vk
{
    struct loaded_functions
    {
        PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT;
        PFN_vkCmdDrawMeshTasksIndirectEXT vkCmdDrawMeshTasksIndirectEXT;
        PFN_vkCmdDrawMeshTasksIndirectCountEXT vkCmdDrawMeshTasksIndirectCountEXT;
    };
}