#include <oblo/core/file_utility.hpp>

#include <oblo/core/finally.hpp>
#include <oblo/core/frame_allocator.hpp>

#include <cstdio>

namespace oblo
{
    namespace
    {
        void* allocate_impl(std::vector<std::byte>& allocator, usize size, usize)
        {
            allocator.assign(size, std::byte{});
            return allocator.data();
        }

        char* allocate_impl(std::string& allocator, usize size, usize)
        {
            allocator.assign(size, '\0');
            return allocator.data();
        }

        void* allocate_impl(frame_allocator& allocator, usize size, usize alignment)
        {
            return allocator.allocate(size, alignment);
        }

        template <typename Allocator>
        expected<std::span<char>> load_impl(
            Allocator& allocator, const std::filesystem::path& path, const char* mode, usize alignment)
        {
            FILE* file;

            // TODO: Should avoid transcoding and possibly allocating with path::string()
            if (const auto ret = fopen_s(&file, path.string().c_str(), mode); ret != 0)
            {
                return unspecified_error;
            }

            const auto closeFile = finally([file] { fclose(file); });

            if (fseek(file, 0, SEEK_END) != 0)
            {
                return unspecified_error;
            }

            const auto fileSize = std::size_t(ftell(file));

            if (fseek(file, 0, SEEK_SET) != 0)
            {
                return unspecified_error;
            }

            auto* const buffer = fileSize == 0 ? nullptr : allocate_impl(allocator, fileSize, alignment);
            const auto readBytes = fread(buffer, 1, fileSize, file);

            if (readBytes < 0)
            {
                return unspecified_error;
            }

            return std::span{static_cast<char*>(buffer), readBytes};
        }
    }

    expected<std::span<std::byte>> load_binary_file_into_memory(
        frame_allocator& allocator, const std::filesystem::path& path, usize alignment)
    {
        const auto e = load_impl(allocator, path, "rb", alignment);
        expected<std::span<std::byte>> res{unspecified_error};

        if (e)
        {
            res = std::as_writable_bytes(*e);
        }

        return res;
    }

    expected<std::span<char>> load_text_file_into_memory(
        frame_allocator& allocator, const std::filesystem::path& path, usize alignment)
    {
        return load_impl(allocator, path, "r", alignment);
    }

    expected<std::span<std::byte>> load_binary_file_into_memory(
        std::vector<std::byte>& out, const std::filesystem::path& path, usize alignment)
    {
        const auto e = load_impl(out, path, "rb", alignment);
        expected<std::span<std::byte>> res{unspecified_error};

        if (e)
        {
            res = std::as_writable_bytes(*e);
        }

        return res;
    }

    expected<std::span<char>> load_text_file_into_memory(
        std::string& out, const std::filesystem::path& path, usize alignment)
    {
        return load_impl(out, path, "r", alignment);
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