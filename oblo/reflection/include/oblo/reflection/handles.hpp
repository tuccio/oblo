#pragma once

#include <oblo/core/handle.hpp>

namespace oblo::reflection
{
    struct class_data;
    struct type_data;

    using class_handle = h32<class_data>;
    using type_handle = h32<type_data>;
}