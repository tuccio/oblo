#pragma once

#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/reflection/registration/registrant.hpp>

namespace oblo::reflection
{
    template <typename F>
    void load_module_and_register(F&& f)
    {
        auto& mm = module_manager::get();

        auto* reflection = mm.load<reflection::reflection_module>();

        auto&& registrant = reflection->get_registrant();
        f(registrant);
    }
}