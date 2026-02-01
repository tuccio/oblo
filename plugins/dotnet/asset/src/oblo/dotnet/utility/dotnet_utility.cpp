#include <oblo/dotnet/utility/dotnet_utility.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/platform/core.hpp>

namespace oblo::dotnet_utility
{
    expected<> generate_csproj(cstring_view path)
    {
        constexpr string_view targetFramework = "net9.0";

        string_builder mainExe;
        string_builder managedHint;

        if (platform::get_main_executable_path(mainExe))
        {
            filesystem::parent_path(mainExe.view(), managedHint);
        }
        else
        {
            filesystem::current_path(managedHint);
        }

        managedHint.append_path("managed").append_path("Oblo.Managed.dll");

        string_builder content;

        content.format(R"(<Project Sdk="Microsoft.NET.Sdk">

    <PropertyGroup>
        <TargetFramework>{0}</TargetFramework>
        <ImplicitUsings>enable</ImplicitUsings>
        <Nullable>enable</Nullable>
    </PropertyGroup>

    <ItemGroup>
        <Reference Include="Oblo.Managed">
            <HintPath>{1}</HintPath>
            <Private>true</Private>
        </Reference>
    </ItemGroup>

</Project>)",
            targetFramework,
            managedHint);

        return filesystem::write_file(path, as_bytes(std::span{content}), {});
    }

    expected<> find_cs_files(cstring_view directory, function_ref<void(cstring_view)> cb)
    {
        // TODO: This should be recursive, but possibly we don't support subdirs in asset sources

        return filesystem::walk(directory,
            [pathBuilder = string_builder{}, &cb](const filesystem::walk_entry& e) mutable
            {
                if (e.is_regular_file())
                {
                    e.append_filename(pathBuilder.clear());

                    if (pathBuilder.view().ends_with(".cs"))
                    {
                        cb(pathBuilder);
                    }
                }

                return filesystem::walk_result::walk;
            });
    }
}