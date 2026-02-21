#pragma once

#include <oblo/core/handle.hpp>

namespace oblo
{
    namespace gpu
    {
        struct acceleration_structure;
        struct buffer;
        struct image;
    }

    template <typename T>
    class dynamic_array;

    template <typename T>
    struct resource_pin
    {
    };

    template <typename T>
    struct data_pin
    {
    };

    template <typename T>
    struct data_sink_pin
    {
    };

    namespace pin
    {
        template <typename T>
        using resource = h32<resource_pin<T>>;

        template <typename T>
        using data = h32<data_pin<T>>;

        template <typename T>
        using data_sink = h32<data_sink_pin<T>>;

        template <typename T>
        using data_sink_container = dynamic_array<T>;

        using acceleration_structure = resource<gpu::acceleration_structure>;
        using buffer = resource<gpu::buffer>;
        using texture = resource<gpu::image>;
    }
}