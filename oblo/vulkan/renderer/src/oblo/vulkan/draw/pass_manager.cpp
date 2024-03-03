#include <oblo/vulkan/draw/pass_manager.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/file_utility.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/log.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/draw/descriptor_set_pool.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/draw/mesh_table.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/draw/texture_registry.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/shader_compiler.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

#include <efsw/efsw.hpp>
#include <spirv_cross/spirv_cross.hpp>

#include <optional>

namespace oblo::vk
{
    namespace
    {
        constexpr u32 TextureSamplerDescriptorSet{1};
        constexpr u32 Textures2DDescriptorSet{2};
        constexpr u32 TexturesSamplerBinding{32};
        constexpr u32 Textures2DBinding{33};

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

        struct compute_pass_variant
        {
            u64 hash;
            h32<compute_pipeline> pipeline;
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
            pipeline_stages stage2;
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

        struct watch_listener final : efsw::FileWatchListener
        {
            void handleFileAction(efsw::WatchID, const std::string&, const std::string&, efsw::Action, std::string)
            {
                anyEvent = true;
            }

            std::atomic<bool> anyEvent{};
        };

        struct vertex_inputs_reflection
        {
            VkVertexInputBindingDescription* bindingDescs;
            VkVertexInputAttributeDescription* attributeDescs;
            u32 count;
        };
    }

    struct render_pass
    {
        h32<string> name;
        u8 stagesCount{0};

        std::filesystem::path shaderSourcePath[MaxPipelineStages];
        pipeline_stages stages[MaxPipelineStages];

        std::vector<render_pass_variant> variants;

        std::unique_ptr<watch_listener> watcher;
    };

    struct compute_pass
    {
        h32<string> name;

        std::filesystem::path shaderSourcePath;

        std::vector<compute_pass_variant> variants;

        std::unique_ptr<watch_listener> watcher;
    };

    struct base_pipeline
    {
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;

        shader_resource vertexInputs;
        std::vector<shader_resource> resources;
        std::vector<descriptor_binding> descriptorSetBindings;

        VkDescriptorSetLayout descriptorSetLayout{};

        bool requiresTextures2D{};

        const char* label{};
    };

    struct render_pipeline : base_pipeline
    {
        VkShaderModule shaderModules[MaxPipelineStages];

        // TODO: Active stages (e.g. tessellation on/off)
        // TODO: Active options
    };

    struct compute_pipeline : base_pipeline
    {
        VkShaderModule shaderModule;
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

        void destroy_pipeline(VkDevice device, const compute_pipeline& variant)
        {
            if (const auto pipeline = variant.pipeline)
            {
                vkDestroyPipeline(device, pipeline, nullptr);
            }

            if (const auto pipelineLayout = variant.pipelineLayout)
            {
                vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            }

            if (variant.shaderModule)
            {
                vkDestroyShaderModule(device, variant.shaderModule, nullptr);
            }
        }

        struct includer final : shader_compiler::include_handler
        {
            explicit includer(frame_allocator& allocator) : allocator{allocator} {}

            frame_allocator& get_allocator() override
            {
                return allocator;
            }

            bool resolve(std::string_view header, std::filesystem::path& outPath) override
            {
                for (auto& path : systemIncludePaths)
                {
                    outPath = path;
                    outPath /= header;
                    outPath.concat(".glsl");

                    if (std::error_code ec; std::filesystem::exists(outPath, ec))
                    {
                        resolvedIncludes.emplace_back(outPath);
                        return true;
                    }
                }

                return false;
            }

            frame_allocator& allocator;
            std::vector<std::filesystem::path> systemIncludePaths;
            std::vector<std::filesystem::path> resolvedIncludes;
        };

        template <typename Pass, typename Pipelines>
        void poll_hot_reloading(VkDevice device, Pass& pass, Pipelines& pipelines)
        {
            if (bool expected{true}; pass.watcher->anyEvent.compare_exchange_weak(expected, false))
            {
                for (auto& variant : pass.variants)
                {
                    if (auto* const pipeline = pipelines.try_find(variant.pipeline))
                    {
                        // TODO: Does destruction need to be deferred?
                        pipelines.erase(variant.pipeline);
                        destroy_pipeline(device, *pipeline);
                    }

                    pass.variants.clear();
                }
            }
        }

        u64 hash_defines(std::span<const h32<string>> defines)
        {
            u64 hash{0};

            // Consider defines at least for now, but order matters here, which is undesirable
            for (const auto define : defines)
            {
                hash = hash_mix(hash, hash_all<std::hash>(define.value));
            }

            return hash;
        }

        struct fixed_string_buffer
        {
            char buffer[2048];
            u32 length;

            operator std::string_view() const noexcept
            {
                return {buffer, length};
            }
        };

        fixed_string_buffer make_debug_name(
            const string_interner& interner, h32<string> name, const std::filesystem::path& filePath)
        {
            fixed_string_buffer debugName;

            auto const [end, size] = std::format_to_n(debugName.buffer,
                array_size(debugName.buffer),
                "[{}] {}",
                interner.str(name),
                filePath.filename().string());

            debugName.length = narrow_cast<u32>(size);

            return debugName;
        };
    }

