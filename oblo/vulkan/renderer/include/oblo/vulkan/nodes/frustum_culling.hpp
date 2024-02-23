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

    struct frustum_culling_data
    {
        resource<buffer> drawCallBuffer;
        resource<buffer> preCullingIndicesBuffer;
        batch_draw_data sourceData;
    };

    struct frustum_culling
    {
        h32<compute_pass> cullPass;

        data<buffer_binding_table> inPerViewBindingTable;

        data<std::span<frustum_culling_data>> outCullData;

        void init(const init_context& context);

        void build(const runtime_builder& builder);

        void execute(const runtime_context& context);
    };
}