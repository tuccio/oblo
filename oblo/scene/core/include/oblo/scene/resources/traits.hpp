#pragma once

#include <oblo/resource/resource_traits.hpp>

namespace oblo
{
    class entity_hierarchy;
    class material;
    class mesh;
    class texture;
    struct model;

    template <>
    struct resource_traits<entity_hierarchy>
    {
        static constexpr uuid uuid = "25d59602-8734-402d-8525-f779ed80ae09"_uuid;
    };

    template <>
    struct resource_traits<material>
    {
        static constexpr uuid uuid = "be9cf615-bbba-4174-bf41-e0337b6baea6"_uuid;
    };

    template <>
    struct resource_traits<mesh>
    {
        static constexpr uuid uuid = "2248147b-7dce-41a3-a1d5-f72b6b19247c"_uuid;
    };

    template <>
    struct resource_traits<model>
    {
        static constexpr uuid uuid = "3f1881e4-db45-4f95-ae4d-9bacccf72c70"_uuid;
    };

    template <>
    struct resource_traits<texture>
    {
        static constexpr uuid uuid = "e98645c8-a9b5-4316-80b5-2558bcbd167c"_uuid;
    };
}