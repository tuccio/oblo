#pragma once

namespace oblo::editor
{
    class viewport final
    {
    public:
        viewport() = default;
        viewport(const viewport&) = delete;
        viewport(viewport&&) noexcept = delete;

        viewport& operator=(const viewport&) = delete;
        viewport& operator=(viewport&&) noexcept = delete;

        bool update();
    };
}