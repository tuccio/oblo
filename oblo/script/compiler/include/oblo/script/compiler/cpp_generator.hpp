#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/string_builder.hpp>

namespace oblo
{
    class abstract_syntax_tree;

    class cpp_generator
    {
    public:
        expected<string_builder> generate_code(const abstract_syntax_tree& ast);
    };
}