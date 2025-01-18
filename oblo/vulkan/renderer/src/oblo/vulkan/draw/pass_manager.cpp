#include <oblo/vulkan/draw/pass_manager.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/iterator/enum_range.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_interner.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/compiler/compiler_module.hpp>
#include <oblo/vulkan/compiler/shader_cache.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/draw/descriptor_set_pool.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/draw/global_shader_options.hpp>
#include <oblo/vulkan/draw/mesh_table.hpp>
#include <oblo/vulkan/draw/raytracing_pass_initializer.hpp>
#include <oblo/vulkan/draw/render_pass_initializer.hpp>
#include <oblo/vulkan/draw/texture_registry.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

#include <efsw/efsw.hpp>
#include <spirv_cross/spirv_cross.hpp>

#ifdef TRACY_ENABLE
    #include <tracy/TracyVulkan.hpp>
#endif

#include <optional>

namespace oblo::vk
{
    namespace
    {
        constexpr u32 TextureSamplerDescriptorSet{1};
        constexpr u32 Textures2DDescriptorSet{2};
        constexpr u32 TexturesSamplerBinding{32};
        constexpr u32 Textures2DBinding{33};

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

        string_view get_define_from_stage(pipeline_stages stage)
        {
            switch (stage)
            {
            case pipeline_stages::mesh:
                return "OBLO_STAGE_MESH";
            case pipeline_stages::vertex:
                return "OBLO_STAGE_VERTEX";
            case pipeline_stages::fragment:
                return "OBLO_STAGE_FRAGMENT";
            default:
                unreachable();
            }
        }

        enum class raytracing_stage : u8
        {
            generation,
            intersection,
            any_hit,
            closest_hit,
            miss,
            callable,
            enum_max,
        };

        VkShaderStageFlagBits to_vulkan_stage_bits(raytracing_stage stage)
        {
            switch (stage)
            {
            case raytracing_stage::generation:
                return VK_SHADER_STAGE_RAYGEN_BIT_KHR;

            case raytracing_stage::intersection:
                return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

            case raytracing_stage::any_hit:
                return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

            case raytracing_stage::closest_hit:
                return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

            case raytracing_stage::miss:
                return VK_SHADER_STAGE_MISS_BIT_KHR;

            case raytracing_stage::callable:
                return VK_SHADER_STAGE_CALLABLE_BIT_KHR;

            default:
                unreachable();
            }
        }

        shader_stage from_vk_shader_stage(VkShaderStageFlagBits vkStage)
        {
            switch (vkStage)
            {
            case VK_SHADER_STAGE_VERTEX_BIT:
                return shader_stage::vertex;
            case VK_SHADER_STAGE_GEOMETRY_BIT:
                return shader_stage::geometry;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                return shader_stage::tessellation_control;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                return shader_stage::tessellation_evaluation;
            case VK_SHADER_STAGE_FRAGMENT_BIT:
                return shader_stage::fragment;
            case VK_SHADER_STAGE_COMPUTE_BIT:
                return shader_stage::compute;
            case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
                return shader_stage::raygen;
            case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
                return shader_stage::intersection;
            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
                return shader_stage::closest_hit;
            case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
                return shader_stage::any_hit;
            case VK_SHADER_STAGE_MISS_BIT_KHR:
                return shader_stage::miss;
            case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
                return shader_stage::callable;
            case VK_SHADER_STAGE_TASK_BIT_EXT:
                return shader_stage::task;
            case VK_SHADER_STAGE_MESH_BIT_EXT:
                return shader_stage::mesh;
            default:
                unreachable();
            }
        }

