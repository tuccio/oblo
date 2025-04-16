#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/flags.hpp>

#include <unordered_set>

namespace oblo::gen
{
    enum class field_flags : u8
    {
        linear_color,
        enum_max,
    };

    enum class record_flags : u8
    {
        ecs_component,
        ecs_tag,
        resource,
        script_api,
        transient,
        enum_max,
    };

    struct enum_type
    {
        string name;
        deque<string> enumerators;
    };

    struct field_type
    {
        string name;

        flags<field_flags> flags;

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
        string name;

        deque<record_type> recordTypes;
        deque<enum_type> enumTypes;
        deque<double> numberAttributeData;
        deque<string> stringAttributeData;
    };
}