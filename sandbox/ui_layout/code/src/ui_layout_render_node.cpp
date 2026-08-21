#include <ui_layout_render_node.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/draw/compute_pass_initializer.hpp>
#include <oblo/renderer/draw/render_pass_initializer.hpp>
#include <oblo/renderer/graph/frame_graph_context.hpp>
#include <oblo/renderer/graph/node_common.hpp>

namespace oblo
{
    void ui_layout_render_node::init(const frame_graph_init_context& ctx)
    {
        rasterizePass = ctx.register_render_pass({
            .name = "UI Layout Rasterize",
            .stages = make_span_initializer<render_pass_stage>({
                {
                    .stage = gpu::shader_stage::vertex,
                    .shaderSourcePath = "./ui_layout/shaders/ui_layout_rasterize.vert",
                },
                {
                    .stage = gpu::shader_stage::fragment,
                    .shaderSourcePath = "./ui_layout/shaders/ui_layout_rasterize.frag",
                },
            }),
        });
    }

    void ui_layout_render_node::build(const frame_graph_build_context& ctx)
    {
        const std::span elements = ctx.access(inElements);

        if (elements.empty())
        {
            rasterizePassInstance = {};
            return;
        }

        const auto resolution = ctx.access(inResolution);
        const u32 width = max(resolution.x, 1u);
        const u32 height = max(resolution.y, 1u);

        const texture_init_desc rtInitializer{
            .width = width,
            .height = height,
            .format = gpu::image_format::r8g8b8a8_srgb,
        };

        rasterizePassInstance =
            ctx
                .render_pass(rasterizePass,
                    {
                        .renderTargets =
                            {
                                .colorAttachmentFormats = make_span_initializer({rtInitializer.format}),
                                .blendStates = make_span_initializer({
                                    gpu::color_blend_attachment_state{
                                        .enable = true,
                                        .srcColorBlendFactor = gpu::blend_factor::src_alpha,
                                        .dstColorBlendFactor = gpu::blend_factor::one_minus_src_alpha,
                                        .colorBlendOp = gpu::blend_op::add,
                                        .srcAlphaBlendFactor = gpu::blend_factor::one,
                                        .dstAlphaBlendFactor = gpu::blend_factor::one_minus_src_alpha,
                                        .alphaBlendOp = gpu::blend_op::add,
                                        .colorWriteMask = gpu::color_component::r | gpu::color_component::g |
                                            gpu::color_component::b | gpu::color_component::a,
                                    },
                                }),
                            },
                        .depthStencilState =
                            {
                                .depthTestEnable = false,
                                .depthWriteEnable = false,
                            },
                        .rasterizationState =
                            {
                                .polygonMode = gpu::polygon_mode::fill,
                                .cullMode = {},
                                .lineWidth = 1.f,
                            },
                        .primitiveTopology = gpu::primitive_topology::triangle_fan,
                    });

        ctx.create(elementsBuffer,
            buffer_resource_initializer{
                .size = elements.size_bytes(),
                .data = as_bytes(elements),
            },
            buffer_usage::storage_read);

        ctx.create(outImage,
            texture_resource_initializer{
                .width = rtInitializer.width,
                .height = rtInitializer.height,
                .format = rtInitializer.format,
            },
            texture_usage::render_target_write);
    }

    void ui_layout_render_node::execute(const frame_graph_execute_context& ctx)
    {
        if (!rasterizePassInstance)
        {
            return;
        }

        const auto resolution = ctx.access(inResolution);
        const auto elements = ctx.access(inElements);

        binding_table bindingTable;

        bindingTable.bind_buffers({
            {"b_ElementsData"_hsv, elementsBuffer},
        });

        if (ctx.begin_pass(rasterizePassInstance,
                {
                    .renderResolution = resolution,
                    .colorAttachments = make_span_initializer<gpu::graphics_attachment>({{
                        .image = ctx.access(outImage),
                        .loadOp = gpu::attachment_load_op::clear,
                        .storeOp = gpu::attachment_store_op::store,
                    }}),
                }))
        {
            ctx.bind_descriptor_sets(bindingTable);

            struct push_constants
            {
                vec2u resolution;
            };

            const push_constants pushConstants{
                .resolution = resolution,
            };

            ctx.push_constants(gpu::shader_stage::vertex, 0, std::as_bytes(std::span{&pushConstants, 1}));

            ctx.set_scissor(0, 0, resolution.x, resolution.y);
            ctx.set_viewport(resolution.x, resolution.y);

            ctx.draw(4u, u32(elements.size()), 0u, 0u);

            ctx.end_pass();
        }
    }
}