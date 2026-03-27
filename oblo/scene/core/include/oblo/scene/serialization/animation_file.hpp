#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/forward.hpp>

namespace oblo
{
    struct animation;

    OBLO_SCENE_API expected<> save_animation(const animation& m, cstring_view destination);
    OBLO_SCENE_API expected<> load_animation(animation& m, cstring_view source);
}