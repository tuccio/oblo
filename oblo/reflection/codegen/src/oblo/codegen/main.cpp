
#include <oblo/core/array_size.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/print.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>

#include "clang_worker.hpp"
#include "reflection_worker.hpp"
#include "target_data.hpp"

int main(int argc, char* argv[])
{
    using namespace oblo;

    constexpr int firstClangArgIdx = 4;

    if (argc < firstClangArgIdx)
    {
        const char* appName = argc > 0 ? argv[0] : "ocodegen";
        print_line("Usage: {} <target name> <source> <output> [clang args ...]", appName);
        return 1;
    }

    const char* const target = argv[1];
    const char* const sourceFile = argv[2];
    const char* const outputFile = argv[3];

    gen::clang_worker parser;
    gen::reflection_worker generator;

    print_line("Parsing {}", target);

    auto parseResult = parser.parse_code(sourceFile, {argv + firstClangArgIdx, argv + argc});

    if (!parseResult)
    {
        const auto clangErrors = parser.get_errors();

        print_line("Failed to parse file {}", sourceFile);

        if (!clangErrors.empty())
        {
            print_line("{}", clangErrors);
        }

        return 1;
    }

    parseResult->name = target;

    const auto generateResult = generator.generate(sourceFile, outputFile, *parseResult);

    if (!generateResult)
    {
        print_line("Failed to generate file {}", outputFile);
        return 1;
    }

    print_line("Code generation finished successfully for {}", target);

    return 0;
}