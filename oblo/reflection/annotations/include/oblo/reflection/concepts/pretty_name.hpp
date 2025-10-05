#pragma once

#include <oblo/core/string/string_view.hpp>

namespace oblo::reflection
{
    struct pretty_name
    {
        string_view identifier;
    };
}