#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>

namespace oblo
{
    class data_document;
}

namespace oblo::json
{
    expected<> read(data_document& doc, cstring_view source);

    expected<> write(const data_document& doc, cstring_view destination);
}