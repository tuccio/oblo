#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/constants.hpp>

namespace oblo
{
    constexpr bool float_equal(f32 lhs, f32 rhs, f32 tolerance = epsilon)
    {
        return lhs >= rhs - tolerance && lhs <= rhs + tolerance;
    }
}