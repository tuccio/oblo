#include <oblo/vulkan/render_pass_manager.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/file_utility.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/log.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/mesh_table.hpp>
#include <oblo/vulkan/render_pass_initializer.hpp>
#include <oblo/vulkan/resource_manager.hpp>

#include <spirv_cross/spirv_cross.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr bool WithShaderCodeOptimizations{false};

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
            h32<render_pipeline> pipeline;
        };

        enum resource_kind : u8
        {
            vertex_stage_input,
            uniform_buffer,
            storage_buffer,
        };

        struct shader_resource
        {
            h32<string> name;
            u32 location;
            u32 binding;
            resource_kind kind;
        };

        constexpr u32 combine_type_vecsize(spirv_cross::SPIRType::BaseType type, u32 vecsize)
        {
            return (u32(type) << 2) | vecsize;
        }

        VkFormat get_type_format(const spirv_cross::SPIRType& type)
        {
            // Not really dealing with matrices here
            OBLO_ASSERT(type.columns == 1);

            switch (combine_type_vecsize(type.basetype, type.vecsize))
            {
            case combine_type_vecsize(spirv_cross::SPIRType::Float, 1):
                return VK_FORMAT_R32_SFLOAT;

            case combine_type_vecsize(spirv_cross::SPIRType::Float, 2):
                return VK_FORMAT_R32G32_SFLOAT;

            case combine_type_vecsize(spirv_cross::SPIRType::Float, 3):
                return VK_FORMAT_R32G32B32_SFLOAT;

            case combine_type_vecsize(spirv_cross::SPIRType::Float, 4):
                return VK_FORMAT_R32G32B32A32_SFLOAT;

            default:
                OBLO_ASSERT(false);
                return VK_FORMAT_UNDEFINED;
            }
        }

        u32 get_type_byte_size(const spirv_cross::SPIRType& type)
        {
            return type.columns * type.vecsize * type.width / 8;
        }
    }

    struct render_pass
    {
        h32<string> name;
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

        shader_resource vertexInputs;
        std::vector<shader_resource> resources;

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

            for (const auto shaderModule : variant.shaderModules)
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

    void render_pass_manager::init(VkDevice device, string_interner& interner, const h32<buffer> dummy)
    {
        m_device = device;
        m_interner = &interner;
        m_dummy = dummy;

        if (device)
        {
            shader_compiler::init();
        }
    }

    void render_pass_manager::shutdown()
    {
        if (m_device)
        {
            for (const auto& renderPipeline : m_renderPipelines.values())
            {
                destroy_pipeline(m_device, renderPipeline);
            }

            shader_compiler::shutdown();
            m_device = nullptr;
            m_interner = nullptr;
        }
    }

    h32<render_pass> render_pass_manager::register_render_pass(const render_pass_initializer& desc)
    {
        const h32<render_pass> handle{++m_lastRenderPassId};

        const auto [it, ok] = m_renderPasses.emplace(handle);
        OBLO_ASSERT(ok);

        auto& renderPass = *it;

        renderPass.name = m_interner->get_or_add(desc.name);

        renderPass.stagesCount = 0;

        for (const auto& stage : desc.stages)
        {
            renderPass.shaderSourcePath[renderPass.stagesCount] = stage.shaderSourcePath;
            renderPass.stages[renderPass.stagesCount] = stage.stage;
            ++renderPass.stagesCount;
        }

        return handle;
    }

    h32<render_pipeline> render_pass_manager::get_or_create_pipeline(frame_allocator& allocator,
                                                                     h32<render_pass> renderPassHandle,
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

        const auto restore = allocator.make_scoped_restore();

        const h32<render_pipeline> pipelineHandle{m_lastRenderPipelineId + 1};

        const auto [pipelineIt, ok] = m_renderPipelines.emplace(pipelineHandle);
        OBLO_ASSERT(ok);
        auto& newPipeline = *pipelineIt;

        const auto failure = [this, &newPipeline, pipelineHandle, renderPass, expectedHash]
        {
            destroy_pipeline(m_device, newPipeline);
            m_renderPipelines.erase(pipelineHandle);
            // We push an invalid handle so we avoid trying to rebuild a failed pipeline every frame
            renderPass->variants.emplace_back().hash = expectedHash;
            return h32<render_pipeline>{};
        };

        VkPipelineShaderStageCreateInfo stageCreateInfo[MaxPipelineStages]{};
        u32 actualStagesCount{0};

        std::vector<unsigned> spirv;
        spirv.reserve(4096);

        auto makeDebugName =
            [debugName = std::string{}, this](const render_pass& pass, const std::filesystem::path& filePath) mutable
        {
            debugName = "[";
            debugName += m_interner->str(pass.name);
            debugName += "] ";
            debugName += filePath.filename().string();
            return std::string_view{debugName};
        };

        VkVertexInputBindingDescription* vertexInputBindingDescs;
        VkVertexInputAttributeDescription* vertexInputAttributeDescs;
        u32 vertexInputsCount{0u};

        constexpr shader_compiler::options compilerOptions{.codeOptimization = WithShaderCodeOptimizations};

        for (u8 stageIndex = 0; stageIndex < renderPass->stagesCount; ++stageIndex)
        {
            const auto pipelineStage = renderPass->stages[stageIndex];
            const auto vkStage = to_vulkan_stage_bits(pipelineStage);

            const auto& filePath = renderPass->shaderSourcePath[stageIndex];
            const auto sourceCode = load_text_file_into_memory(allocator, filePath);

            spirv.clear();

            const std::string_view debugName{makeDebugName(*renderPass, filePath)};
            shader_compiler::compile_glsl_to_spirv(debugName,
                                                   {sourceCode.data(), sourceCode.size()},
                                                   vkStage,
                                                   spirv,
                                                   compilerOptions);

            const auto shaderModule = shader_compiler::create_shader_module_from_spirv(m_device, spirv);

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

            {
                spirv_cross::Compiler compiler{spirv.data(), spirv.size()};

                const auto shaderResources = compiler.get_shader_resources();

                if (pipelineStage == pipeline_stages::vertex)
                {
                    vertexInputsCount = u32(shaderResources.stage_inputs.size());

                    vertexInputBindingDescs = allocate_n<VkVertexInputBindingDescription>(allocator, vertexInputsCount);
                    vertexInputAttributeDescs =
                        allocate_n<VkVertexInputAttributeDescription>(allocator, vertexInputsCount);

                    u32 vertexAttributeIndex = 0;

                    for (const auto& stageInput : shaderResources.stage_inputs)
                    {
                        const auto name = m_interner->get_or_add(stageInput.name);
                        const auto location = compiler.get_decoration(stageInput.id, spv::DecorationLocation);

                        newPipeline.resources.push_back({
                            .name = name,
                            .location = location,
                            .binding = vertexAttributeIndex,
                            .kind = resource_kind::vertex_stage_input,
                        });

                        const spirv_cross::SPIRType& type = compiler.get_type(stageInput.type_id);

                        vertexInputBindingDescs[vertexAttributeIndex] = {
                            .binding = vertexAttributeIndex,
                            .stride = get_type_byte_size(type),
                            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                        };

                        vertexInputAttributeDescs[vertexAttributeIndex] = {
                            .location = location,
                            .binding = vertexAttributeIndex,
                            .format = get_type_format(type),
                            .offset = 0,
                        };

                        ++vertexAttributeIndex;
                    }
                }

                for (const auto& storageBuffer : shaderResources.storage_buffers)
                {
                    // TODO: We are ignoring the descriptor set here
                    const auto name = m_interner->get_or_add(storageBuffer.name);
                    const auto location = compiler.get_decoration(storageBuffer.id, spv::DecorationLocation);
                    const auto binding = compiler.get_decoration(storageBuffer.id, spv::DecorationBinding);

                    newPipeline.resources.push_back({
                        .name = name,
                        .location = location,
                        .binding = binding,
                        .kind = resource_kind::storage_buffer,
                    });
                }

                for (const auto& uniformBuffer : shaderResources.uniform_buffers)
                {
                    // TODO: We are ignoring the descriptor set here
                    const auto name = m_interner->get_or_add(uniformBuffer.name);
                    const auto location = compiler.get_decoration(uniformBuffer.id, spv::DecorationLocation);
                    const auto binding = compiler.get_decoration(uniformBuffer.id, spv::DecorationBinding);

                    newPipeline.resources.push_back({
                        .name = name,
                        .location = location,
                        .binding = binding,
                        .kind = resource_kind::uniform_buffer,
                    });
                }
            }

            ++actualStagesCount;
        }

        struct shader_resource_sorting
        {
            resource_kind kind;
            u32 binding;
            u32 location;

            static constexpr shader_resource_sorting from(const shader_resource& r)
            {
                return {
                    .kind = r.kind,
                    .binding = r.binding,
                    .location = r.location,
                };
            }

            constexpr auto operator<=>(const shader_resource_sorting&) const = default;
        };

        std::sort(newPipeline.resources.begin(),
                  newPipeline.resources.end(),
                  [](const shader_resource& lhs, const shader_resource& rhs)
                  { return shader_resource_sorting::from(lhs) < shader_resource_sorting::from(rhs); });

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
            .vertexBindingDescriptionCount = vertexInputsCount,
            .pVertexBindingDescriptions = vertexInputBindingDescs,
            .vertexAttributeDescriptionCount = vertexInputsCount,
            .pVertexAttributeDescriptions = vertexInputAttributeDescs,
        };

        constexpr VkPipelineViewportStateCreateInfo viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        const VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
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
            .basePipelineHandle = nullptr,
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

    void render_pass_manager::begin_rendering(render_pass_context& context, const VkRenderingInfo& renderingInfo) const
    {
        const auto* pipeline = m_renderPipelines.try_find(context.pipeline);
        OBLO_ASSERT(pipeline);
        context.internalPipeline = pipeline;

        const auto commandBuffer = context.commandBuffer;
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdBeginRendering(commandBuffer, &renderingInfo);
    }

    void render_pass_manager::end_rendering(const render_pass_context& context) const
    {
        vkCmdEndRendering(context.commandBuffer);
    }

    void render_pass_manager::bind(const render_pass_context& context,
                                   const resource_manager& resourceManager,
                                   const mesh_table& meshTable) const
    {
        const auto* pipeline = context.internalPipeline;
        auto& frameAllocator = context.frameAllocator;

        const auto& resources = pipeline->resources;

        const auto begin = resources.begin();
        const auto lastVertexAttribute =
            std::find_if_not(begin,
                             resources.end(),
                             [](const shader_resource& r) { return r.kind == resource_kind::vertex_stage_input; });

        const auto numVertexAttributes = u32(lastVertexAttribute - begin);

        // TODO: Could prepare this array once when creating the pipeline
        const std::span attributeNames = allocate_n_span<h32<string>>(frameAllocator, numVertexAttributes);
        const std::span buffers = allocate_n_span<buffer>(frameAllocator, numVertexAttributes);

        const auto dummy = resourceManager.get(m_dummy);

        for (u32 i = 0; i < numVertexAttributes; ++i)
        {
            attributeNames[i] = resources[i].name;
            buffers[i] = dummy;
        };

        meshTable.fetch_buffers(resourceManager, attributeNames, buffers, nullptr);

        auto* const vkBuffers = allocate_n<VkBuffer>(frameAllocator, numVertexAttributes);
        auto* const offsets = allocate_n<VkDeviceSize>(frameAllocator, numVertexAttributes);

        for (u32 i = 0; i < numVertexAttributes; ++i)
        {
            vkBuffers[i] = buffers[i].buffer;
            offsets[i] = buffers[i].offset;
        }

        vkCmdBindVertexBuffers(context.commandBuffer, 0, numVertexAttributes, vkBuffers, offsets);
    }
}