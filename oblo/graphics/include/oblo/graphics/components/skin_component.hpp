#pragma once

#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    struct skin;

    struct skin_component
    {
        resource_ref<skin> skin;
    } OBLO_COMPONENT("3a8fe516-db72-4619-8145-1512b04c25ef", ScriptAPI);
}