        string_view get_define_from_stage(raytracing_stage stage)
        {
            switch (stage)
            {
            case raytracing_stage::generation:
                return "OBLO_STAGE_RAYGEN";

            case raytracing_stage::intersection:
                return "OBLO_STAGE_INTERSECTION";

            case raytracing_stage::any_hit:
                return "OBLO_STAGE_ANY_HIT";

            case raytracing_stage::closest_hit:
                return "OBLO_STAGE_CLOSEST_HIT";

            case raytracing_stage::miss:
                return "OBLO_STAGE_MISS_HIT";

            case raytracing_stage::callable:
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
                builder.clear().append(dir).append_path(filename);
                touchedFiles.insert(builder.as<string>());
            }

            std::mutex mutex;
            string_builder builder;
            std::unordered_set<string, hash<string>> touchedFiles;
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

        VkShaderModule create_shader_module_from_spirv(
            VkDevice device, std::span<const unsigned> spirv, const VkAllocationCallbacks* allocationCbs)
        {
            const VkShaderModuleCreateInfo shaderModuleCreateInfo{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = spirv.size() * sizeof(spirv[0]),
                .pCode = spirv.data(),
            };

            VkShaderModule shaderModule;

            if (vkCreateShaderModule(device, &shaderModuleCreateInfo, allocationCbs, &shaderModule) != VK_SUCCESS)
            {
                return nullptr;
            }

            return shaderModule;
        }

        bool is_printf_include(string_view path)
        {
            return path.ends_with("renderer/debug/printf.glsl");
        }
    }

    struct render_pass
    {
        h32<string> name;
        u8 stagesCount{0};

        string shaderSourcePath[MaxPipelineStages];
        pipeline_stages stages[MaxPipelineStages];

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
        dynamic_array<raytracing_stage> shaderStages;

        u32 shadersCount{};
        u32 groupsCount{};

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
        bool hasPrintfInclude{};

        const char* label{};

        u32 lastRecompilationChangeId{};

#ifdef TRACY_ENABLE
        std::unique_ptr<tracy::SourceLocationData> tracyLocation;
#endif

        void init(const char* name)
        {
            label = name;

#ifdef TRACY_ENABLE
            tracyLocation = std::make_unique<tracy::SourceLocationData>();
            tracyLocation->name = name;
#endif
        }
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

    struct raytracing_pipeline : base_pipeline
    {
        dynamic_array<VkShaderModule> shaderModules;
        allocated_buffer shaderBindingTable{};

        VkStridedDeviceAddressRegionKHR rayGen{};
        VkStridedDeviceAddressRegionKHR hit{};
        VkStridedDeviceAddressRegionKHR miss{};
        VkStridedDeviceAddressRegionKHR callable{};
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

        void destroy_pipeline(vulkan_context& ctx, const raytracing_pipeline& variant)
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
                ctx.destroy_deferred(shaderModule, submitIndex);
            }

            if (variant.shaderBindingTable.buffer)
            {
                ctx.destroy_deferred(variant.shaderBindingTable.buffer, submitIndex);
                ctx.destroy_deferred(variant.shaderBindingTable.allocation, submitIndex);
            }
        }

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

        unique_ptr<shader_compiler> glslcCompiler;
        unique_ptr<shader_compiler> glslangCompiler;
        option_proxy_struct<global_shader_options_proxy> shaderCompilerOptions;
        options_manager* optionsManager{};
        shader_cache shaderCache;

        vulkan_context* vkCtx{};
        VkDevice device{};
        h32_flat_pool_dense_map<compute_pass> computePasses;
        h32_flat_pool_dense_map<render_pass> renderPasses;
        h32_flat_pool_dense_map<raytracing_pass> raytracingPasses;
        h32_flat_pool_dense_map<render_pipeline> renderPipelines;
        h32_flat_pool_dense_map<compute_pipeline> computePipelines;
        h32_flat_pool_dense_map<raytracing_pipeline> raytracingPipelines;
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

        string_builder instanceDataDefines;

        watch_listener watchListener;
        std::optional<efsw::FileWatcher> fileWatcher;

        bool enableShaderOptimizations{};
        bool emitDebugInfo{};
        bool enableProfiling{true};
        bool enableProfilingThisFrame{false};
        bool globallyEnablePrintf{false};
        bool isRayTracingEnabled{true};

        std::unordered_map<string, watching_passes, hash<string>> fileToPassList;

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelineProperties{};

#ifdef TRACY_ENABLE
        TracyVkCtx tracyCtx{};
#endif

