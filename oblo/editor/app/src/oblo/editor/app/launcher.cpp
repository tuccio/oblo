#include <oblo/editor/app/launcher.hpp>

#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/string/string_builder.hpp>

#include <cxxopts.hpp>

#ifdef _WIN32
    #include <Windows.h>

    #include <Shlobj.h>
#endif

namespace oblo
{
    std::istream& operator>>(std::istream& is, string_builder& s);
}

namespace oblo::editor
{
    namespace
    {
        constexpr string_view default_project_name = "DefaultProject";
        constexpr string_view project_file_name = "project.oproject";

#if defined(_WIN32)
        expected<string_builder> get_default_app_dir()
        {
            wchar_t appdata[MAX_PATH];

            if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata) != S_OK)
            {
                return "Failed to retrieve default application directory"_err;
            }

            return appdata;
        }
#elif defined(__linux__)
        expected<string_builder> get_default_app_dir()
        {
            string_builder appDir;

            if (!platform::read_environment_variable(appDir, "HOME"))
            {
                return "Failed to get HOME directory"_err;
            }

            appDir.append_path(".config");

            return appDir;
        }
#endif

        expected<> create_or_load_default_project(run_config& inOutConfig)
        {
            string_builder defaultProjectDir;
            string_builder defaultProjectPath;

            defaultProjectDir.assign(inOutConfig.appDir).append_path("projects").append_path(default_project_name);
            defaultProjectPath.assign(defaultProjectDir).append_path(project_file_name);

            inOutConfig.projectPath = defaultProjectPath;

            if (filesystem::exists(inOutConfig.projectPath).value_or(false))
            {
                return no_error;
            }

            filesystem::create_directories(defaultProjectDir.view()).assert_value();

            constexpr string_view default_assets_dir = "assets";
            constexpr string_view default_sources_dir = "sources";
            constexpr string_view default_artifacts_dir = ".artifacts";

            string_builder newDir;

            for (const string_view defaultDir : {
                     default_assets_dir,
                     default_sources_dir,
                     default_artifacts_dir,
                 })
            {
                newDir.assign(defaultProjectDir).append_path(defaultDir);

                if (!filesystem::create_directories(newDir.view()))
                {
                    return "Failed to create directories for default project"_err;
                }
            }

            const project p{
                .name = string{default_project_name},
                .assetsDir = string{default_assets_dir},
                .artifactsDir = string{default_artifacts_dir},
                .sourcesDir = string(default_sources_dir),
            };

            return project_save(p, defaultProjectPath);
        }
    }

    launcher::launcher() = default;

    expected<> launcher::run(int argc, char* argv[], run_config& outConfig)
    {
        bool hasProject = false;

        try
        {
            cxxopts::Options options("oblo");

            options.add_options()("project", "The path to the project file", cxxopts::value<string_builder>());

            auto r = options.parse(argc, argv);

            if (r.count("project"))
            {
                auto projectPath = r["project"].as<string_builder>();

                string_builder absoluteProjecPath;

                if (filesystem::absolute(projectPath.view(), absoluteProjecPath))
                {
                    outConfig.projectPath = absoluteProjecPath;
                }
                else
                {
                    outConfig.projectPath = projectPath.as<string>();
                }

                hasProject = true;
            }
        }
        catch (...)
        {
            return "Failed to parse options"_err;
        }

        auto appDir = get_default_app_dir();

        if (!appDir)
        {
            return appDir.error();
        }

        constexpr string_view appName = "oblo";
        appDir->append_path(appName);

        if (!filesystem::create_directories(appDir->view()) && !filesystem::exists(appDir->view()).value_or(false))
        {
            return "Application directory does not exist"_err;
        }

        outConfig.appDir = *appDir;

        if (!hasProject)
        {
            return create_or_load_default_project(outConfig);
        }

        return no_error;
    }
}

namespace oblo
{
    std::istream& operator>>(std::istream& is, string_builder& s)
    {
        for (int c = is.get(); c != EOF; c = is.get())
        {
            s.append(char(c));
        }

        is.clear();

        return is;
    }
}