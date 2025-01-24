#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/unique_ptr.hpp>

namespace oblo::editor
{
    class window_module
    {
    public:
        virtual ~window_module() = default;

        virtual void init() = 0;
        virtual void update() = 0;
    };

    class window_modules_provider
    {
    public:
        virtual ~window_modules_provider() = default;

        virtual void fetch_window_modules(dynamic_array<unique_ptr<window_module>>& outWindowModules) const = 0;
    };
}