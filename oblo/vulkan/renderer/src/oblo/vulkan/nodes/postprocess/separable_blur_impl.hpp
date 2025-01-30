#include <oblo/vulkan/nodes/postprocess/separable_blur.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    template <separable_blur_config Config, separable_blur_pass Pass>
    void separable_blur<Config, Pass>::init(const frame_graph_init_context& ctx)
    {
        const auto& shaderName = Config::get_shader_name();

        string_builder shaderPath;
        shaderPath.append("./vulkan/shaders/postprocess/").append(shaderName).append(".comp");

        string_builder passName;
        passName.format("Blur {} {}",
            shaderName,
            string_view(Pass == separable_blur_pass::horizontal ? "Horizontal" : "Vertical"));

        blurPass = ctx.register_compute_pass({
            .name = passName.as<string>(),
            .shaderSourcePath = shaderPath.as<string>(),
        });
    }

    template <separable_blur_config Config, separable_blur_pass Pass>
    void separable_blur<Config, Pass>::build(const frame_graph_build_context& ctx)
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
        case VK_FORMAT_R8_UNORM:
            imageFormat = "BLUR_IMAGE_FORMAT r8"_hsv;
            imageChannels.format("BLUR_IMAGE_CHANNELS 1");
            break;

        case VK_FORMAT_R8G8_UNORM:
            imageFormat = "BLUR_IMAGE_FORMAT rg8"_hsv;
            imageChannels.format("BLUR_IMAGE_CHANNELS 2");
            break;

        default:
            OBLO_ASSERT(false);
            break;
        }

        const hashed_string_view defines[] = {
            groupSizeDef.as<hashed_string_view>(),
            kernelData.as<hashed_string_view>(),
            kernelSize.as<hashed_string_view>(),
            imageChannels.as<hashed_string_view>(),
            imageFormat,
            Pass == separable_blur_pass::horizontal ? "BLUR_HORIZONTAL"_hsv : "BLUR_VERTICAL"_hsv,
        };

        blurPassInstance = ctx.compute_pass(blurPass, {.defines = defines});

        if (!outputInPlace || Pass == separable_blur_pass::horizontal)
        {
            ctx.acquire(inSource, texture_usage::storage_read);

            const auto imageInitializer = ctx.get_current_initializer(inSource);
            imageInitializer.assert_value();

            ctx.create(outBlurred,
                {
                    .width = imageInitializer->extent.width,
                    .height = imageInitializer->extent.height,
                    .format = imageInitializer->format,
                    .usage = imageInitializer->usage,
                },
                texture_usage::storage_write);
        }
        else
        {
            ctx.acquire(inSource, texture_usage::storage_read);
            ctx.acquire(outBlurred, texture_usage::storage_write);
        }
    }

    template <separable_blur_config Config, separable_blur_pass Pass>
    void separable_blur<Config, Pass>::execute(const frame_graph_execute_context& ctx)
    {
        if (const auto pass = ctx.begin_pass(blurPassInstance))
        {
            const auto& sourceTexture = ctx.access(inSource);
            const vec2u resolution{sourceTexture.initializer.extent.width, sourceTexture.initializer.extent.height};

            binding_table2 bindingTable;

            bindingTable.bind_textures({
                {"t_InSource", inSource},
                {"t_OutBlurred", outBlurred},
            });

            ctx.bind_descriptor_sets(bindingTable);

            if constexpr (Pass == separable_blur_pass::horizontal)
            {
                ctx.dispatch_compute(round_up_div(resolution.x, groupSize), resolution.y, 1);
            }
            else
            {
                ctx.dispatch_compute(resolution.x, round_up_div(resolution.y, groupSize), 1);
            }

            ctx.end_pass();
        }
    }
}