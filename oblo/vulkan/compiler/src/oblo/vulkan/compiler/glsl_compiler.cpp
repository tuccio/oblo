#include <oblo/vulkan/compiler/glsl_compiler.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/lifetime.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/platform/process.hpp>
#include <oblo/core/unreachable.hpp>
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

        struct glsl_compilation_base : shader_compiler::result_core
        {
        public:
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

            glsl_compilation_base(allocator& allocator) : m_preprocessor{allocator}, m_error{&allocator} {}

            bool preprocess_from_file(
                cstring_view path, string_view preamble, std::span<const string_builder> includeDirs)
            {
                OBLO_ASSERT(m_state == state::idle);

                if (!m_preprocessor.process_from_file(path,
                        preamble,
                        [includeDirs](string_view includePath, string_builder& outPath)
                        {
                            for (auto& path : includeDirs)
                            {
                                outPath.clear().append(path).append_path(includePath).append(".glsl");

                                if (filesystem::exists(outPath).value_or(false))
                                {
                                    return true;
                                }
                            }

                            return false;
                        }))
                {
                    m_state = state::compilation_failed;
                    return false;
                }

                m_state = state::preprocess_complete;
                return true;
            }

            bool has_errors() final
            {
                return m_state == state::preprocess_failed || m_state == state::compilation_failed ||
                    m_state == state::link_failed;
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

        protected:
            state m_state{state::idle};
            glsl_preprocessor m_preprocessor;
            string_builder m_error;
        };

        struct glslang_compilation final : glsl_compilation_base
        {
        public:
            glslang_compilation(allocator& allocator, shader_stage stage) :
                glsl_compilation_base{allocator}, m_language{find_language(stage)}, m_shader{m_language}
            {
            }

            bool compile(const shader_compiler_options& options)
            {
                OBLO_ASSERT(m_state == state::preprocess_complete);

                const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

                constexpr auto resources = get_resources();

                const auto sourceCode = m_preprocessor.get_code();

                m_shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
                m_shader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_5);

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

                constexpr bool forceDisableDebugInfo = true;

                if constexpr (forceDisableDebugInfo)
                {
                    // This is failing to generate info with buffer reference types
                    // https://github.com/KhronosGroup/glslang/pull/3617 might fix it
                    spvOptions.generateDebugInfo = false;
                    spvOptions.emitNonSemanticShaderDebugInfo = false;
                    spvOptions.emitNonSemanticShaderDebugSource = false;
                }
                else
                {
                    m_shader.setDebugInfo(options.generateDebugInfo);
                }

                auto* const intermediate = m_program.getIntermediate(m_language);

                if (!options.sourceCodeFilePath.empty())
                {
                    intermediate->setSourceFile(options.sourceCodeFilePath.c_str());
                }

                if (spvOptions.generateDebugInfo)
                {
                    for (const auto& [path, f] : m_preprocessor.get_source_files_map())
                    {
                        intermediate->addIncludeText(path.c_str(), path.data(), path.size());
                    }
                }

                glslang::GlslangToSpv(*intermediate, m_spirv, &spvOptions);

                m_state = state::compilation_complete;

                return true;
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

            std::span<const u32> get_spirv() override
            {
                OBLO_ASSERT(m_state == state::compilation_complete);
                return std::span{m_spirv};
            }

        private:
            EShLanguage m_language;
            glslang::TShader m_shader;
            glslang::TProgram m_program;
            std::vector<unsigned int> m_spirv;
        };

        struct glslc_compilation final : glsl_compilation_base
        {
        public:
            glslc_compilation(allocator& allocator, shader_stage stage) :
                glsl_compilation_base{allocator}, m_stage{stage}, m_spirv{nullptr, 0, &allocator}
            {
            }

            bool compile(const shader_compiler_options& options, cstring_view workDir, cstring_view glslc, u32 id)
            {
                OBLO_ASSERT(m_state == state::preprocess_complete);

                const auto sourceCode = m_preprocessor.get_code();

                constexpr cstring_view spirvExtension = ".spirv";

                string_builder glslFile;
                glslFile.append(workDir).append_path_separator().format("{}", id).append(
                    glsl_deduce_extension(m_stage));

                if (!write_file(glslFile, as_bytes(std::span{sourceCode}), filesystem::write_mode::binary))
                {
                    m_state = state::compilation_failed;
                    m_error.clear().format("A filesystem error occurred while writing source code to {}", glslFile);
                    return false;
                }

                buffered_array<cstring_view, 7> args;
                args.push_back(glslFile);

                args.push_back("--target-env=vulkan1.3");
                args.push_back("--target-spv=spv1.5");
                args.push_back(options.codeOptimization ? "-O" : "-O0");

                if (options.generateDebugInfo)
                {
                    args.push_back("-g");
                }

                string_builder spirvFile;
                spirvFile.append(workDir).append_path_separator().format("{}", id).append(".spirv");

                args.push_back("-o");
                args.push_back(spirvFile);

                platform::process glslcProcess;

                if (!glslcProcess.start(glslc, args) || !glslcProcess.wait())
                {
                    m_error.clear()
                        .format("Failed to execute {} ", glslc)
                        .join(args.begin(), args.end(), " ", "\"{}\"");

                    m_state = state::compilation_failed;
                }
                else if (const auto exitCode = glslcProcess.get_exit_code().value_or(-1); exitCode != 0)
                {
                    m_error.clear()
                        .format("Failed to execute {} ", glslc)
                        .join(args.begin(), args.end(), " ", "\"{}\"");

                    // TODO: Read stdout/stderr
                    m_state = state::compilation_failed;
                }
                else
                {
                    auto spvData =
                        filesystem::load_binary_file_into_memory(m_spirv.get_allocator(), spirvFile, alignof(u32));

                    if (!spvData)
                    {
                        m_error.clear().format("A filesystem error occurred while reading spirv from {}", spirvFile);
                        m_state = state::compilation_failed;
                        return false;
                    }

                    m_spirv = std::move(*spvData);
                    m_state = state::compilation_complete;
                }

                return true;
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
                    return m_error.as<string_view>();
                }

                default:
                    return {};
                }
            }

            std::span<const u32> get_spirv() override
            {
                OBLO_ASSERT(m_state == state::compilation_complete);
                OBLO_ASSERT(m_spirv.size() % sizeof(u32) == 0);

                const auto n = m_spirv.size() / sizeof(u32);
                const auto spv = std::span{start_lifetime_as_array<u32>(m_spirv.data(), n), n};

                return spv;
            }

        private:
            shader_stage m_stage;
            unique_ptr<byte[]> m_spirv;
        };
    }

    void glslang_compiler::init(const shader_compiler_config& config)
    {
        m_includeDirs.clear();
        m_includeDirs.reserve(config.includeDirectories.size());

        for (const auto& p : config.includeDirectories)
        {
            m_includeDirs.emplace_back().append(p);
        }
    }

    shader_compiler::result glslang_compiler::preprocess_from_file(
        allocator& allocator, cstring_view path, shader_stage stage, string_view preamble)
    {
        auto r = std::make_unique<glslang_compilation>(allocator, stage);

        r->preprocess_from_file(path, preamble, m_includeDirs);

        return result{std::move(r)};
    }

    shader_compiler::result glslang_compiler::compile(result r, const shader_compiler_options& options)
    {
        get_shader_compiler_result_core_as<glslang_compilation>(r).compile(options);
        return r;
    }

    bool glslc_compiler::find_glslc()
    {
        char buf[260];
        usize bufSize = sizeof(buf);

        if (getenv_s(&bufSize, buf, bufSize, "VULKAN_SDK") == 0)
        {
            m_glslcPath = buf;
            m_glslcPath.append_path("Bin").append_path("glslc");

            if constexpr (platform::is_windows())
            {
                m_glslcPath.append(".exe");
            }

            return filesystem::exists(m_glslcPath).value_or(false);
        }
        return false;
    }

    void glslc_compiler::set_work_directory(string_view workDirectory)
    {
        m_workDirectory = workDirectory;
    }

    void glslc_compiler::init(const shader_compiler_config& config)
    {
        m_includeDirs.clear();
        m_includeDirs.reserve(config.includeDirectories.size());

        for (const auto& p : config.includeDirectories)
        {
            m_includeDirs.emplace_back().append(p);
        }
    }

    shader_compiler::result glslc_compiler::preprocess_from_file(
        allocator& allocator, cstring_view path, shader_stage stage, string_view preamble)
    {
        auto r = std::make_unique<glslc_compilation>(allocator, stage);

        r->preprocess_from_file(path, preamble, m_includeDirs);

        return result{std::move(r)};
    }

    shader_compiler::result glslc_compiler::compile(result r, const shader_compiler_options& options)
    {
        get_shader_compiler_result_core_as<glslc_compilation>(r).compile(options,
            m_workDirectory,
            m_glslcPath,
            ++m_counter);

        return r;
    }

    cstring_view glsl_deduce_extension(shader_stage stage)
    {
        cstring_view extension = ".glsl";

        switch (stage)
        {
        case shader_stage::vertex:
            extension = ".vert";
            break;

        case shader_stage::fragment:
            extension = ".frag";
            break;

        case shader_stage::compute:
            extension = ".comp";
            break;

        case shader_stage::mesh:
            extension = ".mesh";
            break;

        case shader_stage::raygen:
            extension = ".rgen";
            break;

        case shader_stage::intersection:
            extension = ".rint";
            break;

        case shader_stage::any_hit:
            extension = ".rahit";
            break;

        case shader_stage::closest_hit:
            extension = ".rchit";
            break;

        case shader_stage::miss:
            extension = ".rmiss";
            break;

        case shader_stage::callable:
            extension = ".rcall";
            break;

        default:
            unreachable();
        }

        return extension;
    }
}