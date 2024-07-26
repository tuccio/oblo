#pragma once

#include <oblo/core/string/cstring_view.hpp>

namespace oblo
{
    class data_document;
}

namespace oblo::json
{
    bool read(data_document& doc, cstring_view source);

    bool write(const data_document& doc, cstring_view destination);
}