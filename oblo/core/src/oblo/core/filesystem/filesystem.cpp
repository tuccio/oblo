#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>

#include <oblo/core/finally.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/platform/platform_win32.hpp>

#include <utf8cpp/utf8.h>

#include <cstdio>
#include <filesystem>

namespace oblo::filesystem
{
    namespace
    {
        void* allocate_impl(dynamic_array<byte>& allocator, usize size, usize)
        {
            allocator.assign(size, byte{});
            return allocator.data();
        }

        char* allocate_impl(string_builder& allocator, usize size, usize)
        {
            allocator.clear().resize(size);
            auto* const m = allocator.mutable_data().data();
            return m;
        }

        void* allocate_impl(allocator* allocator, usize size, usize alignment)
        {
            return allocator->allocate(size, alignment);
        }

        void* allocate_impl(frame_allocator& allocator, usize size, usize alignment)
        {
            return allocator.allocate(size, alignment);
        }

        template <typename T>
        void finalize_impl(const T&, usize)
        {
        }

        void finalize_impl(string_builder& allocator, usize readBytes)
        {
            allocator.resize(readBytes);
        }

        template <typename Allocator>
        expected<std::span<char>> load_impl(Allocator& allocator, cstring_view path, const char* mode, usize alignment)
        {
            const file_ptr f{open_file(path, mode)};

            if (!f)
            {
                return unspecified_error;
            }

            FILE* const file = f.get();

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

            finalize_impl(allocator, readBytes);

            return std::span{static_cast<char*>(buffer), readBytes};
        }
    }

    expected<unique_ptr<byte[]>> load_binary_file_into_memory(allocator* allocator, cstring_view path, usize alignment)
    {
        const auto e = load_impl(allocator, path, "rb", alignment);
        expected<unique_ptr<byte[]>> res{unspecified_error};

        if (e)
        {
            const auto span = std::as_writable_bytes(*e);
            res = unique_ptr<byte[]>(span.data(), span.size(), allocator);
        }

        return res;
    }

    expected<std::span<byte>> load_binary_file_into_memory(
        frame_allocator& allocator, cstring_view path, usize alignment)
    {
        const auto e = load_impl(allocator, path, "rb", alignment);
        expected<std::span<byte>> res{unspecified_error};

        if (e)
        {
            res = std::as_writable_bytes(*e);
        }

        return res;
    }

    expected<std::span<char>> load_text_file_into_memory(frame_allocator& allocator, cstring_view path, usize alignment)
    {
        return load_impl(allocator, path, "r", alignment);
    }

    expected<std::span<byte>> load_binary_file_into_memory(dynamic_array<byte>& out, cstring_view path)
    {
        const auto e = load_impl(out, path, "rb", 1);
        expected<std::span<byte>> res{unspecified_error};

        if (e)
        {
            res = std::as_writable_bytes(*e);
        }

        return res;
    }

    expected<std::span<char>> load_text_file_into_memory(string_builder& out, cstring_view path)
    {
        return load_impl(out, path, "r", 1);
    }

    FILE* open_file(cstring_view path, const char* mode)
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
        wchar_t buf[win32::MaxPath];
        win32::convert_path(path, buf);

        FILE* f{};
        _wfopen_s(&f, buf, wMode);
        return f;

#else
        return fopen(path.c_str(), mode);
#endif
    }

    expected<> write_file(cstring_view destination, std::span<const byte> bytes, flags<write_mode> mode)
    {
        const char* cMode{};

        if (mode.is_empty())
        {
            cMode = "w";
        }
        else if (mode == write_mode::binary)
        {
            cMode = "wb";
        }
        else if (mode == write_mode::append)
        {
            cMode = "w+";
        }
        else if (mode == (write_mode::binary | write_mode::append))
        {
            cMode = "wb+";
        }

        OBLO_ASSERT(cMode);

        filesystem::file_ptr f{filesystem::open_file(destination, cMode)};

        if (!f || fwrite(bytes.data(), sizeof(bytes[0]), bytes.size(), f.get()) != bytes.size())
        {
            f.reset();
            return unspecified_error;
        }

        return no_error;
    }

    expected<bool> exists(cstring_view path)
    {
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        std::error_code ec;
        const bool exists = std::filesystem::exists(p, ec);

        if (ec)
        {
            return unspecified_error;
        }

        return exists;
    }

    expected<bool> remove(cstring_view path)
    {
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        std::error_code ec;

        const auto r = std::filesystem::remove(p, ec);

        if (ec)
        {
            return unspecified_error;
        }

        return r;
    }

    expected<bool> remove_all(cstring_view path)
    {
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        std::error_code ec;

        const auto r = std::filesystem::remove_all(p, ec);

        if (ec)
        {
            return unspecified_error;
        }

        return r;
    }

    expected<bool> rename(cstring_view from, cstring_view to)
    {
        std::filesystem::path f{std::u8string_view{from.u8data(), from.size()}};
        std::filesystem::path t{std::u8string_view{to.u8data(), to.size()}};
        std::error_code ec;

        std::filesystem::rename(f, t, ec);

        if (ec)
        {
            return unspecified_error;
        }

        return true;
    }

    expected<bool> copy_file(cstring_view source, cstring_view destination)
    {
        std::filesystem::path p1{std::u8string_view{source.u8data(), source.size()}};
        std::filesystem::path p2{std::u8string_view{destination.u8data(), destination.size()}};

        std::error_code ec;

        const auto r = std::filesystem::copy_file(p1, p2, ec);

        if (ec)
        {
            return unspecified_error;
        }

        return r;
    }

    expected<bool> create_directories(cstring_view path)
    {
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        std::error_code ec;

        const auto r = std::filesystem::create_directories(p, ec);

        if (ec)
        {
            return unspecified_error;
        }

        return r;
    }

    expected<bool> is_directory(cstring_view path)
    {
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        std::error_code ec;

        const auto r = std::filesystem::is_directory(p, ec);

        if (ec)
        {
            return unspecified_error;
        }

        return r;
    }

    expected<> absolute(cstring_view path, string_builder& out)
    {
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        std::error_code ec;

        const auto r = std::filesystem::absolute(p, ec);

        if (ec)
        {
            return unspecified_error;
        }

        out.append(p.c_str());

        return no_error;
    }

    bool is_relative(string_view path)
    {
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        return p.is_relative();
    }

    expected<> relative(string_view path, string_view basePath, string_builder& out)
    {
        std::filesystem::path b{std::u8string_view{basePath.u8data(), basePath.size()}};
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};

        std::error_code ec;

        if (ec)
        {
            return unspecified_error;
        }

        const auto r = std::filesystem::relative(p, b, ec);
        out.append(r.native().c_str());

        return no_error;
    }

    string_view extension(string_view path)
    {
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        const auto n = p.extension().u8string().size();
        return path.substr(path.size() - n);
    }

    string_view stem(string_view path)
    {
        path = filename(path);

        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        const auto n = p.stem().u8string().size();
        return path.substr(0, n);
    }

    string_view parent_path(string_view path)
    {
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        const auto n = p.parent_path().u8string().size();
        return path.substr(0, n);
    }

    string_view filename(string_view path)
    {
        std::filesystem::path p{std::u8string_view{path.u8data(), path.size()}};
        const auto n = p.filename().u8string().size();
        return path.substr(path.size() - n);
    }
}