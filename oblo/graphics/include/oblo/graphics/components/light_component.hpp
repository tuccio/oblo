#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/angle.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    enum class light_type : u8
    {
        point,
        spot,
        directional,
    } OBLO_ENUM();

    struct light_component
    {
        light_type type;

        OBLO_PROPERTY(LinearColor)
        vec3 color;

        f32 intensity;

        OBLO_PROPERTY(ClampMin = 0)
        f32 radius;

        radians spotInnerAngle;
        radians spotOuterAngle;
        bool isShadowCaster;
        bool hardShadows;

        OBLO_PROPERTY(ClampMin = 0)
        f32 shadowBias;

        OBLO_PROPERTY(ClampMin = 0)
        f32 shadowPunctualRadius;

        OBLO_PROPERTY(ClampMin = 0)
        f32 shadowDepthSigma;

        // These should rather be global settings

        OBLO_PROPERTY(ClampMin = 0, ClampMax = 1)
        f32 shadowTemporalAccumulationFactor;

        OBLO_PROPERTY(ClampMin = 1)
        u32 shadowMeanFilterSize;

        OBLO_PROPERTY(ClampMin = 0)
        f32 shadowMeanFilterSigma;
    } OBLO_COMPONENT("50e1c754-3ffc-4898-a1ab-c7af4b77fd2b", ScriptAPI);
}