#pragma once

namespace oblo
{
    template <typename... F>
    struct overload : F...
    {
        using F::operator()...;
    };
}