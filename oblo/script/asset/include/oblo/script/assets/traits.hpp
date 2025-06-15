#pragma once

#include <oblo/asset/asset_traits.hpp>

namespace oblo
{
    class script_graph;

    template <>
    struct asset_traits<script_graph>
    {
        static constexpr uuid uuid = "13f753f3-1d11-4c55-a792-9370e3279d05"_uuid;
    };
}