        void add_watch(string_view file, h32<compute_pass> pass);
        void add_watch(string_view file, h32<render_pass> pass);
        void add_watch(string_view file, h32<raytracing_pass> pass);

        VkShaderModule create_shader_module(VkShaderStageFlagBits vkStage,
            cstring_view filePath,
            std::span<const string_view> builtInDefines,
            std::span<const hashed_string_view> defines,
            string_view debugName,
            const shader_compiler_options& compilerOptions,
            shader_compiler::result& result);

        bool create_pipeline_layout(base_pipeline& newPipeline);

        void create_reflection(base_pipeline& newPipeline,
            VkShaderStageFlagBits vkStage,
            std::span<const u32> spirv,
            vertex_inputs_reflection& vertexInputsReflection);

        VkDescriptorSet create_descriptor_set(VkDescriptorSetLayout descriptorSetLayout,
            const base_pipeline& pipeline,
            std::span<const binding_table* const> bindingTables);

        shader_compiler_options make_compiler_options();

        template <typename Filter = decltype([](auto&&) { return true; })>
        void invalidate_all_passes(Filter&& f = {});

        void propagate_pipeline_invalidation();

        [[nodiscard]] void* begin_pass(VkCommandBuffer commandBuffer, const base_pipeline& pipeline)
        {
            void* scope{};

#ifdef TRACY_ENABLE
            if (enableProfilingThisFrame)
            {
                scope = frameAllocator.allocate(sizeof(tracy::VkCtxScope), alignof(tracy::VkCtxScope));
                new (scope) tracy::VkCtxScope{tracyCtx, pipeline.tracyLocation.get(), commandBuffer, true};
            }
#endif

            vkCtx->begin_debug_label(commandBuffer, pipeline.label);
            return scope;
        }

        void end_pass(VkCommandBuffer commandBuffer, void* ctx)
        {
            vkCtx->end_debug_label(commandBuffer);

#ifdef TRACY_ENABLE
            if (enableProfilingThisFrame)
            {
                std::destroy_at(static_cast<tracy::VkCtxScope*>(ctx));
            }
#endif
        }
    };

    void pass_manager::impl::add_watch(string_view file, h32<compute_pass> pass)
    {
        string_builder abs;
        abs.append(file).make_absolute_path();

        auto& watches = fileToPassList[abs.as<string>()];
        watches.computePasses.emplace(pass);
        fileWatcher->addWatch(filesystem::parent_path(abs.view()).as<std::string>(), &watchListener);
    }

    void pass_manager::impl::add_watch(string_view file, h32<render_pass> pass)
    {
        string_builder abs;
        abs.append(file).make_absolute_path();

        auto& watches = fileToPassList[abs.as<string>()];
        watches.renderPasses.emplace(pass);
        fileWatcher->addWatch(filesystem::parent_path(abs.view()).as<std::string>(), &watchListener);
    }

    void pass_manager::impl::add_watch(string_view file, h32<raytracing_pass> pass)
    {
        string_builder abs;
        abs.append(file).make_absolute_path();

        auto& watches = fileToPassList[abs.as<string>()];
        watches.raytracingPasses.emplace(pass);
        fileWatcher->addWatch(filesystem::parent_path(abs.view()).as<std::string>(), &watchListener);
    }

    VkShaderModule pass_manager::impl::create_shader_module(VkShaderStageFlagBits vkStage,
        cstring_view filePath,
        std::span<const string_view> builtInDefines,
        std::span<const hashed_string_view> userDefines,
        string_view debugName,
        const shader_compiler_options& compilerOptions,
        shader_compiler::result& result)
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

        result = shaderCache.find_or_compile(frameAllocator,
            filePath,
            from_vk_shader_stage(vkStage),
            preambleBuilder.as<string_view>(),
            compilerOptions,
            debugName);

        if (result.has_errors())
        {
            log::error("Shader compilation failed for {}\n{}", debugName, result.get_error_message());
            return nullptr;
        }

