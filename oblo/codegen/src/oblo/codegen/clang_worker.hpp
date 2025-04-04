#pragma once

#include <clang-c/Index.h>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>

#include "target_data.hpp"

namespace oblo::gen
{
    class clang_worker
    {
    public:
        clang_worker();

        clang_worker(const clang_worker&) = delete;
        clang_worker(clang_worker&&) noexcept = delete;

        clang_worker& operator=(const clang_worker&) = delete;
        clang_worker& operator=(clang_worker&&) noexcept = delete;

        ~clang_worker();

        expected<target_data> parse_code(cstring_view sourceFile, const dynamic_array<const char*> args);

    private:
        CXIndex m_index{};
    };
}