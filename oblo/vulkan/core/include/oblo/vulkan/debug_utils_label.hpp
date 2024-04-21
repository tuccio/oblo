#pragma once

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    struct debug_utils_label
    {
        PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
        PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;

        void begin(VkCommandBuffer commandBuffer, const char* label) const
        {
            if (!vkCmdBeginDebugUtilsLabelEXT)
            {
                return;
            }

            const VkDebugUtilsLabelEXT labelInfo{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                .pLabelName = label,
            };

            vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &labelInfo);
        }

        void end(VkCommandBuffer commandBuffer) const
        {
            if (!vkCmdEndDebugUtilsLabelEXT)
            {
                return;
            }

            vkCmdEndDebugUtilsLabelEXT(commandBuffer);
        }
    };
}