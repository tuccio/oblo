#pragma once

#include <oblo/asset/asset_traits.hpp>

namespace oblo
{
    class material;
    class mesh;
    class texture;
    struct model;

    template <>
    struct asset_traits<model>
    {
        static constexpr uuid uuid = "8d5f7a83-faee-41eb-a5c7-ff5f456fe1a5"_uuid;
    };

    template <>
    struct asset_traits<texture>
    {
        static constexpr uuid uuid = "79dfb4aa-bfa4-4833-8b96-756a2eff96eb"_uuid;
    };

    template <>
    struct asset_traits<material>
    {
        static constexpr uuid uuid = "0d9945db-c776-4258-b403-feee59b5362f"_uuid;
    };

    template <>
    struct asset_traits<mesh>
    {
        static constexpr uuid uuid = "2d5702d2-3cda-4273-ab5c-39cf6a487814"_uuid;
    };
}