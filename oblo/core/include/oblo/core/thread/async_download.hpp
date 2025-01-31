#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/types.hpp>

#include <span>

namespace oblo
{
    class allocator;

    class async_download;

    template <typename>
    class function_ref;

    class async_download_promise
    {
    public:
        async_download_promise();
        async_download_promise(const async_download_promise&) = delete;
        async_download_promise(async_download_promise&&) noexcept;

        async_download_promise& operator=(const async_download_promise&) = delete;
        async_download_promise& operator=(async_download_promise&&) noexcept;

        ~async_download_promise();

        void init(allocator* allocator);

        void set_data(std::span<const byte> bytes);
        void populate_data(function_ref<std::span<byte>(allocator*)> cb);

        void reset();

    private:
        struct control_block;

        friend class async_download;

    private:
        control_block* m_block = nullptr;
    };

    class async_download
    {
    public:
        enum class error : u8
        {
            uninitialized,
            not_ready,
            broken_promise,
        };

    public:
        async_download();
        async_download(const async_download_promise& promise);
        async_download(const async_download&) = delete;
        async_download(async_download&&) noexcept;

        async_download& operator=(const async_download&) = delete;
        async_download& operator=(async_download&&) noexcept;

        ~async_download();

        expected<std::span<const byte>, error> try_get_result() const;

        void reset();

    private:
        async_download_promise::control_block* m_block = nullptr;
    };
}