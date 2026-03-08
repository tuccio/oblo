#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    struct skeleton
    {
        struct joint;

        dynamic_array<joint> jointsHierarchy;
    } OBLO_RESOURCE();

    struct skeleton::joint
    {
        static constexpr u32 no_parent = ~0u;

        string name;
        u32 parentIndex;
        mat4 transform;
    };

    struct skin
    {
        resource_ref<skeleton> skeleton{};
        dynamic_array<mat4> invBindPoses;
        dynamic_array<string> jointNames;
    } OBLO_RESOURCE();
}