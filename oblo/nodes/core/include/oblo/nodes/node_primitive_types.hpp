#pragma once

#include <oblo/core/uuid.hpp>
#include <oblo/core/variant.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>

namespace oblo::nodes
{
    struct no_value
    {
    };

    using pin_value = variant<no_value, bool /*, i32, u32, f32, vec2, vec3, vec4, quaternion*/>;

    // Just for primitive types, e.g. in an animation graph Skeleton or Pose might be types, so this needs to be extensible
    template <typename T>
    static constexpr uuid pin_value_type_uuid = uuid{};

    template <>
    constexpr uuid pin_value_type_uuid<bool> = "884832af-d1a2-4e26-beab-253784e3f983"_uuid;
}