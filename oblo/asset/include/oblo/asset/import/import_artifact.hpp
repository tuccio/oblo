#pragma once

#include <oblo/asset/import/any_artifact.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct import_artifact
    {
        uuid id;
        any_artifact data;
        string name;
    };
}