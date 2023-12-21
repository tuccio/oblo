#include <oblo/vulkan/shader_compiler.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/file_utility.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/log.hpp>

#include <glslang/SPIRV/GlslangToSpv.h>

#include <deque>
#include <mutex>
#include <string_view>

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
            default:
                return EShLangCount;
            }
        }

        constexpr TBuiltInResource get_resources()
        {
            TBuiltInResource resources{};
            resources.maxLights = 32;
            resources.maxClipPlanes = 6;
            resources.maxTextureUnits = 32;
            resources.maxTextureCoords = 32;
            resources.maxVertexAttribs = 64;
            resources.maxVertexUniformComponents = 4096;
            resources.maxVaryingFloats = 64;
            resources.maxVertexTextureImageUnits = 32;
            resources.maxCombinedTextureImageUnits = 80;
            resources.maxTextureImageUnits = 32;
            resources.maxFragmentUniformComponents = 4096;
            resources.maxDrawBuffers = 32;
            resources.maxVertexUniformVectors = 128;
            resources.maxVaryingVectors = 8;
            resources.maxFragmentUniformVectors = 16;
            resources.maxVertexOutputVectors = 16;
            resources.maxFragmentInputVectors = 15;
            resources.minProgramTexelOffset = -8;
            resources.maxProgramTexelOffset = 7;
            resources.maxClipDistances = 8;
            resources.maxComputeWorkGroupCountX = 65535;
            resources.maxComputeWorkGroupCountY = 65535;
            resources.maxComputeWorkGroupCountZ = 65535;
            resources.maxComputeWorkGroupSizeX = 1024;
            resources.maxComputeWorkGroupSizeY = 1024;
            resources.maxComputeWorkGroupSizeZ = 64;
            resources.maxComputeUniformComponents = 1024;
            resources.maxComputeTextureImageUnits = 16;
            resources.maxComputeImageUniforms = 8;
            resources.maxComputeAtomicCounters = 8;
            resources.maxComputeAtomicCounterBuffers = 1;
            resources.maxVaryingComponents = 60;
            resources.maxVertexOutputComponents = 64;
            resources.maxGeometryInputComponents = 64;
            resources.maxGeometryOutputComponents = 128;
            resources.maxFragmentInputComponents = 128;
            resources.maxImageUnits = 8;
            resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
            resources.maxCombinedShaderOutputResources = 8;
            resources.maxImageSamples = 0;
            resources.maxVertexImageUniforms = 0;
            resources.maxTessControlImageUniforms = 0;
            resources.maxTessEvaluationImageUniforms = 0;
            resources.maxGeometryImageUniforms = 0;
            resources.maxFragmentImageUniforms = 8;
            resources.maxCombinedImageUniforms = 8;
            resources.maxGeometryTextureImageUnits = 16;
            resources.maxGeometryOutputVertices = 256;
            resources.maxGeometryTotalOutputComponents = 1024;
            resources.maxGeometryUniformComponents = 1024;
            resources.maxGeometryVaryingComponents = 64;
            resources.maxTessControlInputComponents = 128;
            resources.maxTessControlOutputComponents = 128;
            resources.maxTessControlTextureImageUnits = 16;
            resources.maxTessControlUniformComponents = 1024;
            resources.maxTessControlTotalOutputComponents = 4096;
            resources.maxTessEvaluationInputComponents = 128;
            resources.maxTessEvaluationOutputComponents = 128;
            resources.maxTessEvaluationTextureImageUnits = 16;
            resources.maxTessEvaluationUniformComponents = 1024;
            resources.maxTessPatchComponents = 120;
            resources.maxPatchVertices = 32;
            resources.maxTessGenLevel = 64;
            resources.maxViewports = 16;
            resources.maxVertexAtomicCounters = 0;
            resources.maxTessControlAtomicCounters = 0;
            resources.maxTessEvaluationAtomicCounters = 0;
            resources.maxGeometryAtomicCounters = 0;
            resources.maxFragmentAtomicCounters = 8;
            resources.maxCombinedAtomicCounters = 8;
            resources.maxAtomicCounterBindings = 1;
            resources.maxVertexAtomicCounterBuffers = 0;
            resources.maxTessControlAtomicCounterBuffers = 0;
            resources.maxTessEvaluationAtomicCounterBuffers = 0;
            resources.maxGeometryAtomicCounterBuffers = 0;
            resources.maxFragmentAtomicCounterBuffers = 1;
            resources.maxCombinedAtomicCounterBuffers = 1;
            resources.maxAtomicCounterBufferSize = 16384;
            resources.maxTransformFeedbackBuffers = 4;
            resources.maxTransformFeedbackInterleavedComponents = 64;
            resources.maxCullDistances = 8;
            resources.maxCombinedClipAndCullDistances = 8;
            resources.maxSamples = 4;
            resources.maxMeshOutputVerticesNV = 256;
            resources.maxMeshOutputPrimitivesNV = 512;
            resources.maxMeshWorkGroupSizeX_NV = 32;
            resources.maxMeshWorkGroupSizeY_NV = 1;
            resources.maxMeshWorkGroupSizeZ_NV = 1;
            resources.maxTaskWorkGroupSizeX_NV = 32;
            resources.maxTaskWorkGroupSizeY_NV = 1;
            resources.maxTaskWorkGroupSizeZ_NV = 1;
            resources.maxMeshViewCountNV = 4;
            resources.limits.nonInductiveForLoops = 1;
            resources.limits.whileLoops = 1;
            resources.limits.doWhileLoops = 1;
            resources.limits.generalUniformIndexing = 1;
            resources.limits.generalAttributeMatrixVectorIndexing = 1;
            resources.limits.generalVaryingIndexing = 1;
            resources.limits.generalSamplerIndexing = 1;
            resources.limits.generalVariableIndexing = 1;
            resources.limits.generalConstantMatrixVectorIndexing = 1;
            return resources;
        }

        struct glslang_includer final : glslang::TShader::Includer
        {
        public:
            explicit glslang_includer(include_handler& handler) : m_handler{handler} {}

            IncludeResult* includeSystem(
                const char* headerName, const char* /*includerName*/, size_t /*inclusionDepth*/) override
            {
                if (!m_handler.resolve(headerName, m_pathBuffer))
                {
                    return nullptr;
                }

                const std::span data = load_text_file_into_memory(m_allocator, m_pathBuffer);

                auto& result = m_includeResults.emplace_back(m_pathBuffer.string(), data.data(), data.size(), nullptr);
                return &result;
            }

            IncludeResult* includeLocal(
                const char* /*headerName*/, const char* /*includerName*/, size_t /*inclusionDepth*/) override
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

    bool compile_glsl_to_spirv(std::string_view debugName,
        std::string_view sourceCode,
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
        spvOptions.disableOptimizer = !options.codeOptimization;

        outSpirv.clear();

        auto* const intermediate = program.getIntermediate(language);
        glslang::GlslangToSpv(*intermediate, outSpirv);

        return true;
    }

    VkShaderModule create_shader_module_from_spirv(VkDevice device, std::span<const unsigned> spirv)
    {
        const VkShaderModuleCreateInfo shaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * sizeof(spirv[0]),
            .pCode = spirv.data(),
        };

        VkShaderModule shaderModule;

        if (vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            return nullptr;
        }

        return shaderModule;
    }

    VkShaderModule create_shader_module_from_glsl_file(frame_allocator& allocator,
        VkDevice device,
        VkShaderStageFlagBits stage,
        std::string_view filePath,
        const options& options)
    {
        std::vector<unsigned> spirv;
        const auto sourceSpan = load_text_file_into_memory(allocator, filePath);

        if (!shader_compiler::compile_glsl_to_spirv(filePath,
                {sourceSpan.data(), sourceSpan.size()},
                stage,
                spirv,
                options))
        {
            return VK_NULL_HANDLE;
        }

        return shader_compiler::create_shader_module_from_spirv(device, spirv);
    }
}