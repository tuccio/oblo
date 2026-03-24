#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    struct animation;

    enum class animation_status : u8
    {
        pause,
        play,
    } OBLO_ENUM();

    struct animation_component
    {
        resource_ref<animation> animation;
        bool loop;
        animation_status statusOnLoad;
    } OBLO_COMPONENT("0366568e-b3e2-4989-a6bc-35f3987e9049", ScriptAPI);

    struct animation_progress_component
    {
        struct joint_animation
        {
            enum class property : u8
            {
                translation,
                rotation,
                scale
            };

            string_view jointName;
            property target;

            union {
                vec3 translation;
                quaternion rotation;
                vec3 scale;
            };
        };

        resource_ptr<animation> animationPtr;
        i64 progressHns;
        dynamic_array<joint_animation> jointAnimations;
        animation_status currentStatus;
    } OBLO_COMPONENT("a4e465b0-8dfb-49a7-93bc-7c96864d2571", Transient);
}