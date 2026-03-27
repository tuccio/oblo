#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    struct skeleton
    {
        struct joint;

        dynamic_array<joint> jointsHierarchy;
    } OBLO_RESOURCE();

    using skeleton_joint_index_t = u16;

    struct skeleton::joint
    {
        static constexpr skeleton_joint_index_t no_parent = ~skeleton_joint_index_t{};

        string name;
        skeleton_joint_index_t parentIndex;

        vec3 translation;
        quaternion rotation;
        vec3 scale;
    };

    struct skin
    {
        resource_ref<skeleton> skeleton{};
        dynamic_array<mat4> invBindPoses;
        dynamic_array<string> jointNames;
    } OBLO_RESOURCE();
}