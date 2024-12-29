#include <oblo/vulkan/shader_cache.hpp>

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
#include <oblo/vulkan/shader_compiler.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr bool DisableCache{false};
        constexpr bool OutputSource{true};

        void write_file(cstring_view path, std::span<const byte> data)
        {
            filesystem::file_ptr f{filesystem::open_file(path, "wb")};

            if (f && fwrite(data.data(), sizeof(data[0]), data.size(), f.get()) != data.size())
            {
                f.reset();

                filesystem::remove(path).assert_value();
            }
        }
    }

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

    bool shader_cache::find_or_add(std::span<unsigned>& outSpirv,
        frame_allocator& allocator,
        string_view debugName,
        string_view sourceCode,
        VkShaderStageFlagBits stage,
        const shader_compiler::options& options)
    {
        OBLO_PROFILE_SCOPE();

        constexpr auto numOptions{count_fields<shader_compiler::options>()};
        static_assert(numOptions == 3, "The cache hash might need to be updated");

        u64 id = hash_xxh64(sourceCode.data(), sourceCode.size());

        id = hash_xxh64(&stage, sizeof(stage), id);
        id = hash_xxh64(&options.codeOptimization, sizeof(options.codeOptimization), id);
        id = hash_xxh64(&options.generateDebugInfo, sizeof(options.generateDebugInfo), id);

        string_builder spvPath;
        spvPath.append(m_path).append_path_separator().format("{}.spirv", id);

        if constexpr (!DisableCache)
        {
            const auto diskSpv = filesystem::load_binary_file_into_memory(allocator, spvPath, alignof(u32));

            if (diskSpv && !diskSpv->empty())
            {
                const auto count = diskSpv->size() / sizeof(u32);
                outSpirv = {start_lifetime_as_array<unsigned>(diskSpv->data(), count), count};
                return true;
            }
        }

        std::vector<u32> spirv;

        {
            OBLO_PROFILE_SCOPE_NAMED(CompileScope, "Compile shader");

            if (!shader_compiler::compile_glsl_to_spirv(debugName, sourceCode, stage, spirv, options))
            {
                return false;
            }
        }

        outSpirv = allocate_n_span<u32>(allocator, spirv.size());
        std::copy(spirv.begin(), spirv.end(), outSpirv.begin());

        if constexpr (!DisableCache)
        {
            write_file(spvPath, as_bytes(outSpirv));
        }

        if constexpr (OutputSource)
        {
            const char* extension = "glsl";

            switch (stage)
            {
            case VK_SHADER_STAGE_VERTEX_BIT:
                extension = "vert";
                break;

            case VK_SHADER_STAGE_FRAGMENT_BIT:
                extension = "frag";
                break;

            case VK_SHADER_STAGE_COMPUTE_BIT:
                extension = "comp";
                break;

            case VK_SHADER_STAGE_MESH_BIT_EXT:
                extension = "mesh";
                break;

            case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
                extension = "rgen";
                break;

            case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
                extension = "rint";
                break;

            case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
                extension = "rahit";
                break;

            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
                extension = "rchit";
                break;

            case VK_SHADER_STAGE_MISS_BIT_KHR:
                extension = "rmiss";
                break;

            case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
                extension = "rcall";
                break;

            default:
                unreachable();
            }

            spvPath.append(extension);
            write_file(spvPath, as_bytes(std::span{sourceCode.data(), sourceCode.size()}));
        }

        return true;
    }
}