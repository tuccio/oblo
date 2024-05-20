#pragma once

#include <filesystem>

namespace oblo::platform
{
    class shared_library
    {
    public:
        shared_library() = default;
        shared_library(const shared_library&) = delete;
        shared_library(shared_library&&) noexcept;
        explicit shared_library(const std::filesystem::path& path);

        shared_library& operator=(const shared_library&) = delete;
        shared_library& operator=(shared_library&&) noexcept;

        ~shared_library();

        bool open(const std::filesystem::path& path);
        void close();

        bool is_valid() const;
        explicit operator bool() const;

        void* symbol(const char* name) const;

    private:
        void* m_handle{};
    };
}