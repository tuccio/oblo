#pragma once

#include <oblo/core/small_vector.hpp>
#include <oblo/core/types.hpp>

#include <filesystem>
#include <span>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    enum class pipeline_stages : u8
    {
        vertex,
        fragment,
        enum_max
    };

    struct render_pass_stage
    {
        pipeline_stages stage;
        std::filesystem::path shaderSourcePath;
    };

    struct render_pass_initializer
    {
        std::string name;
        small_vector<render_pass_stage, u8(pipeline_stages::enum_max)> stages;
    };

    struct render_pass_targets
    {
        small_vector<VkFormat, 4> colorAttachmentFormats;
        VkFormat depthFormat{VK_FORMAT_UNDEFINED};
        VkFormat stencilFormat{VK_FORMAT_UNDEFINED};
    };

    struct render_pipeline_initializer
    {
        render_pass_targets renderTargets;
    };
}