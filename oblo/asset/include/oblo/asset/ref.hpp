#pragma once

#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo::asset
{
    template <typename T>
    struct ref
    {
        uuid id;
    };
}