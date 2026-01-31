#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/editor/app/run_config.hpp>
#include <oblo/project/project.hpp>

namespace oblo::editor
{
    class launcher
    {
    public:
        launcher();
        launcher(const launcher&) = delete;
        launcher(launcher&&) noexcept = delete;

        launcher& operator=(const launcher&) = delete;
        launcher& operator=(launcher&&) noexcept = delete;

        expected<> run(int argc, char* argv[], run_config& outConfig);
    };
}