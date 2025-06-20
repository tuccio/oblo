#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct node_property_descriptor
    {
        string name;
        uuid typeId{};
    };
}