#pragma once

#include <oblo/core/any.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/modules/utility/provider_service.hpp>
#include <oblo/options/options_manager.hpp>

namespace oblo
{
    struct options_layer_provider_descriptor
    {
        using load_fn = function_ref<bool(data_document& doc, any& userdata)>;

        options_layer_descriptor layer{};
        load_fn load{};
        any userdata;
    };

    using options_layer_provider = provider_service<options_layer_provider_descriptor>;
    using options_provider = provider_service<option_descriptor>;

    template <typename F>
    using lambda_options_provider = lambda_provider_service<option_descriptor, F>;
}