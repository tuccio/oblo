#include <oblo/editor/services/editor_directories.hpp>

#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/uuid_generator.hpp>

namespace oblo::editor
{
    expected<> editor_directories::init(cstring_view temporaryDirectory)
    {
        m_temporaryDir = temporaryDirectory;

        filesystem::remove_all(temporaryDirectory).assert_value();

        if (!filesystem::create_directories(temporaryDirectory))
        {
            return unspecified_error;
        }

        return no_error;
    }

    expected<> editor_directories::create_temporary_directory(string_builder& outDir)
    {
        const auto uuid = uuid_system_generator{}.generate();

        outDir = m_temporaryDir;
        outDir.append_path_separator();
        outDir.format("{}", uuid);

        if (!filesystem::create_directories(outDir))
        {
            return unspecified_error;
        }

        return no_error;
    }
}