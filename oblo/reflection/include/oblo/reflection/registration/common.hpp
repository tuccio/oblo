#pragma once

#include <oblo/reflection/registration/registrant.hpp>

namespace oblo::reflection
{
    void register_fundamental_types(reflection_registry::registrant& reg);
    void register_math_types(reflection_registry::registrant& reg);
}