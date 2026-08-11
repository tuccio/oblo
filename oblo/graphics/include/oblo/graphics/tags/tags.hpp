#pragma once

#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    // Entities marked with this tag are skipped by the picking render pass, so they can never be picked.
    struct picking_excluded_tag
    {
    } OBLO_TAG("6aa868a4-f30a-4e8f-83bb-440cea981450", Transient);
}
