#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    struct skeleton
    {
        struct joint;

        dynamic_array<joint> jointsHierarchy;
    };

    struct skeleton::joint
    {
        static constexpr u32 no_parent = ~0u;

        string name;

        u32 parentIndex;

        vec3 translation;
        quaternion rotation;
        vec3 scale;
    };

    struct skin
    {
        resource_ref<skeleton> skeleton{};
        dynamic_array<mat4> invBindPoses;
        dynamic_array<string> jointNames;
    };
}