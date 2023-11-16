#include <filesystem>

namespace oblo
{
    class data_document;
}

namespace oblo::json
{
    bool read(data_document& doc, const std::filesystem::path& source);

    bool write(const data_document& doc, const std::filesystem::path& destination);
}