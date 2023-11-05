#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

namespace oblo::reflection
{
    enum class type_kind : u8
    {
        class_kind,
    };

    struct type_data
    {
        type_id type;
        type_kind kind;
        u32 size;
        u32 alignment;
    };

    struct field_data
    {
        type_id type;
        std::string_view name;
        u32 offset;
    };
}