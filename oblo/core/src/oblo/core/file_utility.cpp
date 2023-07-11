#include <oblo/core/file_utility.hpp>

#include <oblo/core/finally.hpp>
#include <oblo/core/frame_allocator.hpp>

#include <cstdio>

namespace oblo
{
    namespace
    {
        std::span<char> load_impl(frame_allocator& allocator, const std::filesystem::path& path, const char* mode)
        {
            FILE* file;

            // TODO: Should avoid transcoding and possibily allocating with path::string()
            if (fopen_s(&file, path.string().c_str(), mode) != 0)
            {
                return {};
            }

            const auto closeFile = finally([file] { fclose(file); });

            if (fseek(file, 0, SEEK_END) != 0)
            {
                return {};
            }

            const auto fileSize = std::size_t(ftell(file));

            if (fseek(file, 0, SEEK_SET) != 0)
            {
                return {};
            }

            auto* const buffer = allocator.allocate(fileSize, 1u);
            const auto readBytes = fread(buffer, 1, fileSize, file);

            if (readBytes < 0)
            {
                return {};
            }

            return {static_cast<char*>(buffer), readBytes};
        }
    }

    std::span<std::byte> load_binary_file_into_memory(frame_allocator& allocator, const std::filesystem::path& path)
    {
        return std::as_writable_bytes(load_impl(allocator, path, "rb"));
    }

    std::span<char> load_text_file_into_memory(frame_allocator& allocator, const std::filesystem::path& path)
    {
        return load_impl(allocator, path, "r");
    }
}