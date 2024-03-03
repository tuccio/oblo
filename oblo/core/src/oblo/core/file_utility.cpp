#include <oblo/core/file_utility.hpp>

#include <oblo/core/finally.hpp>
#include <oblo/core/frame_allocator.hpp>

#include <cstdio>

namespace oblo
{
    namespace
    {
        void* allocate_impl(std::vector<std::byte>& allocator, usize size)
        {
            allocator.assign(size, std::byte{});
            return allocator.data();
        }

        char* allocate_impl(std::string& allocator, usize size)
        {
            allocator.assign(size, '\0');
            return allocator.data();
        }

        void* allocate_impl(frame_allocator& allocator, usize size)
        {
            return allocator.allocate(size, 1u);
        }

        template <typename Allocator>
        std::span<char> load_impl(Allocator& allocator, const std::filesystem::path& path, const char* mode)
        {
            FILE* file;

            // TODO: Should avoid transcoding and possibly allocating with path::string()
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

            auto* const buffer = fileSize == 0 ? nullptr : allocate_impl(allocator, fileSize);
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

    std::span<std::byte> load_binary_file_into_memory(std::vector<std::byte>& out, const std::filesystem::path& path)
    {
        return std::as_writable_bytes(load_impl(out, path, "rb"));
    }

    std::span<char> load_text_file_into_memory(std::string& out, const std::filesystem::path& path)
    {
        return load_impl(out, path, "r");
    }

    FILE* open_file(const std::filesystem::path& path, const char* mode)
    {
        constexpr u32 N{31};
        wchar_t wMode[N + 1];

        for (u32 i = 0; i < N; ++i)
        {
            if (mode[i] == '\0')
            {
                wMode[i] = 0;
                break;
            }

            wMode[i] = wchar_t(mode[i]);
        }

        wMode[N] = 0;

#ifdef WIN32
        FILE* f{};
        _wfopen_s(&f, path.native().c_str(), wMode);
        return f;

#else
        return fopen(path.native().c_str(), mode);
#endif
    }
}