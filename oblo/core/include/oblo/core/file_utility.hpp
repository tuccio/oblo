#pragma once

#include <filesystem>
#include <span>

namespace oblo
{
    class frame_allocator;

    std::span<std::byte> load_binary_file_into_memory(frame_allocator& allocator, const std::filesystem::path& path);
    std::span<char> load_text_file_into_memory(frame_allocator& allocator, const std::filesystem::path& path);
}