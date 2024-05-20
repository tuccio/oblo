#pragma once

#include <oblo/core/expected.hpp>

#include <filesystem>

namespace oblo::platform
{
    void open_folder(const std::filesystem::path& dir);

    bool open_file_dialog(std::filesystem::path& pick);

    expected<std::filesystem::path> search_program_files(const std::filesystem::path& relativePath);
}