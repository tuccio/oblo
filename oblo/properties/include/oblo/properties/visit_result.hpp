#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    enum visit_result : u8
    {
        terminate,
        sibling,
        array_elements,
        recurse,
    };
}