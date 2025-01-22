#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct import_artifact
    {
        uuid id;
        type_id type;
        string name;
        string path;
    };
}