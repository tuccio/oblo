#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/unique_ptr.hpp>

namespace oblo
{
    class dotnet_runtime
    {
    public:
        DOTNET_RT_API dotnet_runtime();

        dotnet_runtime(const dotnet_runtime&) = delete;
        dotnet_runtime(dotnet_runtime&&) noexcept = delete;

        DOTNET_RT_API ~dotnet_runtime();

        dotnet_runtime& operator=(const dotnet_runtime&) = delete;
        dotnet_runtime& operator=(dotnet_runtime&&) noexcept = delete;

        DOTNET_RT_API expected<> init();

        DOTNET_RT_API void shutdown();

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}