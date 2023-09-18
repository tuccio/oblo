#pragma once

namespace oblo::editor
{
    class scene_hierarchy final
    {
    public:
        scene_hierarchy() = default;
        scene_hierarchy(const scene_hierarchy&) = delete;
        scene_hierarchy(scene_hierarchy&&) noexcept = delete;

        scene_hierarchy& operator=(const scene_hierarchy&) = delete;
        scene_hierarchy& operator=(scene_hierarchy&&) noexcept = delete;

        bool update();
    };
}