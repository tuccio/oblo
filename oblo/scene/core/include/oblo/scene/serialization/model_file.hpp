#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/forward.hpp>
namespace oblo
{
    struct model;

    SCENE_API expected<> save_model_json(const model& m, cstring_view destination);
    SCENE_API expected<> load_model(model& m, cstring_view source);
}