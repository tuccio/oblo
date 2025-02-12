#pragma once

#include <vulkan/vulkan.h>

namespace oblo::vk::debug_utils
{
    struct label
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

    struct object
    {
        PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;

        void set_object_name(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* name) const
        {
            if (!vkSetDebugUtilsObjectNameEXT)
            {
                return;
            }

            const VkDebugUtilsObjectNameInfoEXT nameInfo{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = objectType,
                .objectHandle = objectHandle,
                .pObjectName = name,
            };

            vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
        }

        void set_object_name(VkDevice device, VkImage image, const char* name) const
        {
            set_object_name(device, VK_OBJECT_TYPE_IMAGE, uint64_t(image), name);
        }

        void set_object_name(VkDevice device, VkImageView imageView, const char* name) const
        {
            set_object_name(device, VK_OBJECT_TYPE_IMAGE_VIEW, uint64_t(imageView), name);
        }

        void set_object_name(VkDevice device, VkBuffer buffer, const char* name) const
        {
            set_object_name(device, VK_OBJECT_TYPE_BUFFER, uint64_t(buffer), name);
        }

        void set_object_name(VkDevice device, VkSemaphore semaphore, const char* name) const
        {
            set_object_name(device, VK_OBJECT_TYPE_SEMAPHORE, uint64_t(semaphore), name);
        }

        void set_object_name(VkDevice device, VkFence fence, const char* name) const
        {
            set_object_name(device, VK_OBJECT_TYPE_FENCE, uint64_t(fence), name);
        }

        void set_object_name(VkDevice device, VkDescriptorSet descriptorSet, const char* name) const
        {
            set_object_name(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, uint64_t(descriptorSet), name);
        }

        void set_object_name(VkDevice device, VkPipeline pieline, const char* name) const
        {
            set_object_name(device, VK_OBJECT_TYPE_PIPELINE, uint64_t(pieline), name);
        }

        void set_object_name(VkDevice device, VkPipelineLayout pipelineLayout, const char* name) const
        {
            set_object_name(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, uint64_t(pipelineLayout), name);
        }
    };
}