#pragma once

#include <oblo/vulkan/graph/pins.hpp>

#include <span>

namespace oblo::vk
{
    struct draw_buffer_data;

    class runtime_builder;
    class runtime_context;

    struct bypass_culling
    {
        data<std::span<draw_buffer_data>> outDrawBufferData;

        void build(const runtime_builder& builder);
        void execute(const runtime_context& builder);
    };
}