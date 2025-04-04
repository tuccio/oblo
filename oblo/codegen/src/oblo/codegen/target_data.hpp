#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/string.hpp>

namespace oblo::gen
{
    enum class record_flags : u8
    {
        ecs_component,
        ecs_tag,
        enum_max,
    };

    struct field_type
    {
        string name;

        i32 attrClampMin{-1};
        i32 attrClampMax{-1};
    };

    struct record_type
    {
        string name;
        deque<field_type> fields;

        flags<record_flags> flags;

        i32 attrGpuComponent{-1};
    };

    struct target_data
    {
        deque<record_type> recordTypes;
        deque<double> numberAttributeData;
        deque<string> stringAttributeData;
    };
}