        return create_shader_module_from_spirv(device,
            result.get_spirv(),
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

        // Merge the same resources that belong to different stages together, but we need to keep the order intact
        for (u32 current = 0; current + 1 < newPipeline.resources.size();)
        {
            const u32 next = current + 1;

            if (shader_resource_sorting::from(newPipeline.resources[current]) ==
                shader_resource_sorting::from(newPipeline.resources[next]))
            {
                newPipeline.resources[current].stageFlags |= newPipeline.resources[next].stageFlags;

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

            newPipeline.resources.push_back({
                .name = name,
                .location = location,
                .binding = binding,
                .kind = resource_kind::storage_buffer,
                .stageFlags = VkShaderStageFlags(vkStage),
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
                .stageFlags = VkShaderStageFlags(vkStage),
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
                .stageFlags = VkShaderStageFlags(vkStage),
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
                .stageFlags = VkShaderStageFlags(vkStage),
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
                .stageFlags = VkShaderStageFlags(vkStage),
            });

            collisionChecker.check(binding, image.name.c_str(), newPipeline.label);
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
                .stageFlags = VkShaderStageFlags(vkStage),
            });

            collisionChecker.check(binding, accelerationStructure.name.c_str(), newPipeline.label);
        }
    }

    shader_compiler_options pass_manager::impl::make_compiler_options()
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
        std::lock_guard lock{watchListener.mutex};

        for (const auto& f : watchListener.touchedFiles)
        {
            const auto it = fileToPassList.find(f);

            if (it == fileToPassList.end())
            {
                continue;
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

        watchListener.touchedFiles.clear();
    }

    VkDescriptorSet pass_manager::impl::create_descriptor_set(VkDescriptorSetLayout descriptorSetLayout,
        const base_pipeline& pipeline,
        std::span<const binding_table* const> bindingTables)
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
                    case bindable_object_kind::acceleration_structure:
                        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                        {
                            writeAccelerationStructureToDescriptorSet(binding, bindableObject->accelerationStructure);
                        }
                        else
                        {
                            log::debug("[{}] A binding for {} was found, but it's not an acceleration structure as "
                                       "expected",
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

        auto* compilerModule = module_manager::get().find<compiler_module>();
        m_impl->glslcCompiler = compilerModule->make_glslc_compiler("./glslc");
        m_impl->glslangCompiler = compilerModule->make_glslang_compiler();

        auto* const optionsModule = module_manager::get().find<options_module>();
        m_impl->optionsManager = &optionsModule->manager();
        m_impl->shaderCompilerOptions.init(*m_impl->optionsManager);

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

        m_impl->rtPipelineProperties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

        VkPhysicalDeviceProperties2 physicalProp2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &m_impl->rtPipelineProperties,
        };

        vkGetPhysicalDeviceProperties2(m_impl->vkCtx->get_physical_device(), &physicalProp2);

#ifdef TRACY_ENABLE
        m_impl->tracyCtx = tracy::CreateVkContext(m_impl->vkCtx->get_physical_device(),
            m_impl->vkCtx->get_device(),
            PFN_vkResetQueryPoolEXT(vkGetInstanceProcAddr(m_impl->vkCtx->get_instance(), "vkResetQueryPoolEXT")),
            PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(
                vkGetInstanceProcAddr(m_impl->vkCtx->get_instance(), "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT")),
            PFN_vkGetCalibratedTimestampsEXT(
                vkGetInstanceProcAddr(m_impl->vkCtx->get_instance(), "vkGetCalibratedTimestampsEXT")));
#endif
    }

    void pass_manager::shutdown(vulkan_context& vkContext)
    {
        if (!m_impl)
        {
            return;
        }

#ifdef TRACY_ENABLE
        if (m_impl->tracyCtx)
        {
            tracy::DestroyVkContext(m_impl->tracyCtx);
            m_impl->tracyCtx = {};
        }
#endif

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

            for (const auto& raytracingPipeline : m_impl->raytracingPipelines.values())
            {
                destroy_pipeline(*m_impl->vkCtx, raytracingPipeline);
            }
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

    namespace
    {
        raytracing_stage deduce_rt_shader_stage(string_view p)
        {
            auto&& ext = filesystem::extension(p);

            if (ext == ".rgen")
            {
                return raytracing_stage::generation;
            }

            if (ext == ".rint")
            {
                return raytracing_stage::intersection;
            }

            if (ext == ".rahit")
            {
                return raytracing_stage::any_hit;
            }

            if (ext == ".rchit")
            {
                return raytracing_stage::closest_hit;
            }

            if (ext == ".rmiss")
            {
                return raytracing_stage::miss;
            }

            if (ext == ".rcall")
            {
                return raytracing_stage::callable;
            }

            unreachable();
        }
    }

    h32<raytracing_pass> pass_manager::register_raytracing_pass(const raytracing_pass_initializer& desc)
    {
        const auto [it, handle] = m_impl->raytracingPasses.emplace();
        OBLO_ASSERT(handle);

        auto& renderPass = *it;

        renderPass.name = m_impl->interner->get_or_add(desc.name);

        const auto appendShader = [&](string_view source)
        {
            if (!source.empty())
            {
                const auto size = renderPass.shaderSourcePaths.size();
                renderPass.shaderSourcePaths.push_back(source.as<string>());
                renderPass.shaderStages.push_back(deduce_rt_shader_stage(source));

                m_impl->add_watch(source, handle);

                return u32(size);
            }

            return ~u32{};
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
        renderPass.groupsCount = u32{renderPass.generation != VK_SHADER_UNUSED_KHR} + u32(renderPass.miss.size()) +
            u32(desc.hitGroups.size());

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

        newPipeline.init(m_impl->interner->c_str(renderPass->name));

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

        vertex_inputs_reflection vertexInputReflection{};

        const shader_compiler_options compilerOptions{m_impl->make_compiler_options()};

        string_view builtInDefines[2]{"OBLO_PIPELINE_RENDER"};

        string_builder builder;

        deque<string_view> sourceFiles;

        for (u8 stageIndex = 0; stageIndex < renderPass->stagesCount; ++stageIndex)
        {
            const auto pipelineStage = renderPass->stages[stageIndex];
            const auto vkStage = to_vulkan_stage_bits(pipelineStage);

            const auto& filePath = renderPass->shaderSourcePath[stageIndex];

            builtInDefines[1] = get_define_from_stage(pipelineStage);

            shader_compiler::result compilerResult;

            const auto shaderModule = m_impl->create_shader_module(vkStage,
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

            newPipeline.shaderModules[stageIndex] = shaderModule;

            stageCreateInfo[actualStagesCount] = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = vkStage,
                .module = shaderModule,
                .pName = "main",
            };

            m_impl->create_reflection(newPipeline, vkStage, compilerResult.get_spirv(), vertexInputReflection);

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
            .topology = desc.primitiveTopology,
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

    void pass_manager::begin_frame([[maybe_unused]] VkCommandBuffer commandBuffer)
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
                chosenCompiler != m_impl->shaderCache.get_glsl_compiler();

            if (anyChange)
            {
                m_impl->shaderCache.set_glsl_compiler(chosenCompiler);

                m_impl->enableShaderOptimizations = shaderCompilerConfig.optimizeShaders;
                m_impl->emitDebugInfo = shaderCompilerConfig.emitDebugInfo;
                m_impl->globallyEnablePrintf = shaderCompilerConfig.enablePrintf;

                m_impl->invalidate_all_passes();
            }

            m_impl->shaderCache.set_cache_enabled(shaderCompilerConfig.enableSpirvCache);
        }

        m_impl->descriptorSetPool.begin_frame();
        m_impl->texturesDescriptorSetPool.begin_frame();

        const auto debugUtils = m_impl->vkCtx->get_debug_utils_object();

        const std::span textures2DInfo = m_impl->textureRegistry->get_textures2d_info();

        if (textures2DInfo.empty())
        {
            m_impl->currentTextures2DDescriptor = {};
            return;
        }

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

        vkUpdateDescriptorSets(m_impl->device, array_size(descriptorSetWrites), descriptorSetWrites, 0, nullptr);

        // Sampler descriptors are immutable and require no update
        const VkDescriptorSet samplerDescriptorSet =
            m_impl->texturesDescriptorSetPool.acquire(m_impl->samplersSetLayout);

        debugUtils.set_object_name(m_impl->device, samplerDescriptorSet, "Sampler Descriptor Set");

        m_impl->currentTextures2DDescriptor = texture2dDescriptorSet;
        m_impl->currentSamplersDescriptor = samplerDescriptorSet;

        m_impl->propagate_pipeline_invalidation();

#ifdef TRACY_ENABLE
        if (m_impl->enableProfilingThisFrame)
        {
            TracyVkCollect(m_impl->tracyCtx, commandBuffer);
        }

        m_impl->enableProfilingThisFrame = m_impl->enableProfiling && tracy::GetProfiler().IsConnected();
#endif
    }

    void pass_manager::end_frame()
    {
        m_impl->descriptorSetPool.end_frame();
        m_impl->texturesDescriptorSetPool.end_frame();
    }

    void pass_manager::update_instance_data_defines(string_view defines)
    {
        m_impl->instanceDataDefines = defines;

        // Invalidate all passes as well, to trigger recompilation of shaders
        m_impl->invalidate_all_passes();
    }

    bool pass_manager::is_profiling_enabled() const
    {
        return m_impl->enableProfiling;
    }

    void pass_manager::set_profiling_enabled(bool enable)
    {
        m_impl->enableProfiling = enable;
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

    void pass_manager::end_render_pass(const render_pass_context& context)
    {
        vkCmdEndRendering(context.commandBuffer);
        m_impl->end_pass(context.commandBuffer, context.internalCtx);
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

    void pass_manager::end_compute_pass(const compute_pass_context& context)
    {
        m_impl->end_pass(context.commandBuffer, context.internalCtx);
    }

    expected<raytracing_pass_context> pass_manager::begin_raytracing_pass(VkCommandBuffer commandBuffer,
        h32<raytracing_pipeline> pipelineHandle) const
    {
        const auto* pipeline = m_impl->raytracingPipelines.try_find(pipelineHandle);

        if (!pipeline)
        {
            return unspecified_error;
        }

        const raytracing_pass_context rtPipelineContext{
            .commandBuffer = commandBuffer,
            .internalCtx = m_impl->begin_pass(commandBuffer, *pipeline),
            .internalPipeline = pipeline,
        };

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline);

        if (pipeline->requiresTextures2D && m_impl->currentSamplersDescriptor)
        {
            vkCmdBindDescriptorSets(commandBuffer,
                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
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
                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                pipeline->pipelineLayout,
                Textures2DDescriptorSet,
                1,
                &m_impl->currentTextures2DDescriptor,
                0,
                nullptr);
        }

        return rtPipelineContext;
    }

    void pass_manager::end_raytracing_pass(const raytracing_pass_context& context)
    {
        m_impl->end_pass(context.commandBuffer, context.internalCtx);
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

    void pass_manager::bind_descriptor_sets(const raytracing_pass_context& ctx,
        std::span<const binding_table* const> bindingTables) const
    {
        auto* const pipeline = ctx.internalPipeline;

        if (const auto descriptorSetLayout = pipeline->descriptorSetLayout)
        {
            const VkDescriptorSet descriptorSet =
                m_impl->create_descriptor_set(descriptorSetLayout, *pipeline, bindingTables);

            vkCmdBindDescriptorSets(ctx.commandBuffer,
                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                pipeline->pipelineLayout,
                0,
                1,
                &descriptorSet,
                0,
                nullptr);
        }
    }

    void pass_manager::trace_rays(const raytracing_pass_context& ctx, u32 width, u32 height, u32 depth) const
    {
        const auto& vkFn = m_impl->vkCtx->get_loaded_functions();

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
}