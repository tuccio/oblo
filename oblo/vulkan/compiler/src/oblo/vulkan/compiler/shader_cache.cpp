#include <oblo/vulkan/compiler/shader_cache.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/lifetime.hpp>
#include <oblo/core/reflection/fields.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/compiler/shader_compiler_result.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr bool DisableCache{true};
        constexpr bool OutputSource{true};
        constexpr bool OutputSpirv{true};

        class cached_spirv final : public shader_compiler::result_core
        {
        public:
            explicit cached_spirv(shader_compiler::result wrapped, std::span<const u32> spirv) :
                m_wrapped{std::move(wrapped)}, m_spirv{spirv}
            {
            }

            bool has_errors() override
            {
                return m_wrapped.has_errors();
            }

            string_view get_error_message() override
            {
                return m_wrapped.get_error_message();
            }

            string_view get_source_code() override
            {
                return m_wrapped.get_source_code();
            }

            void get_source_files(deque<string_view>& sourceFiles) override
            {
                return m_wrapped.get_source_files(sourceFiles);
            }

            std::span<const u32> get_spirv() override
            {
                return m_spirv;
            }

        private:
            shader_compiler::result m_wrapped;
            std::span<const u32> m_spirv;
        };

        class error_result final : public shader_compiler::result_core
        {
        public:
            constexpr explicit error_result(string_view error) : m_error{error} {}

            virtual bool has_errors()
            {
                return true;
            }

            virtual string_view get_error_message()
            {
                return m_error;
            }

            virtual string_view get_source_code()
            {
                OBLO_ASSERT(false);
                return {};
            }

            virtual void get_source_files(deque<string_view>&)
            {
                OBLO_ASSERT(false);
            }

            virtual std::span<const u32> get_spirv()
            {
                OBLO_ASSERT(false);
                return {};
            }

        private:
            string_view m_error;
        };

    }

    cstring_view glsl_deduce_extension(shader_stage stage);

    bool shader_cache::init(string_view dir)
    {
        m_path.clear().append(dir);

        const auto cstr = m_path.view();

        if (!filesystem::create_directories(cstr))
        {
            return false;
        }

        if (filesystem::exists(cstr).value_or(false) || !filesystem::is_directory(cstr).value_or(false))
        {
            return false;
        }

        m_path.make_absolute_path();

        return true;
    }

    void shader_cache::set_glsl_compiler(shader_compiler* glslCompiler)
    {
        m_glslCompiler = glslCompiler;
    }

    shader_compiler* shader_cache::get_glsl_compiler() const
    {
        return m_glslCompiler;
    }

    shader_compiler::result shader_cache::find_or_compile(frame_allocator& allocator,
        cstring_view filePath,
        shader_stage stage,
        string_view preamble,
        const shader_compiler_options& options,
        string_view debugName)
    {
        OBLO_PROFILE_SCOPE();

        constexpr auto numOptions{count_fields<shader_compiler_options>()};
        static_assert(numOptions == 3, "The cache hash might need to be updated");

        if (!m_glslCompiler)
        {
            return shader_compiler::result{allocate_unique<error_result>("No GLSL compiler is available")};
        }

        shader_compiler::result result = m_glslCompiler->preprocess_from_file(allocator, filePath, stage, preamble);

        if (result.has_errors())
        {
            return result;
        }

        const auto sourceCode = result.get_source_code();
        u64 id = hash_xxh64(sourceCode.data(), sourceCode.size());

        id = hash_xxh64(&stage, sizeof(stage), id);
        id = hash_xxh64(&options.codeOptimization, sizeof(options.codeOptimization), id);
        id = hash_xxh64(&options.generateDebugInfo, sizeof(options.generateDebugInfo), id);

        string_builder spvPath;
        spvPath.append(m_path).append_path_separator().format("{}_{}.spirv", debugName, id);

        if constexpr (!DisableCache)
        {
            const auto diskSpv = filesystem::load_binary_file_into_memory(allocator, spvPath, alignof(u32));

            if (diskSpv && !diskSpv->empty())
            {
                const auto n = diskSpv->size() / sizeof(u32);
                const auto spv = std::span{start_lifetime_as_array<u32>(diskSpv->data(), n), n};

                return shader_compiler::result{allocate_unique<cached_spirv>(std::move(result), spv)};
            }
        }

        string_builder sourceCodePath;

        if constexpr (OutputSource)
        {
            sourceCodePath = spvPath;
            sourceCodePath.append(glsl_deduce_extension(stage));

            write_file(sourceCodePath,
                as_bytes(std::span{sourceCode.data(), sourceCode.size()}),
                filesystem::write_mode::binary)
                .assert_value();
        }

        {
            OBLO_PROFILE_SCOPE_NAMED(CompileScope, "Compile shader");

            shader_compiler_options optionsCopy = options;
            optionsCopy.sourceCodeFilePath = sourceCodePath;

            result = m_glslCompiler->compile(std::move(result), optionsCopy);
        }

        if (!result.has_errors())
        {
            if constexpr (!DisableCache || OutputSpirv)
            {
                write_file(spvPath, as_bytes(result.get_spirv()), filesystem::write_mode::binary).assert_value();
            }
        }

        return result;
    }
}