#pragma once

#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>

namespace oblo::platform
{
    void open_folder(string_view dir);

    bool open_file_dialog(string_builder& file);

    bool search_program_files(string_builder& out, string_view relativePath);
}