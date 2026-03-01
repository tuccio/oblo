#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/forward.hpp>

namespace oblo
{
    struct skeleton;
    struct skin;

    OBLO_SCENE_API expected<> save_skeleton_json(const skeleton& sk, cstring_view destination);
    OBLO_SCENE_API expected<> load_skeleton(skeleton& sk, cstring_view source);

    OBLO_SCENE_API expected<> save_skin_json(const skin& sk, cstring_view destination);
    OBLO_SCENE_API expected<> load_skin(skin& sk, cstring_view source);
}