#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace oblo
{
    class frame_allocator;

    std::span<std::byte> load_binary_file_into_memory(frame_allocator& allocator, const std::filesystem::path& path);
    std::span<char> load_text_file_into_memory(frame_allocator& allocator, const std::filesystem::path& path);

    std::span<std::byte> load_binary_file_into_memory(std::vector<std::byte>& out, const std::filesystem::path& path);
    std::span<char> load_text_file_into_memory(std::string& out, const std::filesystem::path& path);
}