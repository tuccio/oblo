#include <oblo/renderer/nodes/postprocess/separable_blur.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/draw/compute_pass_initializer.hpp>
#include <oblo/renderer/graph/node_common.hpp>

namespace oblo
{
    template <separable_blur_config Config>
    void separable_blur<Config>::init(const frame_graph_init_context& ctx)
    {
        const auto& shaderName = Config::get_shader_name();

        string_builder shaderPath;
        shaderPath.append("./vulkan/shaders/postprocess/").append(shaderName).append(".comp");

        string_builder passName;
        passName.format("Blur {}", shaderName);

        blurPass = ctx.register_compute_pass({
            .name = passName.as<string>(),
            .shaderSourcePath = shaderPath.as<string>(),
        });
    }

    template <separable_blur_config Config>
    void separable_blur<Config>::build(const frame_graph_build_context& ctx)
    {
        const auto& sourceInitializer = ctx.get_current_initializer(inSource);

        auto& cfg = ctx.access(inConfig);

        auto& allocator = ctx.get_frame_allocator();
        dynamic_array<f32> kernelArray{&allocator};
        make_separable_blur_kernel(cfg, kernelArray);

        kernel = kernelArray;

        string_builder groupSizeDef;
        groupSizeDef.format("BLUR_GROUP_SIZE {}", groupSize);

        string_builder kernelData;
        kernelData.append("BLUR_KERNEL_DATA ").join(kernel.begin(), kernel.end(), ", ", "{:.10f}f");

        string_builder kernelSize;
        kernelSize.format("BLUR_KERNEL_DATA_SIZE {}", kernel.size());

        string_builder imageChannels;
        hashed_string_view imageFormat;

        switch (sourceInitializer->format)
        {
        case gpu::image_format::r8_unorm:
            imageFormat = "BLUR_IMAGE_FORMAT r8"_hsv;
            imageChannels.format("BLUR_IMAGE_CHANNELS 1");
            break;

        case gpu::image_format::r8g8_unorm:
            imageFormat = "BLUR_IMAGE_FORMAT rg8"_hsv;
            imageChannels.format("BLUR_IMAGE_CHANNELS 2");
            break;

        default:
            OBLO_ASSERT(false);
            break;
        }

        hashed_string_view defines[] = {
            {}, // This gets replaced by BLUR_HORIZONTAL/BLUR_VERTICAL
            groupSizeDef.as<hashed_string_view>(),
            kernelData.as<hashed_string_view>(),
            kernelSize.as<hashed_string_view>(),
            imageChannels.as<hashed_string_view>(),
            imageFormat,
        };

        {
            defines[0] = "BLUR_HORIZONTAL"_hsv;

            passInstanceH = ctx.compute_pass(blurPass, {.defines = defines});
            ctx.acquire(inSource, texture_usage::storage_read);

            ctx.create(inOutIntermediate,
                {
                    .width = sourceInitializer->width,
                    .height = sourceInitializer->height,
                    .format = sourceInitializer->format,
                },
                texture_usage::storage_write);
        }

        {
            defines[0] = "BLUR_VERTICAL"_hsv;

            passInstanceV = ctx.compute_pass(blurPass, {.defines = defines});

            ctx.acquire(inOutIntermediate, texture_usage::storage_read);

            if (!ctx.has_source(outBlurred))
            {
                ctx.create(outBlurred,
                    {
                        .width = sourceInitializer->width,
                        .height = sourceInitializer->height,
                        .format = sourceInitializer->format,
                    },
                    texture_usage::storage_write);
            }
            else
            {
                ctx.acquire(outBlurred, texture_usage::storage_write);
            }
        }
    }

    template <separable_blur_config Config>
    void separable_blur<Config>::execute(const frame_graph_execute_context& ctx)
    {
        const vec2u resolution = ctx.get_resolution(inSource);

        binding_table bindingTable;

        if (const auto pass = ctx.begin_pass(passInstanceH))
        {
            bindingTable.clear();

            bindingTable.bind_textures({
                {"t_InSource", inSource},
                {"t_OutBlurred", inOutIntermediate},
            });

            ctx.bind_descriptor_sets(bindingTable);
            ctx.dispatch_compute(round_up_div(resolution.x, groupSize), resolution.y, 1);

            ctx.end_pass();
        }

        if (const auto pass = ctx.begin_pass(passInstanceV))
        {
            bindingTable.clear();

            bindingTable.bind_textures({
                {"t_InSource", inOutIntermediate},
                {"t_OutBlurred", outBlurred},
            });

            ctx.bind_descriptor_sets(bindingTable);
            ctx.dispatch_compute(resolution.x, round_up_div(resolution.y, groupSize), 1);

            ctx.end_pass();
        }
    }
}