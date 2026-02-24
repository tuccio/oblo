#include <oblo/renderer/draw/pass_manager.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/filesystem/directory_watcher.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/iterator/enum_range.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_interner.hpp>
#include <oblo/core/string/transparent_string_hash.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/vulkan/gpu_allocator.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/renderer/draw/bindable_object.hpp>
#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/draw/compute_pass_initializer.hpp>
#include <oblo/renderer/draw/descriptor_set_pool.hpp>
#include <oblo/renderer/draw/draw_registry.hpp>
#include <oblo/renderer/draw/global_shader_options.hpp>
#include <oblo/renderer/draw/instance_data_type_registry.hpp>
#include <oblo/renderer/draw/mesh_table.hpp>
#include <oblo/renderer/draw/raytracing_pass_initializer.hpp>
#include <oblo/renderer/draw/render_pass_initializer.hpp>
#include <oblo/renderer/draw/shader_stage_utils.hpp>
#include <oblo/renderer/draw/texture_registry.hpp>
#include <oblo/trace/profile.hpp>ss
#include <oblo/vulkan/compiler/compiler_module.hpp>
#include <oblo/vulkan/compiler/shader_cache.hpp>

#include <spirv_cross/spirv_cross.hpp>

namespace oblo
{
    namespace
    {
        constexpr u32 TextureSamplerDescriptorSet{1};
        constexpr u32 Textures2DDescriptorSet{2};
        constexpr u32 TexturesSamplerBinding{32};
        constexpr u32 Textures2DBinding{33};

        // Push constants with this names are detected through reflection to be set at each draw
        constexpr auto InstanceTableIdPushConstant = "instanceTableId";

