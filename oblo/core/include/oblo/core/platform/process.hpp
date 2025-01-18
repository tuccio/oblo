#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>

#include <cstdio>
#include <span>

namespace oblo::platform
{
    class file;

    struct process_descriptor
    {
        cstring_view path;
        std::span<const cstring_view> arguments;
        const file* inputStream{};
        const file* outputStream{};
        const file* errorStream{};
    };

    class process
    {
    public:
        process();
        process(const process&) = delete;
        process(process&&) noexcept;

        ~process();

        process& operator=(const process&) = delete;
        process& operator=(process&&) noexcept;

        expected<> start(const process_descriptor& desc);

        bool is_done();
        expected<> wait();

        expected<i64> get_exit_code();
        void detach();

    private:
#if WIN32
        void* m_hProcess{};
#endif
    };
}