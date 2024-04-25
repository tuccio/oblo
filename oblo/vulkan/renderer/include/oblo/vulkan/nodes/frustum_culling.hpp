#pragma once

#include <oblo/vulkan/draw/buffer_binding_table.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>

namespace oblo::vk
{
    class init_context;
    class runtime_builder;
    class runtime_context;

    struct compute_pass;

    struct draw_buffer_data;
    struct frustum_culling_data;

    struct frustum_culling
    {
        h32<compute_pass> cullPass;
        h32<string> drawIndexedDefine;

        data<std::span<frustum_culling_data>> cullInternalData;

        data<buffer_binding_table> inPerViewBindingTable;

        data<std::span<draw_buffer_data>> outDrawBufferData;

        void init(const init_context& context);

        void build(const runtime_builder& builder);

        void execute(const runtime_context& context);
    };
}