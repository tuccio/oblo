#include <oblo/vulkan/nodes/separable_blur.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    template <separable_blur_config Config, u8 PassIndex>
    void separable_blur<Config, PassIndex>::init(const frame_graph_init_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        const auto& shaderName = Config::get_shader_name();

        string_builder shaderPath;
        shaderPath.append("./vulkan/shaders/postprocess/").append(shaderName).append(".comp");

        string_builder passName;
        passName.format("Blur {} {}", shaderPath, std::string_view(PassIndex == 0 ? "Horizontal" : "Vertical"));

        blurPass = pm.register_compute_pass({
            .name = std::string{passName.view()},
            .shaderSourcePath = std::filesystem::path{shaderPath.view()},
        });
    }

    template <separable_blur_config Config, u8 PassIndex>
    void separable_blur<Config, PassIndex>::build(const frame_graph_build_context& ctx)
    {
        if constexpr (PassIndex == 0)
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

        auto& cfg = ctx.access(inConfig);

        auto& allocator = ctx.get_frame_allocator();
        dynamic_array<f32> kernelArray{&allocator};
        make_separable_blur_kernel(cfg, kernelArray);

        kernel = kernelArray;
    }

    template <separable_blur_config Config, u8 PassIndex>
    void separable_blur<Config, PassIndex>::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        const auto commandBuffer = ctx.get_command_buffer();

        const auto& sourceTexture = ctx.access(inSource);

        string_builder kernelData;
        kernelData.append("BLUR_KERNEL_DATA ").join(kernel.begin(), kernel.end(), ", ", "{:.10f}f");

        string_builder kernelSize;
        kernelSize.format("BLUR_KERNEL_SIZE {}", kernel.size());

        string_builder imageFormat;
        imageFormat.append("BLUR_IMAGE_FORMAT ");

        switch (sourceTexture.initializer.format)
        {
        case VK_FORMAT_R8_UNORM:
            imageFormat.append("r8");
            break;

        default:
            OBLO_ASSERT(false);
            imageFormat.append("rgba8");
            break;
        }

        const h32<string> defines[] = {
            ctx.get_string_interner().get_or_add(kernelData.view()),
            ctx.get_string_interner().get_or_add(kernelSize.view()),
            ctx.get_string_interner().get_or_add(imageFormat.view()),
            ctx.get_string_interner().get_or_add(PassIndex == 0 ? "BLUR_HORIZONTAL" : "BLUR_VERTICAL"),
        };

        const auto lightingPipeline = pm.get_or_create_pipeline(blurPass, {.defines = defines});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, lightingPipeline))
        {

            const vec2u resolution{sourceTexture.initializer.extent.width, sourceTexture.initializer.extent.height};

            binding_table bindingTable;

            ctx.bind_textures(bindingTable,
                {
                    {"t_InSource", inSource},
                    {"t_OutBlurred", outBlurred},
                });

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&resolution, 1}));

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), round_up_multiple(resolution.x, 64u), resolution.y, 1);

            pm.end_compute_pass(*pass);
        }
    }
}