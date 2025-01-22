#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/properties/serialization/data_document.hpp>

namespace oblo
{
    struct import_config
    {
        string sourceFile;
        data_document settings;
    };
}