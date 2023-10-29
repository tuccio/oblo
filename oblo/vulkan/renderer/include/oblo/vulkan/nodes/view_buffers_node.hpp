#pragma once

#include <oblo/vulkan/data/camera_buffer.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>

namespace oblo::vk
{
    struct view_buffers_node
    {
        data<camera_buffer> inCameraData;
        resource<buffer> outViewBuffer;

        void build(const runtime_builder& builder)
        {
            const auto& cameraBuffer = builder.access(inCameraData);

            builder.create(outViewBuffer,
                {
                    .size = sizeof(camera_buffer),
                    .data = std::as_bytes(std::span{&cameraBuffer, 1}),
                });
        }
    };
}