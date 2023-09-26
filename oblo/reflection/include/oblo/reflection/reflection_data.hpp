#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

namespace oblo::reflection
{
    struct field_data
    {
        type_id type;
        std::string_view name;
        u32 offset;
    };
}