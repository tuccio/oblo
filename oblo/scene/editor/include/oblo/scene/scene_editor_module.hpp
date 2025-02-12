#pragma once

#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class scene_editor_module final : public module_interface
    {
    public:
        SCENE_EDITOR_API bool startup(const module_initializer& initializer) override;
        SCENE_EDITOR_API void shutdown() override;
        SCENE_EDITOR_API bool finalize() override;
    };
}