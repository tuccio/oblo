#pragma once

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

    std::span<std::byte> load_binary_file_into_memory(frame_allocator& allocator, const std::filesystem::path& path);
    std::span<char> load_text_file_into_memory(frame_allocator& allocator, const std::filesystem::path& path);

    std::span<std::byte> load_binary_file_into_memory(std::vector<std::byte>& out, const std::filesystem::path& path);
    std::span<char> load_text_file_into_memory(std::string& out, const std::filesystem::path& path);

    FILE* open_file(const std::filesystem::path& path, const char* mode);
}