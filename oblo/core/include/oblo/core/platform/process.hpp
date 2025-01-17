#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>

#include <span>

namespace oblo::platform
{
    class process
    {
    public:
        process();
        process(const process&) = delete;
        process(process&&) noexcept;

        ~process();

        process& operator=(const process&) = delete;
        process& operator=(process&&) noexcept;

        expected<> start(cstring_view path, std::span<const cstring_view> arguments);

        expected<> wait();

        expected<i64> get_exit_code();
        void detach();

    private:
        uintptr m_handles[2]{};
    };
}