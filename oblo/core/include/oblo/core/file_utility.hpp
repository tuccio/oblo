#pragma once

#include <oblo/core/types.hpp>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace oblo
{
    class frame_allocator;

    using file_ptr = std::unique_ptr<FILE, decltype([](FILE* f) { fclose(f); })>;

    std::span<std::byte> load_binary_file_into_memory(
        frame_allocator& allocator, const std::filesystem::path& path, usize alignment = 1);

    std::span<char> load_text_file_into_memory(
        frame_allocator& allocator, const std::filesystem::path& path, usize alignment = 1);

    std::span<std::byte> load_binary_file_into_memory(
        std::vector<std::byte>& out, const std::filesystem::path& path, usize alignment = 1);

    std::span<char> load_text_file_into_memory(
        std::string& out, const std::filesystem::path& path, usize alignment = 1);

    FILE* open_file(const std::filesystem::path& path, const char* mode);
}