#include <oblo/vulkan/render_pass_manager.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/vulkan/render_pass_initializer.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr u8 MaxPipelineStages = u8(pipeline_stages::enum_max);

        constexpr VkShaderStageFlagBits to_vulkan_stage_bits(pipeline_stages stage)
        {
            constexpr VkShaderStageFlagBits vkStageBits[] = {
                VK_SHADER_STAGE_VERTEX_BIT,
                VK_SHADER_STAGE_FRAGMENT_BIT,
            };
            return vkStageBits[u8(stage)];
        }

        struct render_pass_variant
        {
            u64 hash;
            handle<render_pipeline> pipeline;
        };
    }

    struct render_pass
    {
        std::string name;
        std::filesystem::path shaderSourcePath[MaxPipelineStages];
        pipeline_stages stages[MaxPipelineStages];
        u8 stagesCount{0};

        std::vector<render_pass_variant> variants;
    };

    struct render_pipeline
    {
        VkShaderModule shaderModules[MaxPipelineStages];
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        // TODO: Active stages (e.g. tessellation on/off)
        // TODO: Active options
    };

    namespace
    {
        void destroy_pipeline(VkDevice device, const render_pipeline& variant)
        {
            if (const auto pipeline = variant.pipeline)
            {
                vkDestroyPipeline(device, pipeline, nullptr);
            }

            if (const auto pipelineLayout = variant.pipelineLayout)
            {
                vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            }

            for (auto shaderModule : variant.shaderModules)
            {
                if (shaderModule)
                {
                    vkDestroyShaderModule(device, shaderModule, nullptr);
                }
            }
        }
    }

    render_pass_manager::render_pass_manager() = default;
    render_pass_manager::~render_pass_manager() = default;

    void render_pass_manager::init(VkDevice device)
    {
        m_device = device;
    }

    void render_pass_manager::shutdown()
    {
        for (const auto& renderPipeline : m_renderPipelines.values())
        {
            destroy_pipeline(m_device, renderPipeline);
        }
    }

    handle<render_pass> render_pass_manager::register_render_pass(const render_pass_initializer& desc)
    {
        const handle<render_pass> handle{++m_lastRenderPassId};

        const auto [it, ok] = m_renderPasses.emplace(handle);
        OBLO_ASSERT(ok);

        auto& renderPass = *it;

        renderPass.name = desc.name;

        renderPass.stagesCount = 0;

        for (const auto& stage : desc.stages)
        {
            renderPass.shaderSourcePath[renderPass.stagesCount] = stage.shaderSourcePath;
            renderPass.stages[renderPass.stagesCount] = stage.stage;
            ++renderPass.stagesCount;
        }

        return handle;
    }

    handle<render_pipeline> render_pass_manager::get_or_create_pipeline(handle<render_pass> renderPassHandle,
                                                                        const render_pipeline_initializer& desc)
    {
        auto* const renderPass = m_renderPasses.try_find(renderPassHandle);

        if (!renderPass)
        {
            return {};
        }

        // TODO: We need to consider the initializer, for now we'd only end up with one variant
        u64 expectedHash{renderPassHandle.value};

        if (const auto variantIt = std::find_if(renderPass->variants.begin(),
                                                renderPass->variants.end(),
                                                [expectedHash](const render_pass_variant& variant)
                                                { return variant.hash == expectedHash; });
            variantIt != renderPass->variants.end())
        {
            return variantIt->pipeline;
        }

        const handle<render_pipeline> pipelineHandle{m_lastRenderPipelineId + 1};

        const auto [pipelineIt, ok] = m_renderPipelines.emplace(pipelineHandle);
        OBLO_ASSERT(ok);
        auto& newPipeline = *pipelineIt;

        const auto failure = [this, &newPipeline, renderPass]
        {
            destroy_pipeline(m_device, newPipeline);
            renderPass->variants.pop_back();
            return handle<render_pipeline>{};
        };

        VkPipelineShaderStageCreateInfo stageCreateInfo[MaxPipelineStages]{};
        u32 actualStagesCount{0};

        for (u8 stageIndex = 0; stageIndex < renderPass->stagesCount; ++stageIndex)
        {
            const auto pipelineStage = renderPass->stages[stageIndex];
            const auto vkStage = to_vulkan_stage_bits(pipelineStage);

            const auto shaderModule =
                m_compiler.create_shader_module_from_glsl_file(m_device,
                                                               renderPass->shaderSourcePath[stageIndex],
                                                               vkStage);

            if (!shaderModule)
            {
                return failure();
            }

            newPipeline.shaderModules[stageIndex] = shaderModule;

            stageCreateInfo[actualStagesCount] = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = vkStage,
                .module = shaderModule,
                .pName = "main",
            };

            ++actualStagesCount;
        }

        // TODO: Figure out inputs
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        };

        if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &newPipeline.pipelineLayout) != VK_SUCCESS)
        {
            return failure();
        }

        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = narrow_cast<u32>(desc.renderTargets.colorAttachmentFormats.size()),
            .pColorAttachmentFormats = desc.renderTargets.colorAttachmentFormats.data(),
            .depthAttachmentFormat = desc.renderTargets.depthFormat,
            .stencilAttachmentFormat = desc.renderTargets.stencilFormat,
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        const VkPipelineVertexInputStateCreateInfo vertexBufferInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        };

        constexpr VkPipelineViewportStateCreateInfo viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        const VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.f,
        };

        const VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.f,
        };

        const VkPipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        };

        const VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
            .blendConstants = {0.f},
        };

        constexpr VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        const VkPipelineDynamicStateCreateInfo dynamicState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = array_size(dynamicStates),
            .pDynamicStates = dynamicStates,
        };

        const VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &pipelineRenderingCreateInfo,
            .stageCount = actualStagesCount,
            .pStages = stageCreateInfo,
            .pVertexInputState = &vertexBufferInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = newPipeline.pipelineLayout,
            .renderPass = nullptr,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        if (vkCreateGraphicsPipelines(m_device, nullptr, 1, &pipelineInfo, nullptr, &newPipeline.pipeline) ==
            VK_SUCCESS)
        {
            renderPass->variants.push_back({.hash = expectedHash, .pipeline = pipelineHandle});
            m_lastRenderPipelineId = pipelineHandle.value;
            return pipelineHandle;
        }

        return failure();
    }

    void render_pass_manager::bind(VkCommandBuffer commandBuffer, handle<render_pipeline> handle)
    {
        const auto* pipeline = m_renderPipelines.try_find(handle);
        OBLO_ASSERT(pipeline);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    }
}