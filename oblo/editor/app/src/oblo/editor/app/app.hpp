#pragma once

#include <oblo/core/unique_ptr.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/editor/data/time_stats.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/runtime/runtime.hpp>
#include <oblo/runtime/runtime_registry.hpp>
#include <oblo/thread/job_manager.hpp>
#include <oblo/vulkan/renderer.hpp>

namespace oblo::editor
{
    class app
    {
    public:
        app();
        app(const app&) = delete;
        app(app&&) noexcept = delete;

        app& operator=(const app&) = delete;
        app& operator=(app&&) noexcept = delete;

        ~app();

        bool init(int argc, char* argv[]);

        void shutdown();

        void run();

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}