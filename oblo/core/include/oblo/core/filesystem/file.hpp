#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>

#include <cstdio>
#include <memory>
#include <span>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::filesystem
{
    using file_ptr = std::unique_ptr<FILE, decltype([](FILE* f) { fclose(f); })>;

    expected<std::span<byte>> load_binary_file_into_memory(
        frame_allocator& allocator, cstring_view path, usize alignment = 1);

    expected<std::span<char>> load_text_file_into_memory(
        frame_allocator& allocator, cstring_view path, usize alignment = 1);

    expected<std::span<byte>> load_binary_file_into_memory(
        dynamic_array<byte>& out, cstring_view path, usize alignment = 1);

    expected<std::span<char>> load_text_file_into_memory(string_builder& out, cstring_view path, usize alignment = 1);

    FILE* open_file(cstring_view path, const char* mode);
}