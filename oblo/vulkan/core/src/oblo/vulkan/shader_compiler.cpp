#include <oblo/vulkan/shader_compiler.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/file_utility.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/log.hpp>

#include <glslang/SPIRV/GlslangToSpv.h>

#include <deque>
#include <mutex>

namespace oblo::vk::shader_compiler
{
    namespace
    {
        EShLanguage find_language(const VkShaderStageFlagBits stage)
        {
            switch (stage)
            {
            case VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT:
                return EShLangVertex;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                return EShLangTessControl;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                return EShLangTessEvaluation;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_GEOMETRY_BIT:
                return EShLangGeometry;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT:
                return EShLangFragment;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT:
                return EShLangCompute;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_MESH_BIT_EXT:
                return EShLangMesh;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_TASK_BIT_EXT:
                return EShLangTask;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR:
                return EShLangRayGen;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
                return EShLangIntersect;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
                return EShLangClosestHit;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
                return EShLangAnyHit;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_MISS_BIT_KHR:
                return EShLangMiss;
            case VkShaderStageFlagBits::VK_SHADER_STAGE_CALLABLE_BIT_KHR:
                return EShLangCallable;
            default:
                OBLO_ASSERT(false);
                return EShLangCount;
            }
        }

        constexpr TBuiltInResource get_resources()
        {
            const TBuiltInResource resources{
                .maxLights = 32,
                .maxClipPlanes = 6,
                .maxTextureUnits = 32,
                .maxTextureCoords = 32,
                .maxVertexAttribs = 64,
                .maxVertexUniformComponents = 4096,
                .maxVaryingFloats = 64,
                .maxVertexTextureImageUnits = 32,
                .maxCombinedTextureImageUnits = 80,
                .maxTextureImageUnits = 32,
                .maxFragmentUniformComponents = 4096,
                .maxDrawBuffers = 32,
                .maxVertexUniformVectors = 128,
                .maxVaryingVectors = 8,
                .maxFragmentUniformVectors = 16,
                .maxVertexOutputVectors = 16,
                .maxFragmentInputVectors = 15,
                .minProgramTexelOffset = -8,
                .maxProgramTexelOffset = 7,
                .maxClipDistances = 8,
                .maxComputeWorkGroupCountX = 65535,
                .maxComputeWorkGroupCountY = 65535,
                .maxComputeWorkGroupCountZ = 65535,
                .maxComputeWorkGroupSizeX = 1024,
                .maxComputeWorkGroupSizeY = 1024,
                .maxComputeWorkGroupSizeZ = 64,
                .maxComputeUniformComponents = 1024,
                .maxComputeTextureImageUnits = 16,
                .maxComputeImageUniforms = 8,
                .maxComputeAtomicCounters = 8,
                .maxComputeAtomicCounterBuffers = 1,
                .maxVaryingComponents = 60,
                .maxVertexOutputComponents = 64,
                .maxGeometryInputComponents = 64,
                .maxGeometryOutputComponents = 128,
                .maxFragmentInputComponents = 128,
                .maxImageUnits = 8,
                .maxCombinedImageUnitsAndFragmentOutputs = 8,
                .maxCombinedShaderOutputResources = 8,
                .maxImageSamples = 0,
                .maxVertexImageUniforms = 0,
                .maxTessControlImageUniforms = 0,
                .maxTessEvaluationImageUniforms = 0,
                .maxGeometryImageUniforms = 0,
                .maxFragmentImageUniforms = 8,
                .maxCombinedImageUniforms = 8,
                .maxGeometryTextureImageUnits = 16,
                .maxGeometryOutputVertices = 256,
                .maxGeometryTotalOutputComponents = 1024,
                .maxGeometryUniformComponents = 1024,
                .maxGeometryVaryingComponents = 64,
                .maxTessControlInputComponents = 128,
                .maxTessControlOutputComponents = 128,
                .maxTessControlTextureImageUnits = 16,
                .maxTessControlUniformComponents = 1024,
                .maxTessControlTotalOutputComponents = 4096,
                .maxTessEvaluationInputComponents = 128,
                .maxTessEvaluationOutputComponents = 128,
                .maxTessEvaluationTextureImageUnits = 16,
                .maxTessEvaluationUniformComponents = 1024,
                .maxTessPatchComponents = 120,
                .maxPatchVertices = 32,
                .maxTessGenLevel = 64,
                .maxViewports = 16,
                .maxVertexAtomicCounters = 0,
                .maxTessControlAtomicCounters = 0,
                .maxTessEvaluationAtomicCounters = 0,
                .maxGeometryAtomicCounters = 0,
                .maxFragmentAtomicCounters = 8,
                .maxCombinedAtomicCounters = 8,
                .maxAtomicCounterBindings = 1,
                .maxVertexAtomicCounterBuffers = 0,
                .maxTessControlAtomicCounterBuffers = 0,
                .maxTessEvaluationAtomicCounterBuffers = 0,
                .maxGeometryAtomicCounterBuffers = 0,
                .maxFragmentAtomicCounterBuffers = 1,
                .maxCombinedAtomicCounterBuffers = 1,
                .maxAtomicCounterBufferSize = 16384,
                .maxTransformFeedbackBuffers = 4,
                .maxTransformFeedbackInterleavedComponents = 64,
                .maxCullDistances = 8,
                .maxCombinedClipAndCullDistances = 8,
                .maxSamples = 4,
                .maxMeshOutputVerticesNV = 256,
                .maxMeshOutputPrimitivesNV = 512,
                .maxMeshWorkGroupSizeX_NV = 32,
                .maxMeshWorkGroupSizeY_NV = 1,
                .maxMeshWorkGroupSizeZ_NV = 1,
                .maxTaskWorkGroupSizeX_NV = 32,
                .maxTaskWorkGroupSizeY_NV = 1,
                .maxTaskWorkGroupSizeZ_NV = 1,
                .maxMeshViewCountNV = 4,
                .maxMeshOutputVerticesEXT = 256,
                .maxMeshOutputPrimitivesEXT = 256,
                .maxMeshWorkGroupSizeX_EXT = 128,
                .maxMeshWorkGroupSizeY_EXT = 128,
                .maxMeshWorkGroupSizeZ_EXT = 128,
                .maxTaskWorkGroupSizeX_EXT = 128,
                .maxTaskWorkGroupSizeY_EXT = 128,
                .maxTaskWorkGroupSizeZ_EXT = 128,
                .maxMeshViewCountEXT = 4,
                .maxDualSourceDrawBuffersEXT = 1,
                .limits =
                    {
                        .nonInductiveForLoops = 1,
                        .whileLoops = 1,
                        .doWhileLoops = 1,
                        .generalUniformIndexing = 1,
                        .generalAttributeMatrixVectorIndexing = 1,
                        .generalVaryingIndexing = 1,
                        .generalSamplerIndexing = 1,
                        .generalVariableIndexing = 1,
                        .generalConstantMatrixVectorIndexing = 1,
                    },
            };

            return resources;
        }

