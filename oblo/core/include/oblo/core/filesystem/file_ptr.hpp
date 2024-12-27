#pragma once

#include <cstdio>
#include <memory>

namespace oblo::filesystem
{
    using file_ptr = std::unique_ptr<FILE, decltype([](FILE* f) { fclose(f); })>;
}