        string_view get_define_from_stage(gpu::shader_stage stage)
        {
            switch (stage)
            {
            case gpu::shader_stage::mesh:
                return "OBLO_STAGE_MESH";
            case gpu::shader_stage::vertex:
                return "OBLO_STAGE_VERTEX";
            case gpu::shader_stage::fragment:
                return "OBLO_STAGE_FRAGMENT";
            case gpu::shader_stage::raygen:
                return "OBLO_STAGE_RAYGEN";
            case gpu::shader_stage::intersection:
                return "OBLO_STAGE_INTERSECTION";
            case gpu::shader_stage::any_hit:
                return "OBLO_STAGE_ANY_HIT";
            case gpu::shader_stage::closest_hit:
                return "OBLO_STAGE_CLOSEST_HIT";
            case gpu::shader_stage::miss:
                return "OBLO_STAGE_MISS_HIT";
            case gpu::shader_stage::callable:
                return "OBLO_STAGE_CALLABLE";
            default:
                unreachable();
            }
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

        struct raytracing_pass_variant
        {
            u64 hash;
            h32<raytracing_pipeline> pipeline;
        };

        class binding_collision_checker
        {
        public:
            bool has_binding(u32 id) const
            {
                OBLO_ASSERT(id < array_size(m_bindings));
                return m_bindings[id] != nullptr;
            }

            const char* get_binding(u32 id) const
            {
                OBLO_ASSERT(id < array_size(m_bindings));
                return m_bindings[id];
            }

            bool add_binding(u32 id, const char* name)
            {
                OBLO_ASSERT(id < array_size(m_bindings));

                if (m_bindings[id])
                {
                    return false;
                }

                m_bindings[id] = name;
                return true;
            }

            void check(u32 id, const char* name, const char* label)
            {
                if (!add_binding(id, name))
                {
                    log::error(
                        "Shader binding collision detected while compiling {}. Attempted to override {} at binding "
                        "location {} with {}.",
                        label,
                        get_binding(id),
                        id,
                        name);

                    OBLO_ASSERT(false);
                }
            }

        private:
            const char* m_bindings[256]{};
        };

        enum resource_kind : u8
        {
            vertex_stage_input,
            uniform_buffer,
            storage_buffer,
            sampled_image,
            separate_image,
            storage_image,
            acceleration_structure,
        };

        struct shader_resource
        {
            h32<string> name;
            u32 location;
            u32 binding;
            resource_kind kind;
            bool readOnly;
            flags<gpu::shader_stage> stageFlags;
        };

        struct push_constant_info
        {
            flags<gpu::shader_stage> stages{};
            u32 size{};
            i32 instanceTableIdOffset{-1};
        };

        // This has to match the OBLO_SAMPLER_ flags in shaders
        enum class sampler : u8
        {
            linear_repeat,
            linear_clamp_edge,
            linear_clamp_black,
            linear_clamp_white,
            nearest,
            anisotropic,
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

        bool is_printf_include(string_view path)
        {
            return path.ends_with("renderer/debug/printf.glsl");
        }
    }

    struct render_pass
    {
        h32<string> name;

        dynamic_array<string> shaderSourcePath;
        dynamic_array<gpu::shader_stage> stages;

        dynamic_array<render_pass_variant> variants;

        bool shouldRecompile{};
    };

    struct compute_pass
    {
        h32<string> name;

        string shaderSourcePath;

        dynamic_array<compute_pass_variant> variants;

        bool shouldRecompile{};
    };

    struct raytracing_shader
    {
        u32 shaderIndex = VK_SHADER_UNUSED_KHR;
    };

    struct raytracing_hit_group
    {
        raytracing_hit_type type;
        dynamic_array<raytracing_shader> shaders;
    };

    struct raytracing_pass
    {
        h32<string> name;

        u32 generation = VK_SHADER_UNUSED_KHR;

        dynamic_array<raytracing_shader> miss;

        dynamic_array<raytracing_hit_group> hitGroups;

        dynamic_array<raytracing_pass_variant> variants;

        dynamic_array<string> shaderSourcePaths;
        dynamic_array<gpu::shader_stage> shaderStages;

        u32 shadersCount{};
        u32 groupsCount{};

        bool shouldRecompile{};
    };

    struct base_pipeline
    {
        shader_resource vertexInputs;
        dynamic_array<shader_resource> resources;
        dynamic_array<descriptor_binding> descriptorSetBindings;
        flat_dense_map<h32<string>, push_constant_info> pushConstants;

        VkDescriptorSetLayout descriptorSetLayout{};

        bool requiresTextures2D{};
        bool hasPrintfInclude{};

        const char* label{};

        u32 lastRecompilationChangeId{};

        void init(const char* name)
        {
            label = name;
        }
    };

    struct render_pipeline : base_pipeline
    {
        // TODO: Active stages (e.g. tessellation on/off)
        // TODO: Active options
        h32<gpu::graphics_pipeline> pipeline{};
    };

    struct compute_pipeline : base_pipeline
    {
        h32<gpu::compute_pipeline> pipeline{};
    };

    struct raytracing_pipeline : base_pipeline
    {
        h32<gpu::raytracing_pipeline> pipeline{};
    };

    namespace
    {
        void destroy_pipeline(gpu::gpu_instance& ctx, const render_pipeline& variant)
        {
            ctx.destroy_deferred(variant.pipeline, ctx.get_submit_index());
        }

        void destroy_pipeline(gpu::gpu_instance& ctx, const compute_pipeline& variant)
        {
            ctx.destroy_deferred(variant.pipeline, ctx.get_submit_index());
        }

        void destroy_pipeline(gpu::gpu_instance& ctx, const raytracing_pipeline& variant)
        {
            ctx.destroy_deferred(variant.pipeline, ctx.get_submit_index());
        }

        template <typename Pass, typename Pipelines>
        void poll_hot_reloading(
            const string_interner& interner, gpu::gpu_instance& gpu, Pass& pass, Pipelines& pipelines)
        {
            if (pass.shouldRecompile)
            {
                log::debug("Recompiling pass {}", interner.str(pass.name));

                for (auto& variant : pass.variants)
                {
                    if (auto* const pipeline = pipelines.try_find(variant.pipeline))
                    {
                        destroy_pipeline(gpu, *pipeline);
                        pipelines.erase(variant.pipeline);
                    }

                    pass.variants.clear();
                }

                pass.shouldRecompile = false;
            }
        }

        u64 hash_defines(std::span<const hashed_string_view> defines)
        {
            u64 hash{0};

            // Consider defines at least for now, but order matters here, which is undesirable
            for (const auto define : defines)
            {
                hash = hash_mix(hash, define.hash());
            }

            return hash;
        }

        cstring_view make_debug_name(
            string_builder& builder, const string_interner& interner, h32<string> name, string_view filePath)
        {
            builder.clear().format("[{}] {}", interner.str(name), filesystem::filename(filePath));
            return builder.view();
        };

        struct watching_passes
        {
            // Could be sets
            h32_flat_extpool_dense_map<compute_pass, bool> computePasses;
            h32_flat_extpool_dense_map<render_pass, bool> renderPasses;
            h32_flat_extpool_dense_map<raytracing_pass, bool> raytracingPasses;
        };
    }

    struct pass_manager::impl
    {
        frame_allocator frameAllocator;

        unique_ptr<vk::shader_compiler> glslcCompiler;
        unique_ptr<vk::shader_compiler> glslangCompiler;
        option_proxy_struct<global_shader_options_proxy> shaderCompilerOptions;
        options_manager* optionsManager{};
        vk::shader_cache shaderCache;

        gpu::gpu_instance* gpu{};
        h32_flat_pool_dense_map<compute_pass> computePasses;
        h32_flat_pool_dense_map<render_pass> renderPasses;
        h32_flat_pool_dense_map<raytracing_pass> raytracingPasses;
        h32_flat_pool_dense_map<render_pipeline> renderPipelines;
        h32_flat_pool_dense_map<compute_pipeline> computePipelines;
        h32_flat_pool_dense_map<raytracing_pipeline> raytracingPipelines;
        string_interner* interner{};
        const texture_registry* textureRegistry{};
        h32<gpu::bind_group_layout> samplersSetLayout{};
        h32<gpu::bind_group_layout> textures2DSetLayout{};

        h32<gpu::bind_group> currentSamplersDescriptor{};
        h32<gpu::bind_group> currentTextures2DDescriptor{};

        h32<gpu::sampler> samplers[u32(sampler::enum_max)]{};

        u32 subgroupSize;

        string_builder instanceDataDefines;

        dynamic_array<filesystem::directory_watcher> watchers;

        bool enableShaderOptimizations{false};
        bool emitDebugInfo{false};
        bool emitLineDirectives{true};
        bool enableProfiling{true};
        bool enableProfilingThisFrame{false};
        bool globallyEnablePrintf{false};
        bool isRayTracingEnabled{true};

        std::unordered_map<string, watching_passes, transparent_string_hash, std::equal_to<>> fileToPassList;

        void add_watch(string_view file, h32<compute_pass> pass);
        void add_watch(string_view file, h32<render_pass> pass);
        void add_watch(string_view file, h32<raytracing_pass> pass);

        h32<gpu::shader_module> create_shader_module(gpu::shader_stage stage,
            cstring_view filePath,
            std::span<const string_view> builtInDefines,
            std::span<const hashed_string_view> defines,
            string_view debugName,
            const vk::shader_compiler_options& compilerOptions,
            vk::shader_compiler::result& result);

        bool create_pipeline_layout(base_pipeline& newPipeline);

        void create_reflection(base_pipeline& newPipeline,
            gpu::shader_stage stage,
            std::span<const u32> spirv,
            vertex_inputs_reflection& vertexInputsReflection);

        VkDescriptorSet create_descriptor_set(
            VkDescriptorSetLayout descriptorSetLayout, const base_pipeline& pipeline, locate_binding_fn findBinding);

        vk::shader_compiler_options make_compiler_options();

        template <typename Filter = decltype([](auto&&) { return true; })>
        void invalidate_all_passes(Filter&& f = {});

        void propagate_pipeline_invalidation();
    };

    void pass_manager::impl::add_watch(string_view file, h32<compute_pass> pass)
    {
        string_builder abs;
        abs.append(file).make_absolute_path();

        auto& watches = fileToPassList[abs.as<string>()];
        watches.computePasses.emplace(pass);
    }

    void pass_manager::impl::add_watch(string_view file, h32<render_pass> pass)
    {
        string_builder abs;
        abs.append(file).make_absolute_path();

        auto& watches = fileToPassList[abs.as<string>()];
        watches.renderPasses.emplace(pass);
    }

    void pass_manager::impl::add_watch(string_view file, h32<raytracing_pass> pass)
    {
        string_builder abs;
        abs.append(file).make_absolute_path();

        auto& watches = fileToPassList[abs.as<string>()];
        watches.raytracingPasses.emplace(pass);
    }

    h32<gpu::shader_module> pass_manager::impl::create_shader_module(gpu::shader_stage stage,
        cstring_view filePath,
        std::span<const string_view> builtInDefines,
        std::span<const hashed_string_view> userDefines,
        string_view debugName,
        const vk::shader_compiler_options& compilerOptions,
        vk::shader_compiler::result& result)
    {
        OBLO_PROFILE_SCOPE();

        string_builder preambleBuilder{&frameAllocator};
        preambleBuilder.reserve(1u << 16);

        if (globallyEnablePrintf)
        {
            preambleBuilder.format("#define OBLO_DEBUG_PRINTF 1\n");
            preambleBuilder.format("#extension GL_EXT_debug_printf : enable\n");
        }

        preambleBuilder.format("#define OBLO_SUBGROUP_SIZE {}\n", subgroupSize);

        for (const auto& define : builtInDefines)
        {
            preambleBuilder.format("#define {}\n", define);
        }

        preambleBuilder.append(instanceDataDefines);

        for (const auto& define : userDefines)
        {
            preambleBuilder.format("#define {}\n", define);
        }

        const vk::shader_preprocessor_options preprocessorOptions = {
            .emitLineDirectives = emitLineDirectives,
            .preamble = preambleBuilder.as<string_view>(),
        };

        result = shaderCache
                     .find_or_compile(frameAllocator, filePath, stage, preprocessorOptions, compilerOptions, debugName);

        if (result.has_errors())
        {
            log::error("Shader compilation failed for {}\n{}", debugName, result.get_error_message());
            return {};
        }

        gpu->create_shader_module({
            .format = gpu::shader_module_format::spirv,
            .data = as_bytes(result.get_spirv()),
        });
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

        // Merge the same resources that belong to different stages together, but we need to keep the order intact
        for (u32 current = 0; current + 1 < newPipeline.resources.size();)
        {
            const u32 next = current + 1;

            if (shader_resource_sorting::from(newPipeline.resources[current]) ==
                shader_resource_sorting::from(newPipeline.resources[next]))
            {
                newPipeline.resources[current].stageFlags |= newPipeline.resources[next].stageFlags;
                newPipeline.resources[current].readOnly |= newPipeline.resources[next].readOnly;

                // Remove the next but keep the order
                newPipeline.resources.erase(newPipeline.resources.begin() + next);
            }
            else
            {
                ++current;
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

            case resource_kind::acceleration_structure:
                descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                break;

            default:
                continue;
            }

            newPipeline.descriptorSetBindings.push_back({
                .name = resource.name,
                .binding = resource.binding,
                .descriptorType = descriptorType,
                .stageFlags = resource.stageFlags,
                .readOnly = resource.readOnly,
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

        const auto success = vkCreatePipelineLayout(device,
                                 &pipelineLayoutInfo,
                                 vkCtx->get_allocator().get_allocation_callbacks(),
                                 &newPipeline.pipelineLayout) == VK_SUCCESS;

        if (success)
        {
            const auto& debugUtils = vkCtx->get_debug_utils_object();
            debugUtils.set_object_name(device, newPipeline.pipelineLayout, newPipeline.label);
        }

        return success;
    }

    void pass_manager::impl::create_reflection(base_pipeline& newPipeline,
        gpu::shader_stage stage,
        std::span<const u32> spirv,
        vertex_inputs_reflection& vertexInputsReflection)
    {
        spirv_cross::Compiler compiler{spirv.data(), spirv.size()};

        const auto shaderResources = compiler.get_shader_resources();

        if (stage == gpu::shader_stage::vertex)
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
                    .stageFlags = stage,
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
        binding_collision_checker collisionChecker;

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
            const auto readOnly = compiler.get_decoration(storageBuffer.id, spv::DecorationNonWritable) != 0;

            newPipeline.resources.push_back({
                .name = name,
                .location = location,
                .binding = binding,
                .kind = resource_kind::storage_buffer,
                .readOnly = readOnly,
                .stageFlags = stage,
            });

            collisionChecker.check(binding, storageBuffer.name.c_str(), newPipeline.label);
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
                .stageFlags = stage,
            });

            collisionChecker.check(binding, uniformBuffer.name.c_str(), newPipeline.label);
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
                .stageFlags = stage,
            });

            collisionChecker.check(binding, storageImage.name.c_str(), newPipeline.label);
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
                .stageFlags = stage,
            });

