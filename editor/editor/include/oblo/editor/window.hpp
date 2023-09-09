#pragma once

namespace oblo::editor
{
    class window
    {
    public:
        virtual ~window() = default;

        virtual bool update() = 0;
    };
}