#pragma once

#include <oblo/resource/resource_traits.hpp>

namespace oblo
{
    namespace script
    {
        class compiled_script;
    }

    template <>
    struct resource_traits<script::compiled_script>
    {
        static constexpr uuid uuid = "2dec7e1c-b030-4077-8d0e-7dc656bd3564"_uuid;
    };
}