    struct pass_manager::impl
    {
        frame_allocator frameAllocator;
        includer includer{frameAllocator};

        const vulkan_context* vkCtx{};
        VkDevice device{};
        h32_flat_pool_dense_map<compute_pass> computePasses;
        h32_flat_pool_dense_map<render_pass> renderPasses;
        h32_flat_pool_dense_map<render_pipeline> renderPipelines;
        h32_flat_pool_dense_map<compute_pipeline> computePipelines;
        string_interner* interner{};
        descriptor_set_pool descriptorSetPool;
        descriptor_set_pool texturesDescriptorSetPool;
        const texture_registry* textureRegistry{};
        h32<buffer> dummy{};
        std::optional<efsw::FileWatcher> fileWatcher;
        VkDescriptorSetLayout samplersSetLayout{};
        VkDescriptorSetLayout textures2DSetLayout{};

        VkDescriptorSet currentSamplersDescriptor{};
        VkDescriptorSet currentTextures2DDescriptor{};

        VkSampler samplers[1]{};

        u32 subgroupSize;

        VkShaderModule create_shader_module(VkShaderStageFlagBits vkStage,
            const std::filesystem::path& filePath,
            std::span<const h32<string>> defines,
            std::string_view debugName,
            const shader_compiler::options& compilerOptions,
            std::vector<u32>& spirv);

        bool create_pipeline_layout(base_pipeline& newPipeline);

        void create_reflection(base_pipeline& newPipeline,
            VkShaderStageFlagBits vkStage,
            std::span<const u32> spirv,
            vertex_inputs_reflection& vertexInputsReflection);

        template <typename F>
        VkDescriptorSet create_descriptor_set(VkDescriptorSetLayout descriptorSetLayout,
            const base_pipeline& pipeline,
            std::span<const buffer_binding_table* const> bindingTables,
            F&& fallback);
    };

    VkShaderModule pass_manager::impl::create_shader_module(VkShaderStageFlagBits vkStage,
        const std::filesystem::path& filePath,
        std::span<const h32<string>> defines,
        std::string_view debugName,
        const shader_compiler::options& compilerOptions,
        std::vector<u32>& spirv)
    {
        const auto sourceCode = load_text_file_into_memory(frameAllocator, filePath);

        u32 requiredDefinesLength{0};

        char builtInDefinesBuffer[64];

        auto* const builtInEnd = std::format_to(builtInDefinesBuffer,
            R"(#define OBLO_SUBGROUP_SIZE {}
)",
            subgroupSize);

        const u64 builtInDefinesLength = u64(builtInEnd - builtInDefinesBuffer);

        OBLO_ASSERT(builtInEnd - builtInDefinesBuffer <= array_size(builtInDefinesBuffer));

        for (const h32 define : defines)
        {
            constexpr auto fixedSize = std::string_view{"#define \n"}.size();
            requiredDefinesLength += u32(fixedSize + interner->str(define).size());
        }

        constexpr std::string_view lineDirective{"#line 0\n"};

        const auto firstLineEnd = std::find(sourceCode.begin(), sourceCode.end(), '\n');
        const auto firstLineLen = 1 + (firstLineEnd - sourceCode.begin());

        auto sourceWithDefines = allocate_n_span<char>(frameAllocator,
            sourceCode.size() + builtInDefinesLength + requiredDefinesLength + firstLineLen + lineDirective.size());

        auto it = sourceWithDefines.begin();

        // We copy the first line first, because it must contain the #version directive
        it = std::copy(sourceCode.begin(), firstLineEnd, it);
        *it = '\n';
        ++it;

        it = std::copy(builtInDefinesBuffer, builtInEnd, it);

        for (const auto define : defines)
        {
            constexpr std::string_view directive{"#define "};
            it = std::copy(directive.begin(), directive.end(), it);

            const auto str = interner->str(define);
            it = std::copy(str.begin(), str.end(), it);

            *it = '\n';
            ++it;
        }

        it = std::copy(lineDirective.begin(), lineDirective.end(), it);

        const auto end = std::copy(firstLineEnd, sourceCode.end(), it);
        const auto processedSourceCode = sourceWithDefines.subspan(0, end - sourceWithDefines.begin());

        spirv.clear();

        // Clear the resolved includes, we keep track of them for adding watches
        includer.resolvedIncludes.clear();

        if (!shader_compiler::compile_glsl_to_spirv(debugName,
                {processedSourceCode.data(), processedSourceCode.size()},
                vkStage,
                spirv,
                compilerOptions))
        {
            return nullptr;
        }

        return shader_compiler::create_shader_module_from_spirv(device, spirv);
    }

