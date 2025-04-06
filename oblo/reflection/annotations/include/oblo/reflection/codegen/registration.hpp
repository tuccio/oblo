#pragma once

#include <oblo/core/preprocessor.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/registration/module_registration.hpp>

namespace oblo::reflection::gen
{
    void OBLO_CAT_EVAL(register_, OBLO_PROJECT_NAME)(reflection_registry::registrant& reg);
}

namespace oblo::reflection::gen
{
    namespace
    {
        void register_reflection(reflection_registry::registrant& reg)
        {
            OBLO_CAT_EVAL(register_, OBLO_PROJECT_NAME)(reg);
        }

        void load_module_and_register()
        {
            reflection::load_module_and_register(&register_reflection);
        }
    }
}
