#pragma once

#include <filesystem>

namespace oblo::platform
{
    void open_folder(const std::filesystem::path& dir);

    bool open_file_dialog(std::filesystem::path& pick);
}