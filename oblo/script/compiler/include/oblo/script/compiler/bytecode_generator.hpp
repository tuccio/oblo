#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/script/bytecode_module.hpp>

namespace oblo
{
    class abstract_syntax_tree;

    class bytecode_generator
    {
    public:
        expected<bytecode_module> generate_module(const abstract_syntax_tree& ast);
    };
}