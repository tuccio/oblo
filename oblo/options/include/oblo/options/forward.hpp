#pragma once

namespace oblo
{
    template <usize N>
    struct fixed_string;

    template <fixed_string>
    class option_proxy;

    template <fixed_string>
    struct option_traits;
}