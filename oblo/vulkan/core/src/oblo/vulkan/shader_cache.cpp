#include <oblo/vulkan/shader_cache.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/file_utility.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/lifetime.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/vulkan/shader_compiler.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr bool OutputSource{true};

        template <typename T>
        consteval usize count_fields()
        {
            return struct_apply([]([[maybe_unused]] auto&&... m) { return sizeof...(m); }, T{});
        }

        void write_file(const std::filesystem::path& path, std::span<const byte> data)
        {
            file_ptr f{open_file(path, "wb")};

            if (f && fwrite(data.data(), sizeof(data[0]), data.size(), f.get()) != data.size())
            {
                f.reset();

                std::error_code ec;
                std::filesystem::remove(path, ec);
            }
        }
    }

    bool shader_cache::init(const std::filesystem::path& dir)
    {
        std::error_code ec;

        std::filesystem::create_directories(dir, ec);

        if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
        {
            return false;
        }

        m_path = std::filesystem::absolute(dir, ec);
        return true;
    }

    bool shader_cache::find_or_add(std::span<unsigned>& outSpirv,
        frame_allocator& allocator,
        std::string_view debugName,
        std::string_view sourceCode,
        VkShaderStageFlagBits stage,
        const shader_compiler::options& options)
    {
        constexpr auto numOptions{count_fields<shader_compiler::options>()};
        static_assert(numOptions == 3, "The cache hash might need to be updated");

        u64 id = hash_xxh64(sourceCode.data(), sourceCode.size());

        id = hash_xxh64(&stage, sizeof(stage), id);
        id = hash_xxh64(&options.codeOptimization, sizeof(options.codeOptimization), id);
        id = hash_xxh64(&options.generateDebugInfo, sizeof(options.generateDebugInfo), id);

        char buf[64];
        const auto endIt = std::format_to(buf, "{}.spirv", id);
        *endIt = '\0';

        auto spvPath = m_path / buf;

        const auto diskSpv = load_binary_file_into_memory(allocator, spvPath, alignof(u32));

        if (!diskSpv.empty())
        {
            const auto count = diskSpv.size() / sizeof(u32);
            outSpirv = {start_lifetime_as_array<unsigned>(diskSpv.data(), count), count};
            return true;
        }

        std::vector<u32> spirv;

        if (!shader_compiler::compile_glsl_to_spirv(debugName, sourceCode, stage, spirv, options))
        {
            return false;
        }

        outSpirv = allocate_n_span<u32>(allocator, spirv.size());
        std::copy(spirv.begin(), spirv.end(), outSpirv.begin());

        write_file(spvPath, as_bytes(outSpirv));

        if constexpr (OutputSource)
        {
            spvPath.replace_extension("glsl");
            write_file(spvPath, as_bytes(std::span{sourceCode.data(), sourceCode.size()}));
        }

        return true;
    }
}