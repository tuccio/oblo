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
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/draw/descriptor_set_pool.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/draw/mesh_table.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/draw/texture_registry.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/shader_cache.hpp>
#include <oblo/vulkan/shader_compiler.hpp>
#include <oblo/vulkan/texture.hpp>
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
        constexpr bool WithShaderDebugInfo{true};

        // Push constants with this names are detected through reflection to be set at each draw
        constexpr auto InstanceTableIdPushConstant = "instanceTableId";

        constexpr u8 MaxPipelineStages = u8(pipeline_stages::enum_max);

        constexpr VkShaderStageFlagBits to_vulkan_stage_bits(pipeline_stages stage)
        {
            constexpr VkShaderStageFlagBits vkStageBits[] = {
                VK_SHADER_STAGE_MESH_BIT_EXT,
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
            sampled_image,
            separate_image,
            storage_image,
        };

        struct shader_resource
        {
            h32<string> name;
            u32 location;
            u32 binding;
            resource_kind kind;
            VkShaderStageFlags stageFlags;
        };

        struct push_constant_info
        {
            VkPipelineStageFlags stages{};
            u32 size{};
            i32 instanceTableIdOffset{-1};
        };

        // This has to match the OBLO_SAMPLER_ flags in shaders
        enum class sampler : u8
        {
            linear,
            nearest,
            enum_max
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
            void handleFileAction(
                efsw::WatchID, const std::string& dir, const std::string& filename, efsw::Action, std::string)
            {
                std::lock_guard lock{mutex};
                touchedFiles.insert(std::filesystem::path{dir} / filename);
            }

            std::mutex mutex;
            std::unordered_set<std::filesystem::path> touchedFiles;
        };

        struct vertex_inputs_reflection
        {
            VkVertexInputBindingDescription* bindingDescs;
            VkVertexInputAttributeDescription* attributeDescs;
            u32 count;
        };

        bool is_buffer_binding(const descriptor_binding& binding)
        {
            switch (binding.descriptorType)
            {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                return true;

            default:
                return false;
            }
        }

        bool is_image_binding(const descriptor_binding& binding)
        {
            switch (binding.descriptorType)
            {
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                return true;

            default:
                return false;
            }
        }
    }

    struct render_pass
    {
        h32<string> name;
        u8 stagesCount{0};

        std::filesystem::path shaderSourcePath[MaxPipelineStages];
        pipeline_stages stages[MaxPipelineStages];

        dynamic_array<render_pass_variant> variants;

        bool shouldRecompile{};
    };

    struct compute_pass
    {
        h32<string> name;

        std::filesystem::path shaderSourcePath;

        dynamic_array<compute_pass_variant> variants;

        bool shouldRecompile{};
    };

    struct base_pipeline
    {
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;

        shader_resource vertexInputs;
        dynamic_array<shader_resource> resources;
        dynamic_array<descriptor_binding> descriptorSetBindings;
        flat_dense_map<h32<string>, push_constant_info> pushConstants;

        VkDescriptorSetLayout descriptorSetLayout{};

        bool requiresTextures2D{};

        const char* label{};

        u32 lastRecompilationChangeId{};
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
        void destroy_pipeline(vulkan_context& ctx, const render_pipeline& variant)
        {
            const auto submitIndex = ctx.get_submit_index();

            if (const auto pipeline = variant.pipeline)
            {
                ctx.destroy_deferred(pipeline, submitIndex);
            }

            if (const auto pipelineLayout = variant.pipelineLayout)
            {
                ctx.destroy_deferred(pipelineLayout, submitIndex);
            }

            for (const auto shaderModule : variant.shaderModules)
            {
                if (shaderModule)
                {
                    ctx.destroy_deferred(shaderModule, submitIndex);
                }
            }
        }

        void destroy_pipeline(vulkan_context& ctx, const compute_pipeline& variant)
        {
            const auto submitIndex = ctx.get_submit_index();

            if (const auto pipeline = variant.pipeline)
            {
                ctx.destroy_deferred(pipeline, submitIndex);
            }

            if (const auto pipelineLayout = variant.pipelineLayout)
            {
                ctx.destroy_deferred(pipelineLayout, submitIndex);
            }

            if (const auto shaderModule = variant.shaderModule)
            {
                ctx.destroy_deferred(shaderModule, submitIndex);
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
            dynamic_array<std::filesystem::path> systemIncludePaths;
            dynamic_array<std::filesystem::path> resolvedIncludes;
        };

        template <typename Pass, typename Pipelines>
        void poll_hot_reloading(
            const string_interner& interner, vulkan_context& vkCtx, Pass& pass, Pipelines& pipelines)
        {
            if (pass.shouldRecompile)
            {
                log::debug("Recompiling pass {}", interner.str(pass.name));

                for (auto& variant : pass.variants)
                {
                    if (auto* const pipeline = pipelines.try_find(variant.pipeline))
                    {
                        destroy_pipeline(vkCtx, *pipeline);
                        pipelines.erase(variant.pipeline);
                    }

                    pass.variants.clear();
                }

                pass.shouldRecompile = false;
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

        struct watching_passes
        {
            // Could be sets
            h32_flat_extpool_dense_map<compute_pass, bool> computePasses;
            h32_flat_extpool_dense_map<render_pass, bool> renderPasses;
        };
    }

    struct pass_manager::impl
    {
        frame_allocator frameAllocator;
        shader_cache shaderCache;
        includer includer{frameAllocator};

        vulkan_context* vkCtx{};
        VkDevice device{};
        h32_flat_pool_dense_map<compute_pass> computePasses;
        h32_flat_pool_dense_map<render_pass> renderPasses;
        h32_flat_pool_dense_map<render_pipeline> renderPipelines;
        h32_flat_pool_dense_map<compute_pipeline> computePipelines;
        string_interner* interner{};
        descriptor_set_pool descriptorSetPool;
        descriptor_set_pool texturesDescriptorSetPool;
        const texture_registry* textureRegistry{};
        buffer dummy{};
        VkDescriptorSetLayout samplersSetLayout{};
        VkDescriptorSetLayout textures2DSetLayout{};

        VkDescriptorSet currentSamplersDescriptor{};
        VkDescriptorSet currentTextures2DDescriptor{};

        VkSampler samplers[u32(sampler::enum_max)]{};

        u32 subgroupSize;

        std::string instanceDataDefines;

        watch_listener watchListener;
        std::optional<efsw::FileWatcher> fileWatcher;

        std::unordered_map<std::filesystem::path, watching_passes> fileToPassList;

        void add_watch(const std::filesystem::path& file, h32<compute_pass> pass);
        void add_watch(const std::filesystem::path& file, h32<render_pass> pass);

        VkShaderModule create_shader_module(VkShaderStageFlagBits vkStage,
            const std::filesystem::path& filePath,
            std::span<const h32<string>> defines,
            std::string_view debugName,
            const shader_compiler::options& compilerOptions,
            dynamic_array<u32>& spirv);

        bool create_pipeline_layout(base_pipeline& newPipeline);

        void create_reflection(base_pipeline& newPipeline,
            VkShaderStageFlagBits vkStage,
            std::span<const u32> spirv,
            vertex_inputs_reflection& vertexInputsReflection);

        VkDescriptorSet create_descriptor_set(VkDescriptorSetLayout descriptorSetLayout,
            const base_pipeline& pipeline,
            std::span<const binding_table* const> bindingTables);

        shader_compiler::options make_compiler_options();
    };

    void pass_manager::impl::add_watch(const std::filesystem::path& file, h32<compute_pass> pass)
    {
        const auto abs = std::filesystem::absolute(file);

        auto& watches = fileToPassList[abs];
        watches.computePasses.emplace(pass);
        fileWatcher->addWatch(abs.parent_path().string(), &watchListener);
    }

    void pass_manager::impl::add_watch(const std::filesystem::path& file, h32<render_pass> pass)
    {
        const auto abs = std::filesystem::absolute(file);

        auto& watches = fileToPassList[abs];
        watches.renderPasses.emplace(pass);
        fileWatcher->addWatch(abs.parent_path().string(), &watchListener);
    }

    VkShaderModule pass_manager::impl::create_shader_module(VkShaderStageFlagBits vkStage,
        const std::filesystem::path& filePath,
        std::span<const h32<string>> defines,
        std::string_view debugName,
        const shader_compiler::options& compilerOptions,
        dynamic_array<u32>& spirv)
    {
        const auto sourceCodeRes = load_text_file_into_memory(frameAllocator, filePath);

        if (!sourceCodeRes)
        {
            log::debug("Failed to read file {}", filePath.string());
            return nullptr;
        }

        const auto sourceCode = *sourceCodeRes;

        u32 requiredDefinesLength{0};

        char builtInDefinesBuffer[64];

        auto* const builtInEnd = std::format_to(builtInDefinesBuffer,
            R"(#define OBLO_SUBGROUP_SIZE {}
)",
            subgroupSize);

        const u64 builtInDefinesLength = u64(builtInEnd - builtInDefinesBuffer);

        OBLO_ASSERT(builtInEnd - builtInDefinesBuffer <= array_size(builtInDefinesBuffer));

        for (const auto define : defines)
        {
            constexpr auto fixedSize = std::string_view{"#define \n"}.size();
            requiredDefinesLength += u32(fixedSize + interner->str(define).size());
        }

        requiredDefinesLength += u32(instanceDataDefines.size());

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
        it = std::copy(instanceDataDefines.begin(), instanceDataDefines.end(), it);

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

        // Clear the resolved includes, we keep track of them for adding watches
        includer.resolvedIncludes.clear();

        std::span<u32> spirvData;

        if (!shaderCache.find_or_add(spirvData,
                frameAllocator,
                debugName,
                {processedSourceCode.data(), processedSourceCode.size()},
                vkStage,
                compilerOptions))
        {
            return nullptr;
        }

        spirv.assign(spirvData.begin(), spirvData.end());

        return shader_compiler::create_shader_module_from_spirv(device,
            spirvData,
            vkCtx->get_allocator().get_allocation_callbacks());
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

        for (u32 current = 0, next = 1; next < newPipeline.resources.size(); ++current)
        {
            if (shader_resource_sorting::from(newPipeline.resources[current]) ==
                shader_resource_sorting::from(newPipeline.resources[next]))
            {
                newPipeline.resources[current].stageFlags |= newPipeline.resources[next].stageFlags;

                // Remove the next but keep the order
                newPipeline.resources.erase(newPipeline.resources.begin() + next);
            }
            else
            {
                ++next;
            }
        }

        newPipeline.descriptorSetBindings.reserve(newPipeline.resources.size());

        for (const auto& resource : newPipeline.resources)
        {
            VkDescriptorType descriptorType;

            switch (resource.kind)
            {
            case resource_kind::storage_buffer:
                descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                break;

            case resource_kind::uniform_buffer:
                descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                break;

            case resource_kind::separate_image:
                descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                break;

            case resource_kind::storage_image:
                descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                break;

            case resource_kind::sampled_image:
                descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                break;

            default:
                continue;
            }

            newPipeline.descriptorSetBindings.push_back({
                .name = resource.name,
                .binding = resource.binding,
                .descriptorType = descriptorType,
                .stageFlags = resource.stageFlags,
            });
        }

        newPipeline.descriptorSetLayout = descriptorSetPool.get_or_add_layout(newPipeline.descriptorSetBindings);

        VkDescriptorSetLayout descriptorSetLayouts[3] = {newPipeline.descriptorSetLayout};
        u32 descriptorSetLayoutsCount{newPipeline.descriptorSetLayout != nullptr};

        if (newPipeline.requiresTextures2D)
        {
            descriptorSetLayouts[descriptorSetLayoutsCount++] = samplersSetLayout;
            descriptorSetLayouts[descriptorSetLayoutsCount++] = textures2DSetLayout;
        }

        buffered_array<VkPushConstantRange, 2> pushConstantRanges;

        for (const auto& pushConstant : newPipeline.pushConstants.values())
        {
            pushConstantRanges.push_back({
                .stageFlags = pushConstant.stages,
                .size = pushConstant.size,
            });
        }

        // TODO: Figure out inputs
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = descriptorSetLayoutsCount,
            .pSetLayouts = descriptorSetLayouts,
            .pushConstantRangeCount = u32(pushConstantRanges.size()),
            .pPushConstantRanges = pushConstantRanges.data(),
        };

        return vkCreatePipelineLayout(device,
                   &pipelineLayoutInfo,
                   vkCtx->get_allocator().get_allocation_callbacks(),
                   &newPipeline.pipelineLayout) == VK_SUCCESS;
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
                    .stageFlags = VkShaderStageFlags(vkStage),
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
                .stageFlags = VkShaderStageFlags(vkStage),
            });
        }

        for (const auto& storageImage : shaderResources.storage_images)
        {
            const auto set = compiler.get_decoration(storageImage.id, spv::DecorationDescriptorSet);

            if (set != 0)
            {
                continue;
            }

            const auto name = interner->get_or_add(storageImage.name);
            const auto location = compiler.get_decoration(storageImage.id, spv::DecorationLocation);
            const auto binding = compiler.get_decoration(storageImage.id, spv::DecorationBinding);

            newPipeline.resources.push_back({
                .name = name,
                .location = location,
                .binding = binding,
                .kind = resource_kind::storage_image,
                .stageFlags = VkShaderStageFlags(vkStage),
            });
        }

        for (const auto& sampledImage : shaderResources.sampled_images)
        {
            const auto set = compiler.get_decoration(sampledImage.id, spv::DecorationDescriptorSet);

            if (set != 0)
            {
                continue;
            }

            const auto name = interner->get_or_add(sampledImage.name);
            const auto location = compiler.get_decoration(sampledImage.id, spv::DecorationLocation);
            const auto binding = compiler.get_decoration(sampledImage.id, spv::DecorationBinding);

            newPipeline.resources.push_back({
                .name = name,
                .location = location,
                .binding = binding,
                .kind = resource_kind::sampled_image,
                .stageFlags = VkShaderStageFlags(vkStage),
            });
        }

        for (const auto& image : shaderResources.separate_images)
        {
            const auto set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);

            if (set == Textures2DDescriptorSet)
            {
                newPipeline.requiresTextures2D = true;
                continue;
            }

            const auto name = interner->get_or_add(image.name);
            const auto location = compiler.get_decoration(image.id, spv::DecorationLocation);
            const auto binding = compiler.get_decoration(image.id, spv::DecorationBinding);

            newPipeline.resources.push_back({
                .name = name,
                .location = location,
                .binding = binding,
                .kind = resource_kind::separate_image,
                .stageFlags = VkShaderStageFlags(vkStage),
            });
        }

        for (const auto& pushConstant : shaderResources.push_constant_buffers)
        {
            const auto name = interner->get_or_add(pushConstant.name);

            auto [it, inserted] = newPipeline.pushConstants.emplace(name);
            it->stages |= vkStage;
            it->size = 128; // We should figure if we can get the size from reflection instead

            const auto& type = compiler.get_type(pushConstant.base_type_id);

            for (u32 i = 0; i < type.member_types.size(); ++i)
            {
                const auto pcName = compiler.get_member_name(type.self, i);

                if (pcName == InstanceTableIdPushConstant)
                {
                    const auto offset = compiler.type_struct_member_offset(type, i);
                    it->instanceTableIdOffset = i32(offset);
                }
            }
        }
    }

    shader_compiler::options pass_manager::impl::make_compiler_options()
    {
        return {
            .includeHandler = &includer,
            .codeOptimization = WithShaderCodeOptimizations,
            .generateDebugInfo = WithShaderDebugInfo,
        };
    }

    VkDescriptorSet pass_manager::impl::create_descriptor_set(VkDescriptorSetLayout descriptorSetLayout,
        const base_pipeline& pipeline,
        std::span<const binding_table* const> bindingTables)
    {
        const VkDescriptorSet descriptorSet = descriptorSetPool.acquire(descriptorSetLayout);

        constexpr u32 MaxWrites{64};

        u32 buffersCount{0};
        u32 imagesCount{0};
        u32 writesCount{0};

        VkDescriptorBufferInfo bufferInfo[MaxWrites];
        VkDescriptorImageInfo imageInfo[MaxWrites];
        VkWriteDescriptorSet descriptorSetWrites[MaxWrites];

        auto writeBufferToDescriptorSet = [descriptorSet,
                                              &bufferInfo,
                                              &descriptorSetWrites,
                                              &buffersCount,
                                              &writesCount](const descriptor_binding& binding, const buffer& buffer)
        {
            OBLO_ASSERT(buffersCount < MaxWrites);
            OBLO_ASSERT(writesCount < MaxWrites);
            OBLO_ASSERT(buffer.buffer);
            OBLO_ASSERT(buffer.size > 0);

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

        auto writeImageToDescriptorSet =
            [descriptorSet,
                &imageInfo,
                &descriptorSetWrites,
                &imagesCount,
                &writesCount,
                sampler = samplers[u32(sampler::linear)]](const descriptor_binding& binding,
                const bindable_texture& texture)
        {
            OBLO_ASSERT(imagesCount < MaxWrites);
            OBLO_ASSERT(writesCount < MaxWrites);

            imageInfo[imagesCount] = {
                .sampler = sampler,
                .imageView = texture.view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL, // The only 2 allowed layouts for storage images are general and
                                                        // VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR
            };

            descriptorSetWrites[writesCount] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSet,
                .dstBinding = binding.binding,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = binding.descriptorType,
                .pImageInfo = imageInfo + imagesCount,
            };

            ++imagesCount;
            ++writesCount;
        };

        for (const auto& binding : pipeline.descriptorSetBindings)
        {
            bool found = false;

            for (const auto* const table : bindingTables)
            {
                auto* const bindableObject = table->try_find(binding.name);

                if (bindableObject)
                {
                    switch (bindableObject->kind)
                    {
                    case bindable_object_kind::buffer:
                        if (is_buffer_binding(binding))
                        {
                            writeBufferToDescriptorSet(binding, bindableObject->buffer);
                        }
                        else
                        {
                            log::debug("[{}] A binding for {} was found, but it's not a buffer as expected",
                                pipeline.label,
                                interner->str(binding.name));
                        }

                        break;
                    case bindable_object_kind::texture:
                        if (is_image_binding(binding))
                        {
                            writeImageToDescriptorSet(binding, bindableObject->texture);
                        }
                        else
                        {
                            log::debug("[{}] A binding for {} was found, but it's not a texture as expected",
                                pipeline.label,
                                interner->str(binding.name));
                        }

                        break;
                    }

                    found = true;
                    break;
                }
            }

            if (found)
            {
                continue;
            }

            log::debug("[{}] Unable to find matching buffer for binding {}",
                pipeline.label,
                interner->str(binding.name));
        }

        if (writesCount > 0)
        {
            vkUpdateDescriptorSets(device, writesCount, descriptorSetWrites, 0, nullptr);
        }

        char nameBuffer[1024];
        auto [last, n] = std::format_to_n(nameBuffer, 1023, "{} / pass_manager DescriptorSet", pipeline.label);
        *last = '\0';

        vkCtx->get_debug_utils_object().set_object_name(device, descriptorSet, nameBuffer);

        return descriptorSet;
    }

    pass_manager::pass_manager() = default;
    pass_manager::~pass_manager() = default;

    void pass_manager::init(vulkan_context& vkContext,
        string_interner& interner,
        const buffer& dummy,
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
        m_impl->shaderCache.init("./spirv");

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
                .maxLod = VK_LOD_CLAMP_NONE,
                .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                .unnormalizedCoordinates = false,
            };

            vkCreateSampler(vkContext.get_device(),
                &samplerInfo,
                vkContext.get_allocator().get_allocation_callbacks(),
                &m_impl->samplers[u32(sampler::linear)]);
        }

        {
            const VkSamplerCreateInfo samplerInfo{
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_NEAREST,
                .minFilter = VK_FILTER_NEAREST,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .mipLodBias = 0.0f,
                .compareEnable = false,
                .compareOp = VK_COMPARE_OP_ALWAYS,
                .minLod = 0.0f,
                .maxLod = VK_LOD_CLAMP_NONE,
                .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                .unnormalizedCoordinates = false,
            };

            vkCreateSampler(vkContext.get_device(),
                &samplerInfo,
                vkContext.get_allocator().get_allocation_callbacks(),
                &m_impl->samplers[u32(sampler::nearest)]);
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
                destroy_pipeline(*m_impl->vkCtx, renderPipeline);
            }

            for (const auto& computePipeline : m_impl->computePipelines.values())
            {
                destroy_pipeline(*m_impl->vkCtx, computePipeline);
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

        for (const auto& stage : desc.stages)
        {
            renderPass.shaderSourcePath[renderPass.stagesCount] = stage.shaderSourcePath;
            renderPass.stages[renderPass.stagesCount] = stage.stage;
            ++renderPass.stagesCount;

            m_impl->add_watch(stage.shaderSourcePath, handle);
        }

        return handle;
    }

    h32<compute_pass> pass_manager::register_compute_pass(const compute_pass_initializer& desc)
    {
        const auto [it, handle] = m_impl->computePasses.emplace();
        OBLO_ASSERT(handle);

        auto& computePass = *it;

        computePass.name = m_impl->interner->get_or_add(desc.name);

        computePass.shaderSourcePath = desc.shaderSourcePath;

        m_impl->add_watch(desc.shaderSourcePath, handle);

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

        poll_hot_reloading(*m_impl->interner, *m_impl->vkCtx, *renderPass, m_impl->renderPipelines);

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
            destroy_pipeline(*m_impl->vkCtx, newPipeline);
            m_impl->renderPipelines.erase(pipelineHandle);
            // We push an invalid variant so we avoid trying to rebuild a failed pipeline every frame
            renderPass->variants.emplace_back().hash = expectedHash;

            return h32<render_pipeline>{};
        };

        VkPipelineShaderStageCreateInfo stageCreateInfo[MaxPipelineStages]{};
        u32 actualStagesCount{0};

        dynamic_array<unsigned> spirv;
        spirv.reserve(1u << 16);

        vertex_inputs_reflection vertexInputReflection{};

        const shader_compiler::options compilerOptions{m_impl->make_compiler_options()};

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
                m_impl->add_watch(include, renderPassHandle);
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

        if (vkCreateGraphicsPipelines(m_impl->device,
                nullptr,
                1,
                &pipelineInfo,
                m_impl->vkCtx->get_allocator().get_allocation_callbacks(),
                &newPipeline.pipeline) == VK_SUCCESS)
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

        poll_hot_reloading(*m_impl->interner, *m_impl->vkCtx, *computePass, m_impl->computePipelines);

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
            destroy_pipeline(*m_impl->vkCtx, newPipeline);
            m_impl->computePipelines.erase(pipelineHandle);
            // We push an invalid variant so we avoid trying to rebuild a failed pipeline every frame
            computePass->variants.emplace_back().hash = expectedHash;
            return h32<compute_pipeline>{};
        };

        dynamic_array<unsigned> spirv;
        spirv.reserve(1u << 16);

        vertex_inputs_reflection vertexInputReflection{};

        const shader_compiler::options compilerOptions{m_impl->make_compiler_options()};

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
                m_impl->add_watch(include, computePassHandle);
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

        const auto debugUtils = m_impl->vkCtx->get_debug_utils_object();

        const auto samplerDescriptorSet = m_impl->texturesDescriptorSetPool.acquire(m_impl->samplersSetLayout);
        debugUtils.set_object_name(m_impl->device, samplerDescriptorSet, "Sampler Descriptor Set");

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

        debugUtils.set_object_name(m_impl->device, descriptorSet, "Textures 2D Descriptor Set");

        constexpr u32 numSamplers = u32(sampler::enum_max);
        VkDescriptorImageInfo samplers[numSamplers];

        for (u32 i = 0; i < numSamplers; ++i)
        {
            samplers[i] = {.sampler = m_impl->samplers[i]};
        }

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
            //{
            //    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            //    .dstSet = samplerDescriptorSet,
            //    .dstBinding = TexturesSamplerBinding,
            //    .dstArrayElement = 0,
            //    .descriptorCount = numSamplers,
            //    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            //    .pImageInfo = samplers,
            //},
        };

        vkUpdateDescriptorSets(m_impl->device, array_size(descriptorSetWrites), descriptorSetWrites, 0, nullptr);

        m_impl->currentTextures2DDescriptor = descriptorSet;
        m_impl->currentSamplersDescriptor = m_impl->texturesDescriptorSetPool.acquire(m_impl->samplersSetLayout);

        {
            std::lock_guard lock{m_impl->watchListener.mutex};

            for (const auto& f : m_impl->watchListener.touchedFiles)
            {
                const auto it = m_impl->fileToPassList.find(f);

                if (it == m_impl->fileToPassList.end())
                {
                    continue;
                }

                for (const auto& r : it->second.renderPasses.keys())
                {
                    m_impl->renderPasses.at(r).shouldRecompile = true;
                }

                for (const auto& c : it->second.computePasses.keys())
                {
                    m_impl->computePasses.at(c).shouldRecompile = true;
                }
            }

            m_impl->watchListener.touchedFiles.clear();
        }
    }

    void pass_manager::end_frame()
    {
        m_impl->descriptorSetPool.end_frame();
        m_impl->texturesDescriptorSetPool.end_frame();
    }

    void pass_manager::update_instance_data_defines(std::string_view defines)
    {
        m_impl->instanceDataDefines = defines;

        // Invalidate all passes as well, to trigger recompilation of shaders
        for (auto& pass : m_impl->renderPasses.values())
        {
            pass.shouldRecompile = true;
        }

        for (auto& pass : m_impl->computePasses.values())
        {
            pass.shouldRecompile = true;
        }
    }

    expected<render_pass_context> pass_manager::begin_render_pass(
        VkCommandBuffer commandBuffer, h32<render_pipeline> pipelineHandle, const VkRenderingInfo& renderingInfo) const
    {
        const auto* pipeline = m_impl->renderPipelines.try_find(pipelineHandle);

        if (!pipeline)
        {
            return unspecified_error;
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

    expected<compute_pass_context> pass_manager::begin_compute_pass(VkCommandBuffer commandBuffer,
        h32<compute_pipeline> pipelineHandle) const
    {
        const auto* pipeline = m_impl->computePipelines.try_find(pipelineHandle);

        if (!pipeline)
        {
            return unspecified_error;
        }

        const compute_pass_context computePassContext{
            .commandBuffer = commandBuffer,
            .internalPipeline = pipeline,
        };

        m_impl->vkCtx->begin_debug_label(commandBuffer, pipeline->label);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);

        if (pipeline->requiresTextures2D && m_impl->currentSamplersDescriptor)
        {
            vkCmdBindDescriptorSets(commandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
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
                VK_PIPELINE_BIND_POINT_COMPUTE,
                pipeline->pipelineLayout,
                Textures2DDescriptorSet,
                1,
                &m_impl->currentTextures2DDescriptor,
                0,
                nullptr);
        }

        return computePassContext;
    }

    void pass_manager::end_compute_pass(const compute_pass_context& context)
    {
        m_impl->vkCtx->end_debug_label(context.commandBuffer);
    }

    u32 pass_manager::get_subgroup_size() const
    {
        return m_impl->subgroupSize;
    }

    void pass_manager::push_constants(
        const render_pass_context& ctx, VkShaderStageFlags stages, u32 offset, std::span<const byte> data) const
    {
        vkCmdPushConstants(ctx.commandBuffer,
            ctx.internalPipeline->pipelineLayout,
            stages,
            offset,
            u32(data.size()),
            data.data());
    }

    void pass_manager::push_constants(
        const compute_pass_context& ctx, VkShaderStageFlags stages, u32 offset, std::span<const byte> data) const
    {
        vkCmdPushConstants(ctx.commandBuffer,
            ctx.internalPipeline->pipelineLayout,
            stages,
            offset,
            u32(data.size()),
            data.data());
    }

    void pass_manager::bind_descriptor_sets(const render_pass_context& ctx,
        std::span<const binding_table* const> bindingTables) const
    {
        const auto* pipeline = ctx.internalPipeline;

        if (const auto descriptorSetLayout = pipeline->descriptorSetLayout)
        {
            const VkDescriptorSet descriptorSet =
                m_impl->create_descriptor_set(descriptorSetLayout, *pipeline, bindingTables);

            vkCmdBindDescriptorSets(ctx.commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline->pipelineLayout,
                0,
                1,
                &descriptorSet,
                0,
                nullptr);
        }
    }

    void pass_manager::bind_descriptor_sets(const compute_pass_context& ctx,
        std::span<const binding_table* const> bindingTables) const
    {
        auto* const pipeline = ctx.internalPipeline;

        if (const auto descriptorSetLayout = pipeline->descriptorSetLayout)
        {
            const VkDescriptorSet descriptorSet =
                m_impl->create_descriptor_set(descriptorSetLayout, *pipeline, bindingTables);

            vkCmdBindDescriptorSets(ctx.commandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                pipeline->pipelineLayout,
                0,
                1,
                &descriptorSet,
                0,
                nullptr);
        }
    }
}