        struct glslang_includer final : glslang::TShader::Includer
        {
        public:
            explicit glslang_includer(include_handler& handler) : m_handler{handler} {}

            IncludeResult* includeSystem(const char* headerName,
                [[maybe_unused]] const char* includerName,
                [[maybe_unused]] size_t inclusionDepth) override
            {
                if (!m_handler.resolve(headerName, m_pathBuffer))
                {
                    return nullptr;
                }

                const auto data = load_text_file_into_memory(m_allocator, m_pathBuffer);

                if (!data)
                {
                    return nullptr;
                }

                auto& result =
                    m_includeResults.emplace_back(m_pathBuffer.string(), data->data(), data->size(), nullptr);

                return &result;
            }

            IncludeResult* includeLocal([[maybe_unused]] const char* headerName,
                [[maybe_unused]] const char* includerName,
                [[maybe_unused]] size_t inclusionDepth) override
            {
                return nullptr;
            }

            void releaseInclude(IncludeResult*) override {}

        private:
            include_handler& m_handler;
            frame_allocator& m_allocator{m_handler.get_allocator()};
            std::deque<IncludeResult> m_includeResults;
            std::filesystem::path m_pathBuffer;
        };

        std::mutex s_initMutex;
        int s_counter{0};
    }

    void init()
    {
        std::scoped_lock lock{s_initMutex};

        if (s_counter++ == 0)
        {
            glslang::InitializeProcess();
        }
    }

    void shutdown()
    {
        std::scoped_lock lock{s_initMutex};

        if (--s_counter == 0)
        {
            glslang::FinalizeProcess();
        }
    }

    bool compile_glsl_to_spirv(string_view debugName,
        string_view sourceCode,
        VkShaderStageFlagBits stage,
        std::vector<unsigned>& outSpirv,
        const options& options)
    {
        const auto language = find_language(stage);
        glslang::TShader shader{language};
        glslang::TProgram program;

        const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

        const char* const sources[] = {sourceCode.data()};
        const int sourceLengths[] = {int(sourceCode.size())};
        shader.setStringsWithLengths(sources, sourceLengths, 1);

        constexpr auto resources = get_resources();

        glslang::TShader::ForbidIncluder forbidIncluder;
        std::optional<glslang_includer> userIncluder;

        glslang::TShader::Includer* includer{&forbidIncluder};

        if (options.includeHandler)
        {
            includer = &userIncluder.emplace(*options.includeHandler);
        }

        shader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_5);

        if (!shader.parse(&resources, 100, false, messages, *includer))
        {
            const auto* infoLog = shader.getInfoLog();
            const auto* infoDebugLog = shader.getInfoDebugLog();
            log::error("Failed to compile shader '{}'\n{}\n{}", debugName, infoLog, infoDebugLog);
            return false;
        }

        program.addShader(&shader);

        if (!program.link(messages))
        {
            const auto* infoLog = shader.getInfoLog();
            const auto* infoDebugLog = shader.getInfoDebugLog();
            log::error("Failed to link shader '{}'\n{}\n{}", debugName, infoLog, infoDebugLog);
            return false;
        }

        glslang::SpvOptions spvOptions{};
        spvOptions.generateDebugInfo = options.generateDebugInfo;
        spvOptions.disableOptimizer = !options.codeOptimization;

        outSpirv.clear();

        auto* const intermediate = program.getIntermediate(language);
        glslang::GlslangToSpv(*intermediate, outSpirv, &spvOptions);

        return true;
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

    VkShaderModule create_shader_module_from_glsl_file(frame_allocator& allocator,
        VkDevice device,
        VkShaderStageFlagBits stage,
        string_view filePath,
        const VkAllocationCallbacks* allocationCbs,
        const options& options)
    {
        std::vector<unsigned> spirv;
        const auto sourceSpan = load_text_file_into_memory(allocator, filePath.as<std::string_view>());

        if (!sourceSpan)
        {
            return nullptr;
        }

        if (!shader_compiler::compile_glsl_to_spirv(filePath,
                {sourceSpan->data(), sourceSpan->size()},
                stage,
                spirv,
                options))
        {
            return VK_NULL_HANDLE;
        }

        return shader_compiler::create_shader_module_from_spirv(device, spirv, allocationCbs);
    }
}