            collisionChecker.check(binding, sampledImage.name.c_str(), newPipeline.label);
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
                .stageFlags = stage,
            });

            collisionChecker.check(binding, image.name.c_str(), newPipeline.label);
        }

        for (const auto& pushConstant : shaderResources.push_constant_buffers)
        {
            const auto name = interner->get_or_add(pushConstant.name);

            auto [it, inserted] = newPipeline.pushConstants.emplace(name);
            it->stages |= stage;
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

        for (const auto& accelerationStructure : shaderResources.acceleration_structures)
        {
            const auto name = interner->get_or_add(accelerationStructure.name);
            const auto location = compiler.get_decoration(accelerationStructure.id, spv::DecorationLocation);
            const auto binding = compiler.get_decoration(accelerationStructure.id, spv::DecorationBinding);

            newPipeline.resources.push_back({
                .name = name,
                .location = location,
                .binding = binding,
                .kind = resource_kind::acceleration_structure,
                .stageFlags = stage,
            });

            collisionChecker.check(binding, accelerationStructure.name.c_str(), newPipeline.label);
        }
    }

    vk::shader_compiler_options pass_manager::impl::make_compiler_options()
    {
        return {
            .codeOptimization = enableShaderOptimizations,
            .generateDebugInfo = emitDebugInfo,
        };
    }

    template <typename Filter>
    void pass_manager::impl::invalidate_all_passes(Filter&& f)
    {
        const auto processPasses = [&f](auto& passes, auto& pipelines)
        {
            for (auto& pass : passes.values())
            {
                bool skip = true;

                for (const auto& variant : pass.variants)
                {
                    if (!variant.pipeline)
                    {
                        // This happens when a pipeline failed to compile
                        break;
                    }

                    if (f(pipelines.at(variant.pipeline)))
                    {
                        skip = false;
                        break;
                    }
                }

                if (skip)
                {
                    continue;
                }

                pass.shouldRecompile = true;
            }
        };

        processPasses(renderPasses, renderPipelines);
        processPasses(computePasses, computePipelines);
        processPasses(raytracingPasses, raytracingPipelines);
    }

    void pass_manager::impl::propagate_pipeline_invalidation()
    {
        for (auto& watcher : watchers)
        {
            const auto r = watcher.process(
                [this](const filesystem::directory_watcher_event evt)
                {
                    if (evt.eventKind == filesystem::directory_watcher_event_kind::modified)
                    {
                        const auto it = fileToPassList.find(evt.path);

                        if (it == fileToPassList.end())
                        {
                            return;
                        }

                        for (const auto& r : it->second.renderPasses.keys())
                        {
                            renderPasses.at(r).shouldRecompile = true;
                        }

                        for (const auto& c : it->second.computePasses.keys())
                        {
                            computePasses.at(c).shouldRecompile = true;
                        }

                        for (const auto& c : it->second.raytracingPasses.keys())
                        {
                            raytracingPasses.at(c).shouldRecompile = true;
                        }
                    }
                });

            if (!r)
            {
                log::debug("Processing of directory watcher {} failed", watcher.get_directory());
            }
        }
    }

    VkDescriptorSet pass_manager::impl::create_descriptor_set(
        VkDescriptorSetLayout descriptorSetLayout, const base_pipeline& pipeline, locate_binding_fn locateBinding)
    {
        const VkDescriptorSet descriptorSet = descriptorSetPool.acquire(descriptorSetLayout);

        vkCtx->get_debug_utils_object().set_object_name(device,
            descriptorSet,
            string_builder{}.format("{} / pass_manager", pipeline.label).c_str());

        constexpr u32 MaxWrites{64};

        u32 buffersCount{0};
        u32 imagesCount{0};
        u32 writesCount{0};
        u32 accelerationStructuresCount{0};

        VkDescriptorBufferInfo bufferInfo[MaxWrites];
        VkDescriptorImageInfo imageInfo[MaxWrites];
        VkWriteDescriptorSet descriptorSetWrites[MaxWrites];
        VkWriteDescriptorSetAccelerationStructureKHR asSetWrites[MaxWrites];

        auto writeBufferToDescriptorSet =
            [descriptorSet, &bufferInfo, &descriptorSetWrites, &buffersCount, &writesCount](
                const descriptor_binding& binding,
                const bindable_buffer& buffer)
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
                sampler = samplers[u32(sampler::linear_repeat)]](const descriptor_binding& binding,
                const bindable_texture& texture)
        {
            OBLO_ASSERT(imagesCount < MaxWrites);
            OBLO_ASSERT(writesCount < MaxWrites);

            imageInfo[imagesCount] = {
                .sampler = sampler,
                .imageView = texture.view,
                .imageLayout = texture.layout,
            };

            descriptorSetWrites[writesCount] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSet,
                .dstBinding = binding.binding,
                .descriptorCount = 1,
                .descriptorType = binding.descriptorType,
                .pImageInfo = imageInfo + imagesCount,
            };

            ++imagesCount;
            ++writesCount;
        };

        auto writeAccelerationStructureToDescriptorSet =
            [descriptorSet, &asSetWrites, &descriptorSetWrites, &accelerationStructuresCount, &writesCount](
                const descriptor_binding& binding,
                const bindable_acceleration_structure& as)
        {
            OBLO_ASSERT(accelerationStructuresCount < MaxWrites);
            OBLO_ASSERT(writesCount < MaxWrites);

            asSetWrites[accelerationStructuresCount] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                .accelerationStructureCount = 1,
                .pAccelerationStructures = &as.handle,
            };

            descriptorSetWrites[writesCount] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = &asSetWrites[accelerationStructuresCount],
                .dstSet = descriptorSet,
                .dstBinding = binding.binding,
                .descriptorCount = 1,
                .descriptorType = binding.descriptorType,
            };

            ++accelerationStructuresCount;
            ++writesCount;
        };

        for (const auto& binding : pipeline.descriptorSetBindings)
        {
            const auto bindableObject = locateBinding(binding);

            switch (bindableObject.kind)
            {
            case bindable_resource_kind::buffer:
                if (is_buffer_binding(binding))
                {
                    writeBufferToDescriptorSet(binding, bindableObject.buffer);
                }
                else
                {
                    log::debug("[{}] A binding for {} was found, but it's not a buffer as expected",
                        pipeline.label,
                        interner->str(binding.name));
                }

                break;
            case bindable_resource_kind::texture:
                if (is_image_binding(binding))
                {
                    writeImageToDescriptorSet(binding, bindableObject.texture);
                }
                else
                {
                    log::debug("[{}] A binding for {} was found, but it's not a texture as expected",
                        pipeline.label,
                        interner->str(binding.name));
                }

                break;
            case bindable_resource_kind::acceleration_structure:
                if (binding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                {
                    writeAccelerationStructureToDescriptorSet(binding, bindableObject.accelerationStructure);
                }
                else
                {
                    log::debug("[{}] A binding for {} was found, but it's not an acceleration structure as "
                               "expected",
                        pipeline.label,
                        interner->str(binding.name));
                }

                break;

            case bindable_resource_kind::none:
                log::debug("[{}] Unable to find matching buffer for binding {}",
                    pipeline.label,
                    interner->str(binding.name));

                break;

            default:
                unreachable();
            }
        }

        if (writesCount > 0)
        {
            vkUpdateDescriptorSets(device, writesCount, descriptorSetWrites, 0, nullptr);
        }

        return descriptorSet;
    }

    pass_manager::pass_manager() = default;
    pass_manager::~pass_manager() = default;

    void pass_manager::init(gpu::gpu_instance& gpu,
        string_interner& interner,
        const texture_registry& textureRegistry,
        const instance_data_type_registry& instanceDataTypeRegistry)
    {
        m_impl = allocate_unique<impl>();

        m_impl->frameAllocator.init(1u << 22);

        m_impl->gpu = &gpu;
        m_impl->interner = &interner;

        m_impl->textureRegistry = &textureRegistry;

        instanceDataTypeRegistry.generate_defines(m_impl->instanceDataDefines);

        auto* compilerModule = module_manager::get().find<vk::compiler_module>();
        m_impl->glslcCompiler = compilerModule->make_glslc_compiler("./glslc");
        m_impl->glslangCompiler = compilerModule->make_glslang_compiler();

        auto* const optionsModule = module_manager::get().find<options_module>();
        m_impl->optionsManager = &optionsModule->manager();
        m_impl->shaderCompilerOptions.init(*m_impl->optionsManager);

        m_impl->shaderCache.init("./spirv");

        {
            constexpr gpu::sampler_descriptor samplerInfo{
                .magFilter = gpu::sampler_filter::linear,
                .minFilter = gpu::sampler_filter::linear,
                .mipmapMode = gpu::sampler_mipmap_mode::linear,
                .addressModeU = gpu::sampler_address_mode::repeat,
                .addressModeV = gpu::sampler_address_mode::repeat,
                .addressModeW = gpu::sampler_address_mode::repeat,
                .mipLodBias = 0.0f,
                .anisotropyEnable = false,
                .compareEnable = false,
                .compareOp = gpu::compare_op::always,
                .minLod = 0.0f,
                .maxLod = gpu::sampler_descriptor::lod_clamp_none,
                .borderColor = gpu::border_color::int_opaque_black,
                .debugLabel = "linear_repeat",
            };

            m_impl->samplers[u32(sampler::linear_repeat)] = gpu.create_sampler(samplerInfo).value_or({});
        }
        {
            constexpr gpu::sampler_descriptor samplerInfo{
                .magFilter = gpu::sampler_filter::linear,
                .minFilter = gpu::sampler_filter::linear,
                .mipmapMode = gpu::sampler_mipmap_mode::linear,
                .addressModeU = gpu::sampler_address_mode::clamp_to_edge,
                .addressModeV = gpu::sampler_address_mode::clamp_to_edge,
                .addressModeW = gpu::sampler_address_mode::clamp_to_edge,
                .mipLodBias = 0.0f,
                .anisotropyEnable = false,
                .compareEnable = false,
                .compareOp = gpu::compare_op::always,
                .minLod = 0.0f,
                .maxLod = gpu::sampler_descriptor::lod_clamp_none,
                .borderColor = gpu::border_color::int_opaque_black,
                .debugLabel = "linear_clamp_edge",
            };

            m_impl->samplers[u32(sampler::linear_clamp_edge)] = gpu.create_sampler(samplerInfo).value_or({});
        }

        {
            constexpr gpu::sampler_descriptor samplerInfo{
                .magFilter = gpu::sampler_filter::linear,
                .minFilter = gpu::sampler_filter::linear,
                .mipmapMode = gpu::sampler_mipmap_mode::linear,
                .addressModeU = gpu::sampler_address_mode::clamp_to_border,
                .addressModeV = gpu::sampler_address_mode::clamp_to_border,
                .addressModeW = gpu::sampler_address_mode::clamp_to_border,
                .mipLodBias = 0.0f,
                .anisotropyEnable = false,
                .compareEnable = false,
                .compareOp = gpu::compare_op::always,
                .minLod = 0.0f,
                .maxLod = gpu::sampler_descriptor::lod_clamp_none,
                .borderColor = gpu::border_color::int_opaque_black,
                .debugLabel = "linear_clamp_black",
            };

            m_impl->samplers[u32(sampler::linear_clamp_black)] = gpu.create_sampler(samplerInfo).value_or({});
        }

        {
            constexpr gpu::sampler_descriptor samplerInfo{
                .magFilter = gpu::sampler_filter::linear,
                .minFilter = gpu::sampler_filter::linear,
                .mipmapMode = gpu::sampler_mipmap_mode::linear,
                .addressModeU = gpu::sampler_address_mode::clamp_to_border,
                .addressModeV = gpu::sampler_address_mode::clamp_to_border,
                .addressModeW = gpu::sampler_address_mode::clamp_to_border,
                .mipLodBias = 0.0f,
                .anisotropyEnable = false,
                .compareEnable = false,
                .compareOp = gpu::compare_op::always,
                .minLod = 0.0f,
                .maxLod = gpu::sampler_descriptor::lod_clamp_none,
                .borderColor = gpu::border_color::int_opaque_white,
                .debugLabel = "linear_clamp_white",
            };

            m_impl->samplers[u32(sampler::linear_clamp_white)] = gpu.create_sampler(samplerInfo).value_or({});
        }

        {
            constexpr gpu::sampler_descriptor samplerInfo{
                .magFilter = gpu::sampler_filter::nearest,
                .minFilter = gpu::sampler_filter::nearest,
                .mipmapMode = gpu::sampler_mipmap_mode::nearest,
                .addressModeU = gpu::sampler_address_mode::repeat,
                .addressModeV = gpu::sampler_address_mode::repeat,
                .addressModeW = gpu::sampler_address_mode::repeat,
                .mipLodBias = 0.0f,
                .anisotropyEnable = false,
                .compareEnable = false,
                .compareOp = gpu::compare_op::always,
                .minLod = 0.0f,
                .maxLod = gpu::sampler_descriptor::lod_clamp_none,
                .debugLabel = "nearest",
            };

            m_impl->samplers[u32(sampler::nearest)] = gpu.create_sampler(samplerInfo).value_or({});
        }

        {
            constexpr gpu::sampler_descriptor samplerInfo{
                .magFilter = gpu::sampler_filter::linear,
                .minFilter = gpu::sampler_filter::linear,
                .mipmapMode = gpu::sampler_mipmap_mode::linear,
                .addressModeU = gpu::sampler_address_mode::repeat,
                .addressModeV = gpu::sampler_address_mode::repeat,
                .addressModeW = gpu::sampler_address_mode::repeat,
                .mipLodBias = 0.0f,
                .anisotropyEnable = true,
                .maxAnisotropy = 16.0f,
                .compareEnable = false,
                .compareOp = gpu::compare_op::always,
                .minLod = 0.0f,
                .maxLod = gpu::sampler_descriptor::lod_clamp_none,
                .debugLabel = "anisotropic",
            };

            m_impl->samplers[u32(sampler::anisotropic)] = gpu.create_sampler(samplerInfo).value_or({});
        }

        const expected samplersSetLayout = gpu.create_bind_group_layout({
            .bindings = make_span_initializer<gpu::bind_group_binding>({{
                .binding = TexturesSamplerBinding,
                .bindingKind = gpu::resource_binding_kind::sampler,
                .shaderStages = flags<gpu::shader_stage>::all(),
                .immutableSamplers = m_impl->samplers,
            }}),
        });

        m_impl->samplersSetLayout = samplersSetLayout.assert_value_or({});

        // We can only really have 1 bindless descriptor per set, only the last one can have variable count.
        const expected bindlessTexturesLayout = gpu.create_bind_group_layout({
            .bindings = make_span_initializer<gpu::bind_group_binding>({{
                .binding = Textures2DBinding,
                .bindingKind = gpu::resource_binding_kind::sampled_image,
                .shaderStages = flags<gpu::shader_stage>::all(),
                .bindlessCount = textureRegistry.get_max_descriptor_count(),
            }}),
        });

        m_impl->textures2DSetLayout = bindlessTexturesLayout.assert_value_or({});

        {
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
                .bindingCount = array_size32(bindlessFlags),
                .pBindingFlags = bindlessFlags,
            };

            const VkDescriptorSetLayoutCreateInfo layoutInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &extendedInfo,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
                .bindingCount = array_size32(vkBindings),
                .pBindings = vkBindings,
            };

            vkCreateDescriptorSetLayout(vkContext.get_device(),
                &layoutInfo,
                vkContext.get_allocator().get_allocation_callbacks(),
                &m_impl->textures2DSetLayout);
        }

        const gpu::device_info& deviceInfo = gpu.get_device_info();
        m_impl->subgroupSize = deviceInfo.subgroupSize;
    }

    void pass_manager::shutdown()
    {
        if (!m_impl)
        {
            return;
        }

        gpu::gpu_instance& gpu = *m_impl->gpu;

        for (const auto& renderPipeline : m_impl->renderPipelines.values())
        {
            destroy_pipeline(gpu, renderPipeline);
        }

        for (const auto& computePipeline : m_impl->computePipelines.values())
        {
            destroy_pipeline(gpu, computePipeline);
        }

        for (const auto& raytracingPipeline : m_impl->raytracingPipelines.values())
        {
            destroy_pipeline(gpu, raytracingPipeline);
        }

        for (auto sampler : m_impl->samplers)
        {
            if (sampler)
            {
                gpu->destroy_deferred(sampler, gpu.get_submit_index());
            }
        }

        gpu.destroy_deferred(m_impl->textures2DSetLayout, gpu.get_submit_index());
        gpu.destroy_deferred(m_impl->samplersSetLayout, gpu.get_submit_index());

        m_impl.reset();
    }

    void pass_manager::set_system_include_paths(std::span<const string_view> paths)
    {
        if (m_impl->glslcCompiler)
        {
            m_impl->glslcCompiler->init({
                .includeDirectories = paths,
            });
        }

        if (m_impl->glslangCompiler)
        {
            m_impl->glslangCompiler->init({
                .includeDirectories = paths,
            });
        }

        m_impl->watchers.clear();
        m_impl->watchers.reserve(paths.size());

        for (auto& p : paths)
        {
            auto& w = m_impl->watchers.emplace_back();

            if (!w.init({.path = p, .isRecursive = true}))
            {
                log::debug(
                    "Failed to initialize directory watch on {}. Shader hot-reloading might not work as intended.",
                    p);

                m_impl->watchers.pop_back();
            }
        }
    }

    void pass_manager::set_raytracing_enabled(bool isRayTracingEnabled)
    {
        m_impl->isRayTracingEnabled = isRayTracingEnabled;
    }

    h32<render_pass> pass_manager::register_render_pass(const render_pass_initializer& desc)
    {
        const auto [it, handle] = m_impl->renderPasses.emplace();
        OBLO_ASSERT(handle);

        auto& renderPass = *it;

        renderPass.name = m_impl->interner->get_or_add(desc.name);

        for (const auto& stage : desc.stages)
        {
            renderPass.shaderSourcePath.emplace_back(stage.shaderSourcePath);
            renderPass.stages.emplace_back(stage.stage);

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

    namespace
    {
        expected<gpu::shader_stage> deduce_rt_shader_stage(string_view p)
        {
            auto&& ext = filesystem::extension(p);

            if (ext == ".rgen")
            {
                return gpu::shader_stage::raygen;
            }

            if (ext == ".rint")
            {
                return gpu::shader_stage::intersection;
            }

            if (ext == ".rahit")
            {
                return gpu::shader_stage::any_hit;
            }

            if (ext == ".rchit")
            {
                return gpu::shader_stage::closest_hit;
            }

            if (ext == ".rmiss")
            {
                return gpu::shader_stage::miss;
            }

            if (ext == ".rcall")
            {
                return gpu::shader_stage::callable;
            }

            return "Unrecognized file extension"_err;
        }
    }

    h32<raytracing_pass> pass_manager::register_raytracing_pass(const raytracing_pass_initializer& desc)
    {
        const auto [it, handle] = m_impl->raytracingPasses.emplace();
        OBLO_ASSERT(handle);

        auto& renderPass = *it;

        renderPass.name = m_impl->interner->get_or_add(desc.name);

        constexpr u32 noShader = ~u32{};

        const auto appendShader = [&](string_view source)
        {
            if (!source.empty())
            {
                const expected stage = deduce_rt_shader_stage(source);

                if (!stage)
                {
                    return noShader;
                }

                const auto size = renderPass.shaderSourcePaths.size();
                renderPass.shaderSourcePaths.push_back(source.as<string>());
                renderPass.shaderStages.push_back(*stage);

                m_impl->add_watch(source, handle);

                return u32(size);
            }

            return noShader;
        };

        renderPass.generation = appendShader(desc.generation);

        renderPass.miss.reserve(desc.miss.size());

        for (const auto& miss : desc.miss)
        {
            renderPass.miss.push_back({.shaderIndex = appendShader(miss)});
        }

        renderPass.hitGroups.reserve(desc.hitGroups.size());

        for (const auto& hg : desc.hitGroups)
        {
            auto& group = renderPass.hitGroups.push_back({.type = hg.type});

            group.shaders.reserve(hg.shaders.size());

            for (const auto& shader : hg.shaders)
            {
                const u32 shaderIndex = appendShader(shader);

                group.shaders.emplace_back() = {
                    .shaderIndex = shaderIndex,
                };
            }
        }

        renderPass.shadersCount = narrow_cast<u32>(renderPass.shaderSourcePaths.size());
        renderPass.groupsCount =
            u32{renderPass.generation != noShader} + u32(renderPass.miss.size()) + u32(desc.hitGroups.size());

        return handle;
    }

    h32<render_pipeline> pass_manager::get_or_create_pipeline(h32<render_pass> renderPassHandle,
        const render_pipeline_initializer& desc)
    {
        OBLO_PROFILE_SCOPE();

        auto* const renderPass = m_impl->renderPasses.try_find(renderPassHandle);

        if (!renderPass)
        {
            return {};
        }

        poll_hot_reloading(*m_impl->interner, *m_impl->gpu, *renderPass, m_impl->renderPipelines);

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

        newPipeline.init(m_impl->interner->c_str(renderPass->name));

        buffered_array<h32<gpu::shader_module>, 4> shaderModules;

        // Destroy the modules at the end, whether we succeed or not
        const auto cleanup = finally(
            [this, &shaderModules]
            {
                for (const h32 module : shaderModules)
                {
                    m_impl->gpu->destroy(module);
                }
            });

        const auto failure = [this, &newPipeline, pipelineHandle, renderPass, expectedHash]
        {
            destroy_pipeline(*m_impl->gpu, newPipeline);
            m_impl->renderPipelines.erase(pipelineHandle);
            // We push an invalid variant so we avoid trying to rebuild a failed pipeline every frame
            renderPass->variants.emplace_back().hash = expectedHash;

            return h32<render_pipeline>{};
        };

        buffered_array<gpu::graphics_pipeline_stage, 4> stageCreateInfo;

        vertex_inputs_reflection vertexInputReflection{};

        const vk::shader_compiler_options compilerOptions{m_impl->make_compiler_options()};

        string_view builtInDefines[2]{"OBLO_PIPELINE_RENDER"};

        string_builder builder;

        deque<string_view> sourceFiles;

        for (usize stageIndex = 0; stageIndex < renderPass->stages.size(); ++stageIndex)
        {
            const gpu::shader_stage shaderStage = renderPass->stages[stageIndex];

            const auto& filePath = renderPass->shaderSourcePath[stageIndex];

            builtInDefines[1] = get_define_from_stage(shaderStage);

            vk::shader_compiler::result compilerResult;

            const auto shaderModule = m_impl->create_shader_module(shaderStage,
                filePath,
                builtInDefines,
                desc.defines,
                make_debug_name(builder, *m_impl->interner, renderPass->name, filePath),
                compilerOptions,
                compilerResult);

            if (!shaderModule)
            {
                return failure();
            }

            sourceFiles.clear();
            compilerResult.get_source_files(sourceFiles);

            for (const auto& include : sourceFiles)
            {
                m_impl->add_watch(include, renderPassHandle);
                newPipeline.hasPrintfInclude |= is_printf_include(include);
            }

            shaderModules.emplace_back(shaderModule);

            stageCreateInfo.push_back({
                .stage = shaderStage,
                .shaderModule = shaderModule,
                .entryFunction = "main",
            });

            m_impl->create_reflection(newPipeline, shaderStage, compilerResult.get_spirv(), vertexInputReflection);
        }

        if (!m_impl->create_pipeline_layout(newPipeline))
        {
            return failure();
        }

        static_assert(sizeof(VkFormat) == sizeof(gpu::image_format), "We rely on this when reinterpret casting");

        const u32 numAttachments = u32(desc.renderTargets.colorAttachmentFormats.size());

        const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = numAttachments,
            .pColorAttachmentFormats =
                reinterpret_cast<const VkFormat*>(desc.renderTargets.colorAttachmentFormats.data()),
            .depthAttachmentFormat = convert_to_vk(desc.renderTargets.depthFormat),
            .stencilAttachmentFormat = convert_to_vk(desc.renderTargets.stencilFormat),
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = convert_to_vk(desc.primitiveTopology),
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
            .flags = convert_to_vk(desc.rasterizationState.flags),
            .depthClampEnable = desc.rasterizationState.depthClampEnable,
            .rasterizerDiscardEnable = desc.rasterizationState.rasterizerDiscardEnable,
            .polygonMode = convert_to_vk(desc.rasterizationState.polygonMode),
            .cullMode = convert_to_vk(desc.rasterizationState.cullMode),
            .frontFace = convert_to_vk(desc.rasterizationState.frontFace),
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

        buffered_array<VkPipelineColorBlendAttachmentState, 8> colorBlendAttachments;
        colorBlendAttachments.reserve(desc.renderTargets.blendStates.size());

        for (auto& attachment : desc.renderTargets.blendStates)
        {
            colorBlendAttachments.push_back({
                .blendEnable = attachment.enable,
                .srcColorBlendFactor = convert_to_vk(attachment.srcColorBlendFactor),
                .dstColorBlendFactor = convert_to_vk(attachment.dstColorBlendFactor),
                .colorBlendOp = convert_to_vk(attachment.colorBlendOp),
                .srcAlphaBlendFactor = convert_to_vk(attachment.srcAlphaBlendFactor),
                .dstAlphaBlendFactor = convert_to_vk(attachment.dstAlphaBlendFactor),
                .alphaBlendOp = convert_to_vk(attachment.alphaBlendOp),
                .colorWriteMask = convert_to_vk(attachment.colorWriteMask),
            });
        }

        const VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = colorBlendAttachments.size32(),
            .pAttachments = colorBlendAttachments.data(),
            .blendConstants = {},
        };

        const VkPipelineDepthStencilStateCreateInfo depthStencil{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .flags = convert_to_vk(desc.depthStencilState.flags),
            .depthTestEnable = desc.depthStencilState.depthTestEnable,
            .depthWriteEnable = desc.depthStencilState.depthWriteEnable,
            .depthCompareOp = convert_to_vk(desc.depthStencilState.depthCompareOp),
            .depthBoundsTestEnable = desc.depthStencilState.depthBoundsTestEnable,
            .stencilTestEnable = desc.depthStencilState.stencilTestEnable,
            .front =
                {
                    .failOp = convert_to_vk(desc.depthStencilState.front.failOp),
                    .passOp = convert_to_vk(desc.depthStencilState.front.passOp),
                    .depthFailOp = convert_to_vk(desc.depthStencilState.front.depthFailOp),
                    .compareOp = convert_to_vk(desc.depthStencilState.front.compareOp),
                    .compareMask = desc.depthStencilState.front.compareMask,
                    .writeMask = desc.depthStencilState.front.writeMask,
                    .reference = desc.depthStencilState.front.reference,
                },
            .back =
                {
                    .failOp = convert_to_vk(desc.depthStencilState.back.failOp),
                    .passOp = convert_to_vk(desc.depthStencilState.back.passOp),
                    .depthFailOp = convert_to_vk(desc.depthStencilState.back.depthFailOp),
                    .compareOp = convert_to_vk(desc.depthStencilState.back.compareOp),
                    .compareMask = desc.depthStencilState.back.compareMask,
                    .writeMask = desc.depthStencilState.back.writeMask,
                    .reference = desc.depthStencilState.back.reference,
                },
            .minDepthBounds = desc.depthStencilState.minDepthBounds,
            .maxDepthBounds = desc.depthStencilState.maxDepthBounds,
        };

        constexpr VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        const VkPipelineDynamicStateCreateInfo dynamicState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = array_size32(dynamicStates),
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

            const auto& debugUtils = m_impl->vkCtx->get_debug_utils_object();
            debugUtils.set_object_name(m_impl->device, newPipeline.pipeline, newPipeline.label);

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

        newPipeline.init(m_impl->interner->c_str(computePass->name));

        const auto failure = [this, &newPipeline, pipelineHandle, computePass, expectedHash]
        {
            destroy_pipeline(*m_impl->vkCtx, newPipeline);
            m_impl->computePipelines.erase(pipelineHandle);
            // We push an invalid variant so we avoid trying to rebuild a failed pipeline every frame
            computePass->variants.emplace_back().hash = expectedHash;
            return h32<compute_pipeline>{};
        };

        vertex_inputs_reflection vertexInputReflection{};

        const shader_compiler_options compilerOptions{m_impl->make_compiler_options()};

        {
            constexpr string_view builtInDefines[] = {"OBLO_PIPELINE_COMPUTE", "OBLO_STAGE_COMPUTE"};

            constexpr auto vkStage = VK_SHADER_STAGE_COMPUTE_BIT;

            const auto& filePath = computePass->shaderSourcePath;

            string_builder builder;

            shader_compiler::result compilerResult;

            const auto shaderModule = m_impl->create_shader_module(vkStage,
                filePath,
                builtInDefines,
                desc.defines,
                make_debug_name(builder, *m_impl->interner, computePass->name, filePath),
                compilerOptions,
                compilerResult);

            if (!shaderModule)
            {
                return failure();
            }

            deque<string_view> sourceFiles;
            compilerResult.get_source_files(sourceFiles);

            for (const auto& include : sourceFiles)
            {
                m_impl->add_watch(include, computePassHandle);
                newPipeline.hasPrintfInclude |= is_printf_include(include);
            }

            newPipeline.shaderModule = shaderModule;

            m_impl->create_reflection(newPipeline, vkStage, compilerResult.get_spirv(), vertexInputReflection);
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

        if (vkCreateComputePipelines(m_impl->device,
                nullptr,
                1,
                &pipelineInfo,
                m_impl->vkCtx->get_allocator().get_allocation_callbacks(),
                &newPipeline.pipeline) == VK_SUCCESS)
        {
            computePass->variants.push_back({.hash = expectedHash, .pipeline = pipelineHandle});

            const auto& debugUtils = m_impl->vkCtx->get_debug_utils_object();
            debugUtils.set_object_name(m_impl->device, newPipeline.pipeline, newPipeline.label);

            return pipelineHandle;
        }

        return failure();
    }

    h32<raytracing_pipeline> pass_manager::get_or_create_pipeline(h32<raytracing_pass> raytracingPassHandle,
        const raytracing_pipeline_initializer& desc)
    {
        auto* const raytracingPass = m_impl->raytracingPasses.try_find(raytracingPassHandle);

        if (!raytracingPass || !m_impl->isRayTracingEnabled)
        {
            return {};
        }

        auto* const vkCtx = m_impl->vkCtx;

        poll_hot_reloading(*m_impl->interner, *vkCtx, *raytracingPass, m_impl->raytracingPipelines);

        usize definesHash = 0;

        for (auto& define : desc.defines)
        {
            definesHash = hash_mix(definesHash, define.hash());
        }

        // The whole initializer should be considered, but we only look at defines for now
        const u64 expectedHash =
            hash_mix(hash_all<std::hash>(raytracingPassHandle.value, desc.maxPipelineRayRecursionDepth), definesHash);

        if (const auto variantIt = std::find_if(raytracingPass->variants.begin(),
                raytracingPass->variants.end(),
                [expectedHash](const raytracing_pass_variant& variant) { return variant.hash == expectedHash; });
            variantIt != raytracingPass->variants.end())
        {
            return variantIt->pipeline;
        }

        const auto restore = m_impl->frameAllocator.make_scoped_restore();

        const auto [pipelineIt, pipelineHandle] = m_impl->raytracingPipelines.emplace();
        OBLO_ASSERT(pipelineHandle);
        auto& newPipeline = *pipelineIt;

        newPipeline.init(m_impl->interner->c_str(raytracingPass->name));

        const auto failure = [this, &newPipeline, pipelineHandle, raytracingPass, expectedHash]
        {
            destroy_pipeline(*m_impl->vkCtx, newPipeline);
            m_impl->raytracingPipelines.erase(pipelineHandle);
            // We push an invalid variant so we avoid trying to rebuild a failed pipeline every frame
            raytracingPass->variants.emplace_back().hash = expectedHash;
            return h32<raytracing_pipeline>{};
        };

        vertex_inputs_reflection vertexInputReflection{};

        const shader_compiler_options compilerOptions{m_impl->make_compiler_options()};

        dynamic_array<VkPipelineShaderStageCreateInfo> stages{&m_impl->frameAllocator};
        stages.reserve(raytracingPass->shadersCount);

        string_view builtInDefines[2] = {"OBLO_PIPELINE_RAYTRACING"};

        string_builder builder;
        deque<string_view> sourceFiles;

        for (u32 currentShaderIndex = 0; currentShaderIndex < raytracingPass->shaderSourcePaths.size();
            ++currentShaderIndex)
        {
            const auto& filePath = raytracingPass->shaderSourcePaths[currentShaderIndex];
            const auto rtStage = raytracingPass->shaderStages[currentShaderIndex];

            const auto vkStage = to_vulkan_stage_bits(rtStage);

            builtInDefines[1] = get_define_from_stage(rtStage);

            shader_compiler::result compilerResult;

            const auto shaderModule = m_impl->create_shader_module(vkStage,
                filePath,
                builtInDefines,
                desc.defines,
                make_debug_name(builder, *m_impl->interner, raytracingPass->name, filePath),
                compilerOptions,
                compilerResult);

            if (!shaderModule)
            {
                return failure();
            }

            sourceFiles.clear();
            compilerResult.get_source_files(sourceFiles);

            for (const auto& include : sourceFiles)
            {
                m_impl->add_watch(include, raytracingPassHandle);
                newPipeline.hasPrintfInclude |= is_printf_include(include);
            }

            newPipeline.shaderModules.push_back(shaderModule);

            m_impl->create_reflection(newPipeline, vkStage, compilerResult.get_spirv(), vertexInputReflection);

            stages.push_back({
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = vkStage,
                .module = shaderModule,
                .pName = "main",
            });
        }

        dynamic_array<VkRayTracingShaderGroupCreateInfoKHR> groups{&m_impl->frameAllocator};
        groups.reserve(raytracingPass->groupsCount);

        groups.push_back({
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = raytracingPass->generation,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
        });

        for (const auto& miss : raytracingPass->miss)
        {
            groups.push_back({
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = miss.shaderIndex,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
            });
        }

        for (const auto& hg : raytracingPass->hitGroups)
        {
            const auto type = hg.type == raytracing_hit_type::triangle
                ? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR
                : VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;

            groups.push_back({
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = type,
                .generalShader = VK_SHADER_UNUSED_KHR,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
            });

            for (const auto& shader : hg.shaders)
            {
                switch (raytracingPass->shaderStages[shader.shaderIndex])
                {
                case raytracing_stage::intersection:
                    groups.back().intersectionShader = shader.shaderIndex;
                    break;
                case raytracing_stage::any_hit:
                    groups.back().anyHitShader = shader.shaderIndex;
                    break;
                case raytracing_stage::closest_hit:
                    groups.back().closestHitShader = shader.shaderIndex;
                    break;
                default:
                    OBLO_ASSERT(false);
                    break;
                }
            }
        }

        if (!m_impl->create_pipeline_layout(newPipeline))
        {
            return failure();
        }

        const VkRayTracingPipelineCreateInfoKHR pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
            .flags = 0,
            .stageCount = u32(stages.size()),
            .pStages = stages.data(),
            .groupCount = u32(groups.size()),
            .pGroups = groups.data(),
            .maxPipelineRayRecursionDepth = desc.maxPipelineRayRecursionDepth,
            .layout = newPipeline.pipelineLayout,
        };

        const auto& vkFn = vkCtx->get_loaded_functions();

        if (vkFn.vkCreateRayTracingPipelinesKHR(m_impl->device,
                nullptr,
                nullptr,
                1u,
                &pipelineInfo,
                vkCtx->get_allocator().get_allocation_callbacks(),
                &newPipeline.pipeline) != VK_SUCCESS)
        {
            return failure();
        }

        const auto& debugUtils = m_impl->vkCtx->get_debug_utils_object();
        debugUtils.set_object_name(m_impl->device, newPipeline.pipeline, newPipeline.label);

        // Create the shader buffer table

        const u32 handleSize = m_impl->rtPipelineProperties.shaderGroupHandleSize;
        const u32 handleSizeAligned =
            round_up_multiple(handleSize, m_impl->rtPipelineProperties.shaderGroupHandleAlignment);

        newPipeline.rayGen = {
            .stride = round_up_multiple(handleSizeAligned, m_impl->rtPipelineProperties.shaderGroupBaseAlignment),
        };

        // Ray-generation is a special case, size has to match the stride
        newPipeline.rayGen.size = newPipeline.rayGen.stride;

        const u32 missCount = u32(raytracingPass->miss.size());

        if (missCount > 0)
        {
            newPipeline.miss = {
                .stride = handleSizeAligned,
                .size = round_up_multiple(missCount * handleSizeAligned,
                    m_impl->rtPipelineProperties.shaderGroupBaseAlignment),
            };
        }

        u32 hitCount = 0;

        for (auto& hg : raytracingPass->hitGroups)
        {
            hitCount += u32(hg.shaders.size());
        }

        newPipeline.hit = {
            .stride = handleSizeAligned,
            .size =
                round_up_multiple(hitCount * handleSizeAligned, m_impl->rtPipelineProperties.shaderGroupBaseAlignment),
        };

        const auto sbtBufferSize =
            u32(newPipeline.rayGen.size + newPipeline.miss.size + newPipeline.hit.size + newPipeline.callable.size);

        auto& allocator = vkCtx->get_allocator();

        if (allocator.create_buffer(
                {
                    .size = narrow_cast<u32>(sbtBufferSize),
                    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                    .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    .debugLabel = "Shader Binding Table",
                },
                &newPipeline.shaderBindingTable) != VK_SUCCESS)
        {
            return failure();
        }

        void* sbtPtr;

        if (allocator.map(newPipeline.shaderBindingTable.allocation, &sbtPtr) != VK_SUCCESS)
        {
            return failure();
        }

        const u32 handleCount = raytracingPass->groupsCount;
        const u32 handleDataSize = handleCount * handleSize;
        dynamic_array<u8> handles{&m_impl->frameAllocator, handleDataSize};

        if (vkFn.vkGetRayTracingShaderGroupHandlesKHR(vkCtx->get_device(),
                newPipeline.pipeline,
                0,
                handleCount,
                handleDataSize,
                handles.data()) != VK_SUCCESS)
        {
            return failure();
        }

        const auto sbtAddress = vkCtx->get_device_address(newPipeline.shaderBindingTable.buffer);

        newPipeline.rayGen.deviceAddress = sbtAddress;
        newPipeline.miss.deviceAddress = newPipeline.rayGen.deviceAddress + newPipeline.rayGen.size;
        newPipeline.hit.deviceAddress = newPipeline.miss.deviceAddress + newPipeline.miss.size;
        newPipeline.callable.deviceAddress = newPipeline.hit.deviceAddress + newPipeline.hit.size;

        struct group_desc
        {
            VkStridedDeviceAddressRegionKHR* group;
            u32 groupHandles;
        };

        const group_desc groupsWithCount[] = {
            {&newPipeline.rayGen, 1},
            {&newPipeline.miss, missCount},
            {
                &newPipeline.hit,
                hitCount,
            },
            {&newPipeline.callable, 0},
        };

        u32 nextHandleIndex = 0;

        for (auto const [group, numHandles] : groupsWithCount)
        {
            if (group->size == 0)
            {
                continue;
            }

            const auto offset = group->deviceAddress - sbtAddress;

            for (u32 i = 0; i < numHandles; ++i)
            {
                const auto dstOffset = offset + i * handleSizeAligned;
                const auto srcOffset = nextHandleIndex * handleSize;

                std::memcpy(static_cast<u8*>(sbtPtr) + dstOffset, handles.data() + srcOffset, handleSize);

                ++nextHandleIndex;
            }
        }

        allocator.unmap(newPipeline.shaderBindingTable.allocation);
        allocator.invalidate_mapped_memory_ranges({&newPipeline.shaderBindingTable.allocation, 1});

        raytracingPass->variants.push_back({.hash = expectedHash, .pipeline = pipelineHandle});
        return pipelineHandle;
    }

    void pass_manager::begin_frame([[maybe_unused]] hptr<gpu::command_buffer> commandBuffer)
    {
        m_impl->frameAllocator.restore_all();

        {
            global_shader_options shaderCompilerConfig{};
            m_impl->shaderCompilerOptions.read(*m_impl->optionsManager, shaderCompilerConfig);

            shader_compiler* const compilers[2] = {m_impl->glslcCompiler.get(), m_impl->glslangCompiler.get()};
            const u32 preferred = u32{shaderCompilerConfig.preferGlslang};

            auto* const chosenCompiler = compilers[preferred] ? compilers[preferred] : compilers[1 - preferred];

            const bool anyChange = m_impl->enableShaderOptimizations != shaderCompilerConfig.optimizeShaders ||
                m_impl->emitDebugInfo != shaderCompilerConfig.emitDebugInfo ||
                m_impl->globallyEnablePrintf != shaderCompilerConfig.enablePrintf ||
                m_impl->emitLineDirectives != shaderCompilerConfig.emitLineDirectives ||
                chosenCompiler != m_impl->shaderCache.get_glsl_compiler();

            if (anyChange)
            {
                m_impl->shaderCache.set_glsl_compiler(chosenCompiler);

                m_impl->enableShaderOptimizations = shaderCompilerConfig.optimizeShaders;
                m_impl->emitDebugInfo = shaderCompilerConfig.emitDebugInfo;
                m_impl->globallyEnablePrintf = shaderCompilerConfig.enablePrintf;
                m_impl->emitLineDirectives = shaderCompilerConfig.emitLineDirectives;

                m_impl->invalidate_all_passes();
            }

            m_impl->shaderCache.set_cache_enabled(shaderCompilerConfig.enableSpirvCache);
        }

        m_impl->propagate_pipeline_invalidation();

#ifdef TRACY_ENABLE
        if (m_impl->enableProfilingThisFrame)
        {
            TracyVkCollect(m_impl->tracyCtx, commandBuffer);
        }

        m_impl->enableProfilingThisFrame = m_impl->enableProfiling && tracy::GetProfiler().IsConnected();
#endif

        m_impl->descriptorSetPool.begin_frame();
        m_impl->texturesDescriptorSetPool.begin_frame();
    }

    void pass_manager::end_frame()
    {
        m_impl->descriptorSetPool.end_frame();
        m_impl->texturesDescriptorSetPool.end_frame();
    }

    void pass_manager::update_global_descriptor_sets()
    {
        const auto debugUtils = m_impl->vkCtx->get_debug_utils_object();

        const std::span textures2DInfo = m_impl->textureRegistry->get_textures2d_info();

        if (!textures2DInfo.empty())
        {
            // Update the Texture 2D descriptor set
            const u32 maxTextureDescriptorId = u32(textures2DInfo.size());

            VkDescriptorSetVariableDescriptorCountAllocateInfoEXT countInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
                .descriptorSetCount = 1,
                .pDescriptorCounts = &maxTextureDescriptorId,
            };

            const VkDescriptorSet texture2dDescriptorSet =
                m_impl->texturesDescriptorSetPool.acquire(m_impl->textures2DSetLayout, &countInfo);

            debugUtils.set_object_name(m_impl->device, texture2dDescriptorSet, "Textures 2D Descriptor Set");

            const VkWriteDescriptorSet descriptorSetWrites[] = {
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = texture2dDescriptorSet,
                    .dstBinding = Textures2DBinding,
                    .dstArrayElement = 0,
                    .descriptorCount = u32(textures2DInfo.size()),
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .pImageInfo = textures2DInfo.data(),
                },
            };

            vkUpdateDescriptorSets(m_impl->device, array_size32(descriptorSetWrites), descriptorSetWrites, 0, nullptr);

            m_impl->currentTextures2DDescriptor = texture2dDescriptorSet;
        }
        else
        {
            m_impl->currentTextures2DDescriptor = nullptr;
        }

        // Sampler descriptors are immutable and require no update
        const VkDescriptorSet samplerDescriptorSet =
            m_impl->texturesDescriptorSetPool.acquire(m_impl->samplersSetLayout);

        debugUtils.set_object_name(m_impl->device, samplerDescriptorSet, "Sampler Descriptor Set");

        m_impl->currentSamplersDescriptor = samplerDescriptorSet;
    }

    bool pass_manager::is_profiling_enabled() const
    {
        return m_impl->enableProfiling;
    }

    void pass_manager::set_profiling_enabled(bool enable)
    {
        m_impl->enableProfiling = enable;
    }

    expected<render_pass_context> pass_manager::begin_render_pass(hptr<gpu::command_buffer> commandBuffer,
        h32<render_pipeline> pipelineHandle,
        const VkRenderingInfo& renderingInfo) const
    {
        const auto* pipeline = m_impl->renderPipelines.try_find(pipelineHandle);

        if (!pipeline)
        {
            return "Render pipeline not found"_err;
        }

        const render_pass_context renderPassContext{
            .commandBuffer = commandBuffer,
            .internalCtx = m_impl->begin_pass(commandBuffer, *pipeline),
            .internalPipeline = pipeline,
        };

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

    void pass_manager::end_render_pass(const render_pass_context& context) const
    {
        vkCmdEndRendering(context.commandBuffer);
        m_impl->end_pass(context.commandBuffer, context.internalCtx);
    }

    expected<compute_pass_context> pass_manager::begin_compute_pass(hptr<gpu::command_buffer> commandBuffer,
        h32<compute_pipeline> pipelineHandle) const
    {
        const auto* pipeline = m_impl->computePipelines.try_find(pipelineHandle);

        if (!pipeline)
        {
            return "Compute pipeline not found"_err;
        }

        const compute_pass_context computePassContext{
            .commandBuffer = commandBuffer,
            .internalCtx = m_impl->begin_pass(commandBuffer, *pipeline),
            .internalPipeline = pipeline,
        };

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

    void pass_manager::end_compute_pass(const compute_pass_context& context) const
    {
        m_impl->end_pass(context.commandBuffer, context.internalCtx);
    }

    expected<raytracing_pass_context> pass_manager::begin_raytracing_pass(hptr<gpu::command_buffer> commandBuffer,
        h32<raytracing_pipeline> pipelineHandle) const
    {
        const auto* pipeline = m_impl->raytracingPipelines.try_find(pipelineHandle);

        if (!pipeline)
        {
            return "Raytracing pipeline not found"_err;
        }

        if (pipeline->requiresTextures2D && m_impl->currentSamplersDescriptor)
        {
            m_impl->gpu->cmd_bind_groups(commandBuffer,
                pipelineHandle,
                TextureSamplerDescriptorSet,
                {&m_impl->currentSamplersDescriptor, 1u});
        }

        if (pipeline->requiresTextures2D && m_impl->currentTextures2DDescriptor)
        {
            m_impl->gpu->cmd_bind_groups(commandBuffer,
                pipelineHandle,
                Textures2DDescriptorSet,
                {&m_impl->currentTextures2DDescriptor, 1u});
        }

        const expected pass = m_impl->gpu->begin_raytracing_pass(commandBuffer, pipeline->pipeline);

        if (!pass)
        {
            return "Failed to begin pass"_err;
        }

        const raytracing_pass_context rtPipelineContext{
            .commandBuffer = commandBuffer,
            .pass = *pass,
            .internalPipeline = pipeline,
        };

        return rtPipelineContext;
    }

    void pass_manager::end_raytracing_pass(const raytracing_pass_context& context) const
    {
        m_impl->gpu->end_raytracing_pass(context.commandBuffer, context.pass);
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

    void pass_manager::push_constants(
        const raytracing_pass_context& ctx, VkShaderStageFlags stages, u32 offset, std::span<const byte> data) const
    {
        vkCmdPushConstants(ctx.commandBuffer,
            ctx.internalPipeline->pipelineLayout,
            stages,
            offset,
            u32(data.size()),
            data.data());
    }

    void pass_manager::push_constants(hptr<gpu::command_buffer> commandBuffer,
        const base_pipeline& pipeline,
        VkShaderStageFlags stages,
        u32 offset,
        std::span<const byte> data) const
    {
        vkCmdPushConstants(commandBuffer, pipeline.pipelineLayout, stages, offset, u32(data.size()), data.data());
    }

    void pass_manager::bind_descriptor_sets(hptr<gpu::command_buffer> commandBuffer,
        VkPipelineBindPoint bindPoint,
        const base_pipeline& pipeline,
        function_ref<bindable_object(const descriptor_binding&)> locateBinding) const
    {
        if (const auto descriptorSetLayout = pipeline.descriptorSetLayout)
        {
            const VkDescriptorSet descriptorSet =
                m_impl->create_descriptor_set(descriptorSetLayout, pipeline, locateBinding);

            vkCmdBindDescriptorSets(commandBuffer,
                bindPoint,
                pipeline.pipelineLayout,
                0,
                1,
                &descriptorSet,
                0,
                nullptr);
        }
    }

    void pass_manager::trace_rays(const raytracing_pass_context& ctx, u32 width, u32 height, u32 depth) const
    {
        m_impl->gpu->cmd_trace_rays(ctx.commandBuffer, ctx.internalCtx) const auto& vkFn =
            m_impl->vkCtx->get_loaded_functions();

        const auto& pipeline = *ctx.internalPipeline;

        vkFn.vkCmdTraceRaysKHR(ctx.commandBuffer,
            &pipeline.rayGen,
            &pipeline.miss,
            &pipeline.hit,
            &pipeline.callable,
            width,
            height,
            depth);
    }

    const base_pipeline* pass_manager::get_base_pipeline(const compute_pipeline* pipeline) const
    {
        return pipeline;
    }

    const base_pipeline* pass_manager::get_base_pipeline(const render_pipeline* pipeline) const
    {
        return pipeline;
    }

    const base_pipeline* pass_manager::get_base_pipeline(const raytracing_pipeline* pipeline) const
    {
        return pipeline;
    }

    string_view pass_manager::get_pass_name(const base_pipeline& pipeline) const
    {
        return pipeline.label;
    }
}