#pragma once

#include <oblo/modules/utility/provider_service.hpp>
#include <oblo/options/options_manager.hpp>

namespace oblo
{
    using options_layer_provider = provider_service<options_layer_descriptor>;
    using options_provider = provider_service<option_descriptor>;

    template <typename F>
    using lambda_options_provider = lambda_provider_service<option_descriptor, F>;
}