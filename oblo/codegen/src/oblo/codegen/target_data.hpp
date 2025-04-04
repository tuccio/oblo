#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/deque.hpp>

namespace oblo::gen
{
    struct field_type
    {
        string name;
    };

    enum class record_flags : u8
    {
        ecs_component,
        ecs_tag,
        enum_max,
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
        deque<string> stringAttributeData;
    };
}