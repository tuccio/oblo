#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    enum class property_kind : u8;

    enum class data_node_kind : u8
    {
        none,
        object,
        value,
        array,
    };

    struct data_node
    {
        static constexpr u32 Invalid{~0u};

        data_node_kind kind;
        property_kind valueKind;
        u16 keyLen;
        u32 nextSibling;
        const char* key;
        usize keyHash;

        union {
            struct
            {
                u32 firstChild;
                u32 lastChild;
                u32 childrenCount;
            } objectOrArray;

            struct
            {
                void* data;
            } value;
        };
    };
}