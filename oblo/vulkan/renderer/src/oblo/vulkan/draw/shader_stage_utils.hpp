#pragma once

#include <oblo/vulkan/compiler/shader_stage.hpp>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    inline VkShaderStageFlagBits to_vk_shader_stage(shader_stage stage)
    {
        switch (stage)
        {
        case shader_stage::vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case shader_stage::geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case shader_stage::tessellation_control:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case shader_stage::tessellation_evaluation:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case shader_stage::fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case shader_stage::compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case shader_stage::raygen:
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case shader_stage::intersection:
            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case shader_stage::closest_hit:
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case shader_stage::any_hit:
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case shader_stage::miss:
            return VK_SHADER_STAGE_MISS_BIT_KHR;
        case shader_stage::callable:
            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        case shader_stage::task:
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        case shader_stage::mesh:
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        default:
            unreachable();
        }
    }

    inline shader_stage from_vk_shader_stage(VkShaderStageFlagBits vkStage)
    {
        switch (vkStage)
        {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return shader_stage::vertex;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return shader_stage::geometry;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return shader_stage::tessellation_control;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return shader_stage::tessellation_evaluation;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return shader_stage::fragment;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return shader_stage::compute;
        case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            return shader_stage::raygen;
        case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
            return shader_stage::intersection;
        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
            return shader_stage::closest_hit;
        case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
            return shader_stage::any_hit;
        case VK_SHADER_STAGE_MISS_BIT_KHR:
            return shader_stage::miss;
        case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
            return shader_stage::callable;
        case VK_SHADER_STAGE_TASK_BIT_EXT:
            return shader_stage::task;
        case VK_SHADER_STAGE_MESH_BIT_EXT:
            return shader_stage::mesh;
        default:
            unreachable();
        }
    }
}