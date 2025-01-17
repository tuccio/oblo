#include <oblo/vulkan/compiler/glslang_compiler.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/uuid_generator.hpp>
#include <oblo/vulkan/compiler/glsl_preprocessor.hpp>
#include <oblo/vulkan/compiler/shader_compiler_result.hpp>

#include <glslang/SPIRV/GlslangToSpv.h>

namespace oblo::vk
{
    namespace
    {
        EShLanguage find_language(const shader_stage stage)
        {
            switch (stage)
            {
            case shader_stage::mesh:
                return EShLangMesh;
            case shader_stage::task:
                return EShLangTask;
            case shader_stage::vertex:
                return EShLangVertex;
            case shader_stage::compute:
                return EShLangCompute;
            case shader_stage::tessellation_control:
                return EShLangTessControl;
            case shader_stage::tessellation_evaluation:
                return EShLangTessEvaluation;
            case shader_stage::geometry:
                return EShLangGeometry;
            case shader_stage::fragment:
                return EShLangFragment;

            case shader_stage::raygen:
                return EShLangRayGen;
            case shader_stage::intersection:
                return EShLangIntersect;
            case shader_stage::closest_hit:
                return EShLangClosestHit;
            case shader_stage::any_hit:
                return EShLangAnyHit;
            case shader_stage::miss:
                return EShLangMiss;
            case shader_stage::callable:
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

        struct glslang_compilation final : shader_compiler::result_core
        {
            enum class state : u8
            {
                idle,
                preprocess_failed,
                preprocess_complete,
                compilation_failed,
                compilation_complete,
                link_failed,
                done,
            };

        public:
            glslang_compilation(allocator& allocator, shader_stage stage) :
                m_language{find_language(stage)}, m_preprocessor{allocator}, m_shader{m_language}, m_error{&allocator}
            {
            }

            bool preprocess_from_file(string_view path, string_view preamble, resolve_include_fn resolveInclude)
            {
                OBLO_ASSERT(m_state == state::idle);

                if (!m_preprocessor.process_from_file(path, preamble, resolveInclude))
                {
                    m_state = state::compilation_failed;
                    return false;
                }

                m_state = state::preprocess_complete;
                return true;
            }

            bool compile(const shader_compiler_options& options)
            {
                OBLO_ASSERT(m_state == state::preprocess_complete);

                const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

                constexpr auto resources = get_resources();

                const auto sourceCode = m_preprocessor.get_code();

                m_shader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_5);
                m_shader.setDebugInfo(options.generateDebugInfo);

                const char* const sourceCodeStrings[] = {sourceCode.data()};
                const int sourceCodeLengths[] = {narrow_cast<int>(sourceCode.size())};

                m_shader.setStringsWithLengths(sourceCodeStrings, sourceCodeLengths, array_size(sourceCodeStrings));

                // We want to process includes ourselves instead
                glslang::TShader::ForbidIncluder forbidIncluder;

                if (!m_shader.parse(&resources, 100, false, messages, forbidIncluder))
                {
                    m_state = state::compilation_failed;
                    return false;
                }

                m_program.addShader(&m_shader);

                if (!m_program.link(messages))
                {
                    m_state = state::link_failed;
                    return false;
                }

                glslang::SpvOptions spvOptions{};
                spvOptions.generateDebugInfo = options.generateDebugInfo;
                spvOptions.emitNonSemanticShaderDebugInfo = options.generateDebugInfo;
                spvOptions.emitNonSemanticShaderDebugSource = options.generateDebugInfo;
                spvOptions.disableOptimizer = !options.codeOptimization;

                auto* const intermediate = m_program.getIntermediate(m_language);

                if (!options.sourceCodeFilePath.empty())
                {
                    intermediate->setSourceFile(options.sourceCodeFilePath.c_str());
                }

                if (options.generateDebugInfo)
                {
                    string_builder include;

                    {
                        include = m_preprocessor.get_main_source_name();
                        const auto mainPath = m_preprocessor.get_main_source_path();
                        intermediate->addIncludeText(include.c_str(), mainPath.data(), mainPath.size());
                    }

                    for (const auto& [k, f] : m_preprocessor.get_includes_map())
                    {
                        include = k;
                        const auto path = m_preprocessor.get_resolved_path(f);
                        intermediate->addIncludeText(include.c_str(), path.data(), path.size());
                    }
                }

                glslang::GlslangToSpv(*intermediate, m_spirv, &spvOptions);

                return true;
            }

            bool has_errors() override
            {
                return m_state == state::preprocess_failed || m_state == state::compilation_failed ||
                    m_state == state::link_failed;
            }

            string_view get_error_message() override
            {
                switch (m_state)
                {
                case state::preprocess_failed:
                    return m_preprocessor.get_error();

                case state::compilation_failed:
                    [[fallthrough]];
                case state::link_failed: {
                    const auto* failedAction = m_state == state::compilation_failed ? "compile" : "link";
                    const auto* infoLog = m_shader.getInfoLog();
                    const auto* infoDebugLog = m_shader.getInfoDebugLog();

                    m_error.format("Failed to {}:\n{}\n{}", failedAction, infoLog, infoDebugLog);
                    return m_error.as<string_view>();
                }

                default:
                    return {};
                }
            }

            string_view get_source_code() override
            {
                OBLO_ASSERT(m_state >= state::preprocess_complete);
                return m_preprocessor.get_code();
            }

            void get_source_files(deque<string_view>& sourceFiles) override
            {
                OBLO_ASSERT(m_state >= state::preprocess_failed);
                m_preprocessor.get_source_files(sourceFiles);
            }

            std::span<const u32> get_spirv() override
            {
                return std::span{m_spirv};
            }

        private:
            state m_state{state::idle};
            EShLanguage m_language;
            glsl_preprocessor m_preprocessor;
            glslang::TShader m_shader;
            glslang::TProgram m_program;
            string_builder m_error;
            std::vector<unsigned int> m_spirv;
        };
    }

    void glslang_compiler::init(const shader_compiler_config& config)
    {
        m_includeDirs.clear();

        for (const auto& p : config.includeDirectories)
        {
            m_includeDirs.emplace_back().append(p);
        }
    }

    shader_compiler::result glslang_compiler::preprocess_from_file(
        allocator& allocator, string_view path, shader_stage stage, string_view preamble)
    {
        auto r = std::make_unique<glslang_compilation>(allocator, stage);

        r->preprocess_from_file(path,
            preamble,
            [this](string_view includePath, string_builder& outPath)
            {
                for (auto& path : m_includeDirs)
                {
                    outPath.clear().append(path).append_path(includePath).append(".glsl");

                    if (filesystem::exists(outPath).value_or(false))
                    {
                        return true;
                    }
                }

                return false;
            });

        return result{std::move(r)};
    }

    shader_compiler::result glslang_compiler::compile(result r, const shader_compiler_options& options)
    {
        get_shader_compiler_result_core_as<glslang_compilation>(r).compile(options);
        return r;
    }
}