    bool pass_manager::impl::create_pipeline_layout(base_pipeline& newPipeline)
    {
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

        // TODO: We could merge the bindings with the same name and kind that belong to different stages, to make
        // descriptors smaller

        newPipeline.descriptorSetLayout = descriptorSetPool.get_or_add_layout(newPipeline.descriptorSetBindings);

        VkDescriptorSetLayout descriptorSetLayouts[3] = {newPipeline.descriptorSetLayout};
        u32 descriptorSetLayoutsCount{newPipeline.descriptorSetLayout != nullptr};

        if (newPipeline.requiresTextures2D)
        {
            descriptorSetLayouts[descriptorSetLayoutsCount++] = samplersSetLayout;
            descriptorSetLayouts[descriptorSetLayoutsCount++] = textures2DSetLayout;
        }

        // TODO: Figure out inputs
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = descriptorSetLayoutsCount,
            .pSetLayouts = descriptorSetLayouts,
        };

        return vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &newPipeline.pipelineLayout) == VK_SUCCESS;
    }

    void pass_manager::impl::create_reflection(base_pipeline& newPipeline,
        VkShaderStageFlagBits vkStage,
        std::span<const u32> spirv,
        vertex_inputs_reflection& vertexInputsReflection)
    {
        spirv_cross::Compiler compiler{spirv.data(), spirv.size()};

        const auto shaderResources = compiler.get_shader_resources();

        if (vkStage == VK_SHADER_STAGE_VERTEX_BIT)
        {
            vertexInputsReflection.count = u32(shaderResources.stage_inputs.size());

            if (vertexInputsReflection.count > 0)
            {
                vertexInputsReflection.bindingDescs =
                    allocate_n<VkVertexInputBindingDescription>(frameAllocator, vertexInputsReflection.count);
                vertexInputsReflection.attributeDescs =
                    allocate_n<VkVertexInputAttributeDescription>(frameAllocator, vertexInputsReflection.count);
            }

            u32 vertexAttributeIndex = 0;

            for (const auto& stageInput : shaderResources.stage_inputs)
            {
                const auto name = interner->get_or_add(stageInput.name);
                const auto location = compiler.get_decoration(stageInput.id, spv::DecorationLocation);

                newPipeline.resources.push_back({
                    .name = name,
                    .location = location,
                    .binding = vertexAttributeIndex,
                    .kind = resource_kind::vertex_stage_input,
                });

                const spirv_cross::SPIRType& type = compiler.get_type(stageInput.type_id);

                vertexInputsReflection.bindingDescs[vertexAttributeIndex] = {
                    .binding = vertexAttributeIndex,
                    .stride = get_type_byte_size(type),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                };

                vertexInputsReflection.attributeDescs[vertexAttributeIndex] = {
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
            const auto set = compiler.get_decoration(storageBuffer.id, spv::DecorationDescriptorSet);

            if (set != 0)
            {
                continue;
            }

            // TODO: We are ignoring the descriptor set here
            const auto name = interner->get_or_add(storageBuffer.name);
            const auto location = compiler.get_decoration(storageBuffer.id, spv::DecorationLocation);
            const auto binding = compiler.get_decoration(storageBuffer.id, spv::DecorationBinding);

            newPipeline.resources.push_back({
                .name = name,
                .location = location,
                .binding = binding,
                .kind = resource_kind::storage_buffer,
            });

            newPipeline.descriptorSetBindings.push_back({
                .name = name,
                .binding = binding,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .stageFlags = VkShaderStageFlags(vkStage),
            });
        }

        for (const auto& uniformBuffer : shaderResources.uniform_buffers)
        {
            const auto set = compiler.get_decoration(uniformBuffer.id, spv::DecorationDescriptorSet);

            if (set != 0)
            {
                continue;
            }

            const auto name = interner->get_or_add(uniformBuffer.name);
            const auto location = compiler.get_decoration(uniformBuffer.id, spv::DecorationLocation);
            const auto binding = compiler.get_decoration(uniformBuffer.id, spv::DecorationBinding);

            newPipeline.resources.push_back({
                .name = name,
                .location = location,
                .binding = binding,
                .kind = resource_kind::uniform_buffer,
            });

            newPipeline.descriptorSetBindings.push_back({
                .name = name,
                .binding = binding,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .stageFlags = VkShaderStageFlags(vkStage),
            });
        }

        for (const auto& image : shaderResources.separate_images)
        {
            const auto set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);

            if (set == Textures2DDescriptorSet)
            {
                newPipeline.requiresTextures2D = true;
                break;
            }
        }
    }

    template <typename F>
    VkDescriptorSet pass_manager::impl::create_descriptor_set(VkDescriptorSetLayout descriptorSetLayout,
        const base_pipeline& pipeline,
        std::span<const buffer_binding_table* const> bindingTables,
        F&& fallback)
    {
        const VkDescriptorSet descriptorSet = descriptorSetPool.acquire(descriptorSetLayout);

        constexpr u32 MaxWrites{64};

        u32 buffersCount{0};
        u32 writesCount{0};

        VkDescriptorBufferInfo bufferInfo[MaxWrites];
        VkWriteDescriptorSet descriptorSetWrites[MaxWrites];

        auto writeToDescriptorSet = [descriptorSet, &bufferInfo, &descriptorSetWrites, &buffersCount, &writesCount](
                                        const descriptor_binding& binding,
                                        const buffer& buffer)
        {
            // TODO: Handle more
            OBLO_ASSERT(buffersCount < MaxWrites);
            OBLO_ASSERT(writesCount < MaxWrites);
            OBLO_ASSERT(buffer.buffer);

            bufferInfo[buffersCount] = {
                .buffer = buffer.buffer,
                .offset = buffer.offset,
                .range = buffer.size,
            };

            descriptorSetWrites[writesCount] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSet,
                .dstBinding = binding.binding,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = binding.descriptorType,
                .pBufferInfo = bufferInfo + buffersCount,
            };

            ++buffersCount;
            ++writesCount;
        };

        for (const auto& binding : pipeline.descriptorSetBindings)
        {
            bool found = false;

            for (const auto* const table : bindingTables)
            {
                auto* const buffer = table->try_find(binding.name);

                if (buffer)
                {
                    writeToDescriptorSet(binding, *buffer);
                    found = true;
                    break;
                }
            }

            if (found)
            {
                continue;
            }

            const buffer fallbackBuffer = fallback(binding.name);

            if (fallbackBuffer.buffer)
            {
                writeToDescriptorSet(binding, fallbackBuffer);
                continue;
            }

            OBLO_ASSERT(!found);
            log::debug("Unable to find matching buffer for binding {}", interner->str(binding.name));
        }

        if (writesCount > 0)
        {
            vkUpdateDescriptorSets(device, writesCount, descriptorSetWrites, 0, nullptr);
        }

        return descriptorSet;
    }

    pass_manager::pass_manager() = default;
    pass_manager::~pass_manager() = default;

    void pass_manager::init(const vulkan_context& vkContext,
        string_interner& interner,
        const h32<buffer> dummy,
        const texture_registry& textureRegistry)
    {
        m_impl = std::make_unique<impl>();

        m_impl->frameAllocator.init(1u << 22);

        m_impl->vkCtx = &vkContext;
        m_impl->device = vkContext.get_device();
        m_impl->interner = &interner;
        m_impl->dummy = dummy;

        m_impl->textureRegistry = &textureRegistry;

        shader_compiler::init();

        {
            const VkSamplerCreateInfo samplerInfo{
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .mipLodBias = 0.0f,
                .compareEnable = false,
                .compareOp = VK_COMPARE_OP_ALWAYS,
                .minLod = 0.0f,
                .maxLod = 0.0f,
                .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                .unnormalizedCoordinates = false,
            };

            vkCreateSampler(vkContext.get_device(),
                &samplerInfo,
                vkContext.get_allocator().get_allocation_callbacks(),
                &m_impl->samplers[0]);
        }

        {
            constexpr VkDescriptorPoolSize descriptorPoolSizes[] = {
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64},
            };

            m_impl->descriptorSetPool.init(vkContext,
                128,
                VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                descriptorPoolSizes);
        }

        {
            const VkDescriptorPoolSize descriptorPoolSizes[] = {
                {VK_DESCRIPTOR_TYPE_SAMPLER, array_size(m_impl->samplers)},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureRegistry.get_max_descriptor_count()},
            };

            m_impl->texturesDescriptorSetPool.init(vkContext,
                128,
                VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
                descriptorPoolSizes);
        }

        {
            const VkDescriptorSetLayoutBinding vkBindings[] = {
                {
                    .binding = TexturesSamplerBinding,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .descriptorCount = array_size(m_impl->samplers),
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .pImmutableSamplers = m_impl->samplers,
                },
            };

            const VkDescriptorSetLayoutCreateInfo layoutInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = array_size(vkBindings),
                .pBindings = vkBindings,
            };

            vkCreateDescriptorSetLayout(vkContext.get_device(),
                &layoutInfo,
                vkContext.get_allocator().get_allocation_callbacks(),
                &m_impl->samplersSetLayout);
        }

        {
            // We can only really have 1 bindless descriptor per set, only the last one can have variable count.
            constexpr VkDescriptorBindingFlags bindlessFlags[] = {
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
                    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT,
            };

            const VkDescriptorSetLayoutBinding vkBindings[] = {
                {
                    .binding = Textures2DBinding,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .descriptorCount = textureRegistry.get_max_descriptor_count(),
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
            };

            const VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extendedInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
                .bindingCount = array_size(bindlessFlags),
                .pBindingFlags = bindlessFlags,
            };

            const VkDescriptorSetLayoutCreateInfo layoutInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &extendedInfo,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
                .bindingCount = array_size(vkBindings),
                .pBindings = vkBindings,
            };

            vkCreateDescriptorSetLayout(vkContext.get_device(),
                &layoutInfo,
                vkContext.get_allocator().get_allocation_callbacks(),
                &m_impl->textures2DSetLayout);
        }

        auto& watcher = m_impl->fileWatcher.emplace();
        watcher.watch();

        const auto subgroupProperties = vkContext.get_physical_device_subgroup_properties();
        m_impl->subgroupSize = subgroupProperties.subgroupSize;
    }

    void pass_manager::shutdown(vulkan_context& vkContext)
    {
        if (!m_impl)
        {
            return;
        }

        if (const auto device = m_impl->device)
        {
            for (const auto& renderPipeline : m_impl->renderPipelines.values())
            {
                // TODO: These should be deferred instead
                destroy_pipeline(device, renderPipeline);
            }

            for (const auto& computePipeline : m_impl->computePipelines.values())
            {
                // TODO: These should be deferred instead
                destroy_pipeline(device, computePipeline);
            }

            shader_compiler::shutdown();
        }

        for (auto sampler : m_impl->samplers)
        {
            if (sampler)
            {
                vkContext.destroy_deferred(sampler, vkContext.get_submit_index());
            }
        }

        vkContext.destroy_deferred(m_impl->textures2DSetLayout, vkContext.get_submit_index());
        vkContext.destroy_deferred(m_impl->samplersSetLayout, vkContext.get_submit_index());

        m_impl->descriptorSetPool.shutdown(vkContext);
        m_impl->texturesDescriptorSetPool.shutdown(vkContext);

        m_impl.reset();
    }

    void pass_manager::set_system_include_paths(std::span<const std::filesystem::path> paths)
    {
        m_impl->includer.systemIncludePaths.assign(paths.begin(), paths.end());
    }

    h32<render_pass> pass_manager::register_render_pass(const render_pass_initializer& desc)
    {
        const auto [it, handle] = m_impl->renderPasses.emplace();
        OBLO_ASSERT(handle);

        auto& renderPass = *it;

        renderPass.name = m_impl->interner->get_or_add(desc.name);

        renderPass.stagesCount = 0;
        renderPass.watcher = std::make_unique<watch_listener>();

        for (const auto& stage : desc.stages)
        {
            renderPass.shaderSourcePath[renderPass.stagesCount] = stage.shaderSourcePath;
            renderPass.stages[renderPass.stagesCount] = stage.stage;
            ++renderPass.stagesCount;

            m_impl->fileWatcher->addWatch(stage.shaderSourcePath.parent_path().string(), renderPass.watcher.get());
        }

        return handle;
    }

    h32<compute_pass> pass_manager::register_compute_pass(const compute_pass_initializer& desc)
    {
        const auto [it, handle] = m_impl->computePasses.emplace();
        OBLO_ASSERT(handle);

        auto& computePass = *it;

        computePass.name = m_impl->interner->get_or_add(desc.name);
        computePass.watcher = std::make_unique<watch_listener>();

        computePass.shaderSourcePath = desc.shaderSourcePath;

        m_impl->fileWatcher->addWatch(desc.shaderSourcePath.parent_path().string(), computePass.watcher.get());

        return handle;
    }

    h32<render_pipeline> pass_manager::get_or_create_pipeline(h32<render_pass> renderPassHandle,
        const render_pipeline_initializer& desc)
    {
        auto* const renderPass = m_impl->renderPasses.try_find(renderPassHandle);

        if (!renderPass)
        {
            return {};
        }

        poll_hot_reloading(m_impl->device, *renderPass, m_impl->renderPipelines);

        const u64 definesHash = hash_defines(desc.defines);

        // The whole initializer should be considered, but we only look at defines for now
        const u64 expectedHash = hash_mix(hash_all<std::hash>(renderPassHandle.value), definesHash);

        if (const auto variantIt = std::find_if(renderPass->variants.begin(),
                renderPass->variants.end(),
                [expectedHash](const render_pass_variant& variant) { return variant.hash == expectedHash; });
            variantIt != renderPass->variants.end())
        {
            return variantIt->pipeline;
        }

        const auto restore = m_impl->frameAllocator.make_scoped_restore();

        const auto [pipelineIt, pipelineHandle] = m_impl->renderPipelines.emplace();
        OBLO_ASSERT(pipelineHandle);
        auto& newPipeline = *pipelineIt;

        newPipeline.label = m_impl->interner->c_str(renderPass->name);

        const auto failure = [this, &newPipeline, pipelineHandle, renderPass, expectedHash]
        {
            destroy_pipeline(m_impl->device, newPipeline);
            m_impl->renderPipelines.erase(pipelineHandle);
            // We push an invalid variant so we avoid trying to rebuild a failed pipeline every frame
            renderPass->variants.emplace_back().hash = expectedHash;
            return h32<render_pipeline>{};
        };

        VkPipelineShaderStageCreateInfo stageCreateInfo[MaxPipelineStages]{};
        u32 actualStagesCount{0};

        std::vector<unsigned> spirv;
        spirv.reserve(1u << 16);

        vertex_inputs_reflection vertexInputReflection{};

        const shader_compiler::options compilerOptions{
            .includeHandler = &m_impl->includer,
            .codeOptimization = WithShaderCodeOptimizations,
        };

        for (u8 stageIndex = 0; stageIndex < renderPass->stagesCount; ++stageIndex)
        {
            const auto pipelineStage = renderPass->stages[stageIndex];
            const auto vkStage = to_vulkan_stage_bits(pipelineStage);

            const auto& filePath = renderPass->shaderSourcePath[stageIndex];

            const auto shaderModule = m_impl->create_shader_module(vkStage,
                filePath,
                desc.defines,
                make_debug_name(*m_impl->interner, renderPass->name, filePath),
                compilerOptions,
                spirv);

            if (!shaderModule)
            {
                return failure();
            }

            for (const auto& include : m_impl->includer.resolvedIncludes)
            {
                m_impl->fileWatcher->addWatch(include.parent_path().string(), renderPass->watcher.get());
            }

            newPipeline.shaderModules[stageIndex] = shaderModule;

            stageCreateInfo[actualStagesCount] = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = vkStage,
                .module = shaderModule,
                .pName = "main",
            };

            m_impl->create_reflection(newPipeline, vkStage, spirv, vertexInputReflection);

            ++actualStagesCount;
        }

        if (!m_impl->create_pipeline_layout(newPipeline))
        {
            return failure();
        }

        const u32 numAttachments = u32(desc.renderTargets.colorAttachmentFormats.size());

        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = numAttachments,
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
            .vertexBindingDescriptionCount = vertexInputReflection.count,
            .pVertexBindingDescriptions = vertexInputReflection.bindingDescs,
            .vertexAttributeDescriptionCount = vertexInputReflection.count,
            .pVertexAttributeDescriptions = vertexInputReflection.attributeDescs,
        };

        constexpr VkPipelineViewportStateCreateInfo viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        const VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .flags = desc.rasterizationState.flags,
            .depthClampEnable = desc.rasterizationState.depthClampEnable,
            .rasterizerDiscardEnable = desc.rasterizationState.rasterizerDiscardEnable,
            .polygonMode = desc.rasterizationState.polygonMode,
            .cullMode = desc.rasterizationState.cullMode,
            .frontFace = desc.rasterizationState.frontFace,
            .depthBiasEnable = desc.rasterizationState.depthBiasEnable,
            .depthBiasConstantFactor = desc.rasterizationState.depthBiasConstantFactor,
            .depthBiasClamp = desc.rasterizationState.depthBiasClamp,
            .depthBiasSlopeFactor = desc.rasterizationState.depthBiasSlopeFactor,
            .lineWidth = desc.rasterizationState.lineWidth,

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

        // TODO: Just hardcoded max number of 4 right now
        const VkPipelineColorBlendAttachmentState colorBlendAttachments[] = {
            colorBlendAttachment,
            colorBlendAttachment,
            colorBlendAttachment,
            colorBlendAttachment,
        };

        OBLO_ASSERT(numAttachments <= array_size(colorBlendAttachments));

        const VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = numAttachments,
            .pAttachments = colorBlendAttachments,
            .blendConstants = {0.f},
        };

        const VkPipelineDepthStencilStateCreateInfo depthStencil{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .flags = desc.depthStencilState.flags,
            .depthTestEnable = desc.depthStencilState.depthTestEnable,
            .depthWriteEnable = desc.depthStencilState.depthWriteEnable,
            .depthCompareOp = desc.depthStencilState.depthCompareOp,
            .depthBoundsTestEnable = desc.depthStencilState.depthBoundsTestEnable,
            .stencilTestEnable = desc.depthStencilState.stencilTestEnable,
            .front = desc.depthStencilState.front,
            .back = desc.depthStencilState.back,
            .minDepthBounds = desc.depthStencilState.minDepthBounds,
            .maxDepthBounds = desc.depthStencilState.maxDepthBounds,
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
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = newPipeline.pipelineLayout,
            .renderPass = nullptr,
            .subpass = 0,
            .basePipelineHandle = nullptr,
            .basePipelineIndex = -1,
        };

        if (vkCreateGraphicsPipelines(m_impl->device, nullptr, 1, &pipelineInfo, nullptr, &newPipeline.pipeline) ==
            VK_SUCCESS)
        {
            renderPass->variants.push_back({.hash = expectedHash, .pipeline = pipelineHandle});
            return pipelineHandle;
        }

        return failure();
    }

    h32<compute_pipeline> pass_manager::get_or_create_pipeline(h32<compute_pass> computePassHandle,
        const compute_pipeline_initializer& desc)
    {
        auto* const computePass = m_impl->computePasses.try_find(computePassHandle);

        if (!computePass)
        {
            return {};
        }

        poll_hot_reloading(m_impl->device, *computePass, m_impl->computePipelines);

        const u64 definesHash = hash_defines(desc.defines);

        // The whole initializer should be considered, but we only look at defines for now
        const u64 expectedHash = hash_mix(hash_all<std::hash>(computePassHandle.value), definesHash);

        if (const auto variantIt = std::find_if(computePass->variants.begin(),
                computePass->variants.end(),
                [expectedHash](const compute_pass_variant& variant) { return variant.hash == expectedHash; });
            variantIt != computePass->variants.end())
        {
            return variantIt->pipeline;
        }

        const auto restore = m_impl->frameAllocator.make_scoped_restore();

        const auto [pipelineIt, pipelineHandle] = m_impl->computePipelines.emplace();
        OBLO_ASSERT(pipelineHandle);
        auto& newPipeline = *pipelineIt;

        newPipeline.label = m_impl->interner->c_str(computePass->name);

        const auto failure = [this, &newPipeline, pipelineHandle, computePass, expectedHash]
        {
            destroy_pipeline(m_impl->device, newPipeline);
            m_impl->computePipelines.erase(pipelineHandle);
            // We push an invalid variant so we avoid trying to rebuild a failed pipeline every frame
            computePass->variants.emplace_back().hash = expectedHash;
            return h32<compute_pipeline>{};
        };

        std::vector<unsigned> spirv;
        spirv.reserve(1u << 16);

        vertex_inputs_reflection vertexInputReflection{};

        const shader_compiler::options compilerOptions{
            .includeHandler = &m_impl->includer,
            .codeOptimization = WithShaderCodeOptimizations,
        };

        {
            constexpr auto vkStage = VK_SHADER_STAGE_COMPUTE_BIT;

            const auto& filePath = computePass->shaderSourcePath;

            const auto shaderModule = m_impl->create_shader_module(vkStage,
                filePath,
                desc.defines,
                make_debug_name(*m_impl->interner, computePass->name, filePath),
                compilerOptions,
                spirv);

            if (!shaderModule)
            {
                return failure();
            }

            for (const auto& include : m_impl->includer.resolvedIncludes)
            {
                m_impl->fileWatcher->addWatch(include.parent_path().string(), computePass->watcher.get());
            }

            newPipeline.shaderModule = shaderModule;

            m_impl->create_reflection(newPipeline, vkStage, spirv, vertexInputReflection);
        }

        if (!m_impl->create_pipeline_layout(newPipeline))
        {
            return failure();
        }

        const VkComputePipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .flags = 0,
            .stage =
                {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = newPipeline.shaderModule,
                    .pName = "main",
                },
            .layout = newPipeline.pipelineLayout,
        };

        if (vkCreateComputePipelines(m_impl->device, nullptr, 1, &pipelineInfo, nullptr, &newPipeline.pipeline) ==
            VK_SUCCESS)
        {
            computePass->variants.push_back({.hash = expectedHash, .pipeline = pipelineHandle});
            return pipelineHandle;
        }

        return failure();
    }

    void pass_manager::begin_frame()
    {
        m_impl->descriptorSetPool.begin_frame();
        m_impl->texturesDescriptorSetPool.begin_frame();

        m_impl->currentSamplersDescriptor = m_impl->texturesDescriptorSetPool.acquire(m_impl->samplersSetLayout);

        const std::span textures2DInfo = m_impl->textureRegistry->get_textures2d_info();

        if (textures2DInfo.empty())
        {
            m_impl->currentTextures2DDescriptor = {};
            return;
        }

        u32 maxBinding = u32(textures2DInfo.size());

        VkDescriptorSetVariableDescriptorCountAllocateInfoEXT countInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &maxBinding,
        };

        const VkDescriptorSet descriptorSet =
            m_impl->texturesDescriptorSetPool.acquire(m_impl->textures2DSetLayout, &countInfo);

        const VkWriteDescriptorSet descriptorSetWrites[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSet,
                .dstBinding = Textures2DBinding,
                .dstArrayElement = 0,
                .descriptorCount = u32(textures2DInfo.size()),
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .pImageInfo = textures2DInfo.data(),
            },
        };

        vkUpdateDescriptorSets(m_impl->device, array_size(descriptorSetWrites), descriptorSetWrites, 0, nullptr);

        m_impl->currentTextures2DDescriptor = descriptorSet;
    }

    void pass_manager::end_frame()
    {
        m_impl->descriptorSetPool.end_frame();
        m_impl->texturesDescriptorSetPool.end_frame();
    }

    expected<render_pass_context> pass_manager::begin_render_pass(
        VkCommandBuffer commandBuffer, h32<render_pipeline> pipelineHandle, const VkRenderingInfo& renderingInfo) const
    {
        const auto* pipeline = m_impl->renderPipelines.try_find(pipelineHandle);

        if (!pipeline)
        {
            return unspecified_error{};
        }

        const render_pass_context renderPassContext{
            .commandBuffer = commandBuffer,
            .internalPipeline = pipeline,
        };

        m_impl->vkCtx->begin_debug_label(commandBuffer, pipeline->label);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdBeginRendering(commandBuffer, &renderingInfo);

        if (pipeline->requiresTextures2D && m_impl->currentSamplersDescriptor)
        {
            vkCmdBindDescriptorSets(commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline->pipelineLayout,
                TextureSamplerDescriptorSet,
                1,
                &m_impl->currentSamplersDescriptor,
                0,
                nullptr);
        }

        if (pipeline->requiresTextures2D && m_impl->currentTextures2DDescriptor)
        {
            vkCmdBindDescriptorSets(commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline->pipelineLayout,
                Textures2DDescriptorSet,
                1,
                &m_impl->currentTextures2DDescriptor,
                0,
                nullptr);
        }

        return renderPassContext;
    }

    void pass_manager::end_render_pass(const render_pass_context& context)
    {
        vkCmdEndRendering(context.commandBuffer);
        m_impl->vkCtx->end_debug_label(context.commandBuffer);
        m_impl->frameAllocator.restore_all();
    }

    void pass_manager::draw(const render_pass_context& context,
        const resource_manager& resourceManager,
        const draw_registry& drawRegistry,
        std::span<const batch_draw_data> drawCalls,
        std::span<const buffer_binding_table* const> bindingTables)
    {
        if (drawCalls.empty())
        {
            return;
        }

        const auto* pipeline = context.internalPipeline;

        const auto& resources = pipeline->resources;

        const auto begin = resources.begin();
        const auto lastVertexAttribute = std::find_if_not(begin,
            resources.end(),
            [](const shader_resource& r) { return r.kind == resource_kind::vertex_stage_input; });

        const auto numVertexAttributes = u32(lastVertexAttribute - begin);

        // TODO: Could prepare this array once when creating the pipeline
        const std::span attributeNames = allocate_n_span<h32<string>>(m_impl->frameAllocator, numVertexAttributes);
        const std::span buffers = allocate_n_span<buffer>(m_impl->frameAllocator, numVertexAttributes);

        const auto dummy = resourceManager.get(m_impl->dummy);

        for (u32 i = 0; i < numVertexAttributes; ++i)
        {
            attributeNames[i] = resources[i].name;
            buffers[i] = dummy;
        };

        for (const auto& draw : drawCalls)
        {
            if (const auto descriptorSetLayout = pipeline->descriptorSetLayout)
            {
                const VkDescriptorSet descriptorSet = m_impl->create_descriptor_set(descriptorSetLayout,
                    *pipeline,
                    bindingTables,
                    [&draw, &drawRegistry](h32<string> bindingName)
                    {
                        const auto instanceBuffers = draw.instanceBuffers;

                        for (u32 i = 0; i < instanceBuffers.count; ++i)
                        {
                            const auto name = drawRegistry.get_name(instanceBuffers.bindings[i]);

                            if (name == bindingName)
                            {
                                return instanceBuffers.buffers[i];
                            }
                        }
                        return buffer{};
                    });

                vkCmdBindDescriptorSets(context.commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->pipelineLayout,
                    0,
                    1,
                    &descriptorSet,
                    0,
                    nullptr);

                if (draw.drawCommands.isIndexed)
                {
                    vkCmdBindIndexBuffer(context.commandBuffer,
                        draw.drawCommands.indexBuffer,
                        draw.drawCommands.indexBufferOffset,
                        draw.drawCommands.indexType);

                    vkCmdDrawIndexedIndirect(context.commandBuffer,
                        draw.drawCommands.buffer,
                        draw.drawCommands.bufferOffset,
                        draw.drawCommands.drawCount,
                        sizeof(VkDrawIndexedIndirectCommand));
                }
                else
                {
                    vkCmdDrawIndirect(context.commandBuffer,
                        draw.drawCommands.buffer,
                        draw.drawCommands.bufferOffset,
                        draw.drawCommands.drawCount,
                        sizeof(VkDrawIndirectCommand));
                }
            }
        }
    }
    expected<compute_pass_context> pass_manager::begin_compute_pass(VkCommandBuffer commandBuffer,
        h32<compute_pipeline> pipelineHandle) const
    {
        const auto* pipeline = m_impl->computePipelines.try_find(pipelineHandle);

        if (!pipeline)
        {
            return unspecified_error{};
        }

        const compute_pass_context computePassContext{
            .commandBuffer = commandBuffer,
            .internalPipeline = pipeline,
        };

        m_impl->vkCtx->begin_debug_label(commandBuffer, pipeline->label);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);

        return computePassContext;
    }

    void pass_manager::end_compute_pass(const compute_pass_context& context)
    {
        m_impl->vkCtx->end_debug_label(context.commandBuffer);
    }

    void pass_manager::dispatch(const compute_pass_context& context,
        u32 x,
        u32 y,
        u32 z,
        std::span<const buffer_binding_table* const> bindingTables)
    {
        auto* const pipeline = context.internalPipeline;

        if (const auto descriptorSetLayout = pipeline->descriptorSetLayout)
        {
            const VkDescriptorSet descriptorSet = m_impl->create_descriptor_set(descriptorSetLayout,
                *pipeline,
                bindingTables,
                [](h32<string>) { return buffer{}; });

            vkCmdBindDescriptorSets(context.commandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                pipeline->pipelineLayout,
                0,
                1,
                &descriptorSet,
                0,
                nullptr);
        }

        vkCmdDispatch(context.commandBuffer, x, y, z);
    }

    u32 pass_manager::get_subgroup_size() const
    {
        return m_impl->subgroupSize;
    }
}