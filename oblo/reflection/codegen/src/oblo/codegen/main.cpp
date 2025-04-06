
#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/file_ptr.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>

#include "clang_worker.hpp"
#include "reflection_worker.hpp"
#include "target_data.hpp"

namespace oblo
{
    template <typename... Args>
    void report_error(std::format_string<Args...> fmt, Args&&... args)
    {
        string_builder b;
        b.format(fmt, std::forward<Args>(args)...);
        b.append('\n');
        std::fputs(b.c_str(), stderr);
    }
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        const char* appName = argc > 0 ? argv[0] : "ocodegen";
        oblo::report_error("Usage: {} <path to config file>", appName);
        return 1;
    }

    const char* configFile = argv[1];
    const auto file = oblo::filesystem::file_ptr{oblo::filesystem::open_file(configFile, "r")};

    if (!file)
    {
        oblo::report_error("Failed to read config file {}", configFile);
        return 1;
    }

    constexpr auto bufferSize{4096};
    char buffer[bufferSize];

    rapidjson::FileReadStream rs{file.get(), buffer, bufferSize};

    rapidjson::Document doc;
    doc.ParseStream(rs);

    if (doc.HasParseError() || !doc.IsArray())
    {
        oblo::report_error("Failed to parse config file {}", configFile);
        return 1;
    }

    struct thread_context
    {
        oblo::gen::clang_worker parser;
        oblo::gen::reflection_worker generator;
        oblo::dynamic_array<const char*> clangArguments;
    };

    thread_context ctx;
    ctx.clangArguments.reserve(1024);

    int errors = 0;

    for (auto&& target : doc.GetArray())
    {
        const auto sourceFileIt = target.FindMember("source_file");
        const auto outputFileIt = target.FindMember("output_file");
        const auto includesIt = target.FindMember("include_directories");
        const auto definesIt = target.FindMember("compile_definitions");

        if (sourceFileIt == target.MemberEnd() || outputFileIt == target.MemberEnd() ||
            includesIt == target.MemberEnd() || definesIt == target.MemberEnd())
        {
            oblo::report_error("Failed to parse target within file {}", configFile);
            continue;
        }

        ctx.clangArguments.clear();

        ctx.clangArguments.emplace_back("-std=c++20");

        // Macro used to distinguish the codegen pass in annotation macros
        ctx.clangArguments.emplace_back("-DOBLO_CODEGEN");

        for (auto&& include : includesIt->value.GetArray())
        {
            ctx.clangArguments.emplace_back(include.GetString());
        }

        for (auto&& define : definesIt->value.GetArray())
        {
            ctx.clangArguments.emplace_back(define.GetString());
        }

        const char* sourceFile = sourceFileIt->value.GetString();

        const auto parseResult = ctx.parser.parse_code(sourceFile, ctx.clangArguments);

        if (!parseResult)
        {
            const auto clangErrors = ctx.parser.get_errors();

            oblo::report_error("Failed to parse file {}", sourceFile);

            if (!clangErrors.empty())
            {
                oblo::report_error("{}", clangErrors);
            }

            ++errors;
            continue;
        }

        const char* outputFile = outputFileIt->value.GetString();

        const auto generateResult = ctx.generator.generate(sourceFile, outputFile, *parseResult);

        if (!generateResult)
        {
            oblo::report_error("Failed to generate file {}", outputFile);
            ++errors;
            continue;
        }
    }

    return 0;
}