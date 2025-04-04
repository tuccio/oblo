#include <clang-c/Index.h>

#include <oblo/core/deque.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/file_ptr.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/unique_ptr.hpp>

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>

#include <format>

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

    struct record_type
    {
        string_view name;
    };

    struct target_data
    {
        deque<record_type> recordTypes;
    };

    class codegen_worker
    {
    public:
        codegen_worker()
        {
            m_index = clang_createIndex(0, 0);
        }

        codegen_worker(const codegen_worker&) = delete;
        codegen_worker(codegen_worker&&) noexcept = delete;

        codegen_worker& operator=(const codegen_worker&) = delete;
        codegen_worker& operator=(codegen_worker&&) noexcept = delete;

        ~codegen_worker()
        {
            if (m_index)
            {
                clang_disposeIndex(m_index);
                m_index = {};
            }
        }

        void process(cstring_view sourceFile, cstring_view /*outputFile*/, const dynamic_array<const char*> args)
        {
            const CXTranslationUnit tu = clang_parseTranslationUnit(m_index,
                sourceFile.c_str(),
                args.data(),
                int(args.size()),
                nullptr,
                0,
                CXTranslationUnit_SkipFunctionBodies);

            if (!tu)
            {
                report_error("Failed to parse file {}", sourceFile);
                return;
            }

            const CXCursor rootCursor = clang_getTranslationUnitCursor(tu);
            clang_visitChildren(rootCursor, visit_recursive, this);

            clang_disposeTranslationUnit(tu);
        }

    private:
        static CXChildVisitResult visit_recursive(CXCursor cursor, CXCursor, CXClientData userdata)
        {
            [[maybe_unused]] auto* const self = static_cast<codegen_worker*>(userdata);

            if (clang_isCursorDefinition(cursor) && cursor.kind == CXCursor_ClassDecl ||
                cursor.kind == CXCursor_StructDecl)
            {
                string_builder fullyQualifiedName;
                build_fully_qualified_name(fullyQualifiedName, cursor);

                report_error("Found type: {}", fullyQualifiedName);
            }

            return CXChildVisit_Recurse;
        }

        static void build_fully_qualified_name(string_builder& out, CXCursor cursor)
        {
            if (cursor.kind == CXCursor_TranslationUnit)
            {
                return;
            }

            build_fully_qualified_name(out, clang_getCursorSemanticParent(cursor));

            const auto name = clang_getCursorDisplayName(cursor);

            out.append("::");
            out.append(clang_getCString(name));

            clang_disposeString(name);
        }

    private:
        CXIndex m_index{};
    };
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

    oblo::codegen_worker worker;
    oblo::dynamic_array<const char*> clangArguments;
    clangArguments.reserve(1024);

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

        clangArguments.clear();

        for (auto&& include : includesIt->value.GetArray())
        {
            clangArguments.emplace_back(include.GetString());
        }

        for (auto&& define : definesIt->value.GetArray())
        {
            clangArguments.emplace_back(define.GetString());
        }

        worker.process(sourceFileIt->value.GetString(), outputFileIt->value.GetString(), clangArguments);
    }

    return 0;
}