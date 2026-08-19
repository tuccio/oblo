#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/types.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/tags/script_api.hpp>

namespace oblo::script_api
{
    template <typename Fn>
    void for_each_script_function(const reflection::reflection_registry& registry, Fn&& fn)
    {
        deque<reflection::function_handle> functions;
        registry.find_by_tag<reflection::script_api>(functions);

        for (const auto handle : functions)
        {
            if (!fn(registry.get_function_data(handle)))
            {
                break;
            }
        }
    }
}