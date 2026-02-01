#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/editor/app/run_config.hpp>

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

        expected<> init(const run_config& cfg);

        void shutdown();

        void run();

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}