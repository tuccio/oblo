#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/script/bytecode.hpp>
#include <oblo/script/operations.hpp>

namespace oblo::script
{
    struct exported_function
    {
        string id;
        u32 paramsSize;
        u32 returnSize;
        u32 textOffset;
    };

    struct module
    {
        dynamic_array<exported_function> functions;
        dynamic_array<bytecode_instruction> text;
    };
}