#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
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

        DOTNET_RT_API void* load_assembly_delegate(
            cstring_view assemblyPath, cstring_view assemblyType, cstring_view methodName) const;

        template <typename T>
            requires std::is_function_v<T>
        std::add_pointer_t<T> load_assembly_delegate(
            cstring_view assemblyPath, cstring_view assemblyType, cstring_view methodName) const
        {
            return std::add_pointer_t<T>(load_assembly_delegate(assemblyPath, assemblyType, methodName));
        }

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}