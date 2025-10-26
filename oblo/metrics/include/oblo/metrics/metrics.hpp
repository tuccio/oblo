#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/type_id.hpp>

namespace oblo
{
    struct metrics_entry
    {
        type_id type;
        buffered_array<byte, 256> data;
    };

    struct metrics
    {
        using entry = metrics_entry;

        deque<entry> entries;
    };
}