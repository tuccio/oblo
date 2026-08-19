#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/tags/script_api.hpp>

namespace oblo::script_api
{
    using script_function_fn = void (*)(void);

    template <typename Fn>
    void for_each_script_function(const reflection::reflection_registry& registry, Fn&& fn)
    {
        deque<reflection::function_handle> functions;
        registry.find_by_tag<reflection::script_api>(functions);

        for (const auto handle : functions)
        {
            const auto data = registry.get_function_data(handle);

            // We only support void(void) functions for now
            if (data.returnType != get_type_id<void>() || !data.parameterTypes.empty())
            {
                continue;
            }

            if (!fn(data.fullyQualifiedName, data.functionPtr))
            {
                break;
            }
        }
    }
}