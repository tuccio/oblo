#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_builder.hpp>

namespace oblo::editor
{
    class editor_directories
    {
    public:
        expected<> init(cstring_view temporaryDirectory);

        expected<> create_temporary_directory(string_builder& outDir);

    private:
        string m_temporaryDir;
    };
}