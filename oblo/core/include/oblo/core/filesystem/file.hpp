#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/filesystem/file_ptr.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/unique_ptr.hpp>

#include <cstdio>
#include <span>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::filesystem
{
    expected<unique_ptr<byte[]>> load_binary_file_into_memory(
        allocator* allocator, cstring_view path, usize alignment = 1);

    expected<std::span<byte>> load_binary_file_into_memory(
        frame_allocator& allocator, cstring_view path, usize alignment = 1);

    expected<std::span<char>> load_text_file_into_memory(
        frame_allocator& allocator, cstring_view path, usize alignment = 1);

    expected<std::span<byte>> load_binary_file_into_memory(dynamic_array<byte>& out, cstring_view path);

    expected<std::span<char>> load_text_file_into_memory(string_builder& out, cstring_view path);

    FILE* open_file(cstring_view path, const char* mode);

    enum class write_mode : u8
    {
        binary,
        append,
        enum_max,
    };

    expected<> write_file(cstring_view destination, std::span<const byte> bytes, flags<write_mode> mode);
}