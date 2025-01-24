#pragma once

namespace oblo
{
    class cstring_view;
    struct model;

    SCENE_API bool save_model_json(const model& m, cstring_view destination);
    SCENE_API bool load_model(model& m, cstring_view source);
}