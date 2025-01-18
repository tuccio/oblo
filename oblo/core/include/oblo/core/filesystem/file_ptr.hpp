#pragma once

#include <oblo/core/unique_ptr.hpp>

#include <cstdio>

namespace oblo::filesystem
{
    using file_ptr = unique_ptr<FILE, decltype([](FILE* f) { fclose(f); })>;
}