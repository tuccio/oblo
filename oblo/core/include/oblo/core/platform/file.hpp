#pragma once

#include <oblo/core/expected.hpp>

namespace oblo::platform
{
    class file
    {
    public:
        static expected<> create_pipe(file& readPipe, file& writePipe, u32 bufferSizeHint);

    public:
        enum class error
        {
            eof,
            unspecified
        };

    public:
        file() noexcept;
        file(const file&) = delete;
        file(file&&) noexcept;

        ~file();

        file& operator=(const file&) = delete;
        file& operator=(file&&) noexcept;

        expected<u32, error> read(void* dst, u32 size) const noexcept;
        expected<u32, error> write(const void* src, u32 size) const noexcept;

        bool is_open() const noexcept;

        void close() noexcept;

        explicit operator bool() const noexcept;

        void* get_native_handle() const noexcept;

    private:
#if WIN32
        void* m_handle{};
#endif
    };
}