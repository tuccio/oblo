#pragma once

#include <oblo/core/string/cstring_view.hpp>

namespace oblo::platform
{
    class shared_library
    {
    public:
        shared_library() = default;
        shared_library(const shared_library&) = delete;
        shared_library(shared_library&&) noexcept;
        explicit shared_library(cstring_view path);

        shared_library& operator=(const shared_library&) = delete;
        shared_library& operator=(shared_library&&) noexcept;

        ~shared_library();

        bool open(cstring_view path);

        void close();

        bool is_valid() const;
        explicit operator bool() const;

        void* symbol(const char* name) const;

    private:
        void* m_handle{};
    };
}