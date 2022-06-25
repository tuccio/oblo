#pragma once

#include <span>
#include <vulkan/vulkan.h>

namespace oblo::vk
{
    inline void destroy_device_object(VkDevice device, VkSemaphore semaphore)
    {
        return vkDestroySemaphore(device, semaphore, nullptr);
    }

    inline void destroy_device_object(VkDevice device, VkFence fence)
    {
        return vkDestroyFence(device, fence, nullptr);
    }

    inline void destroy_device_object(VkDevice device, VkPipeline pipeline)
    {
        return vkDestroyPipeline(device, pipeline, nullptr);
    }

    inline void destroy_device_object(VkDevice device, VkPipelineLayout pipelineLayout)
    {
        return vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }

    inline void destroy_device_object(VkDevice device, VkRenderPass renderPass)
    {
        return vkDestroyRenderPass(device, renderPass, nullptr);
    }

    inline void destroy_device_object(VkDevice device, VkShaderModule shaderModule)
    {
        return vkDestroyShaderModule(device, shaderModule, nullptr);
    }

    inline void destroy_device_object(VkDevice device, VkFramebuffer frameBuffer)
    {
        return vkDestroyFramebuffer(device, frameBuffer, nullptr);
    }

    inline void destroy_device_object(VkDevice device, VkDescriptorPool descriptorPool)
    {
        return vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }

    inline void destroy_device_object(VkDevice device, VkDescriptorSetLayout descriptorSetLayout)
    {
        return vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    }

    inline void destroy_device_object(VkDevice device, VkQueryPool queryPool)
    {
        return vkDestroyQueryPool(device, queryPool, nullptr);
    }

    template <typename T, std::size_t N>
    void destroy_device_object(VkDevice device, std::span<T, N> objects)
    {
        for (auto object : objects)
        {
            destroy_device_object(device, object);
        }
    }

    template <typename T>
    void reset_device_object(VkDevice device, T& vulkanObject)
    {
        if (vulkanObject)
        {
            destroy_device_object(device, vulkanObject);
            vulkanObject = VK_NULL_HANDLE;
        }
    }

    template <typename T, std::size_t N>
    void reset_device_object(VkDevice device, const std::span<T, N>& objects)
    {
        for (auto& object : objects)
        {
            reset_device_object(device, object);
        }
    }

    template <typename... T>
    void reset_device_objects(VkDevice device, T&&... vulkanObjects)
    {
        (reset_device_object(device, std::forward<T>(vulkanObjects)), ...);
    }
}