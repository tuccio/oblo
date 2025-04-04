#include <clang-c/Index.h>

#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
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

    struct field_type
    {
        string name;
    };

    struct record_type
    {
        string_builder name;
        deque<field_type> fields;
    };

    struct target_data
    {
        deque<record_type> recordTypes;
    };

    class clang_worker
    {
    public:
        clang_worker()
        {
            m_index = clang_createIndex(0, 0);
        }

        clang_worker(const clang_worker&) = delete;
        clang_worker(clang_worker&&) noexcept = delete;

        clang_worker& operator=(const clang_worker&) = delete;
        clang_worker& operator=(clang_worker&&) noexcept = delete;

        ~clang_worker()
        {
            if (m_index)
            {
                clang_disposeIndex(m_index);
                m_index = {};
            }
        }

        expected<target_data> parse_code(cstring_view sourceFile, const dynamic_array<const char*> args)
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
                return unspecified_error;
            }

            target_data targetData;

            const CXCursor rootCursor = clang_getTranslationUnitCursor(tu);
            clang_visitChildren(rootCursor, visit_translation_unit, &targetData);

            clang_disposeTranslationUnit(tu);

            return targetData;
        }

    private:
        class clang_string
        {
        public:
            clang_string() : m_string{} {}

            explicit clang_string(CXString str)
            {
                m_string = str;
            }

            clang_string(const clang_string&) = delete;

            clang_string(clang_string&& other) noexcept
            {
                m_string = other.m_string;
                other.m_string = {};
            }

            clang_string& operator=(const clang_string&) = delete;

            clang_string& operator=(clang_string&& other) noexcept
            {
                if (m_string.data)
                {
                    clang_disposeString(m_string);
                }

                m_string = other.m_string;
                other.m_string = {};

                return *this;
            }

            ~clang_string()
            {
                if (m_string.data)
                {
                    clang_disposeString(m_string);
                }
            }

            explicit operator bool() const noexcept
            {
                return m_string.data != nullptr;
            }

            cstring_view view() const noexcept
            {
                return cstring_view{clang_getCString(m_string)};
            }

        private:
            CXString m_string;
        };

        static CXChildVisitResult visit_translation_unit(CXCursor cursor, CXCursor, CXClientData userdata)
        {
            target_data& targetReflection = *reinterpret_cast<target_data*>(userdata);

            if (clang_isCursorDefinition(cursor) && cursor.kind == CXCursor_ClassDecl ||
                cursor.kind == CXCursor_StructDecl)
            {
                // TODO: We should determine whether the definition belongs to the project (i.e. is the file it's
                // defined in within the project directory?)

                clang_string annotation{};

                clang_visitChildren(cursor, find_annotation, &annotation);

                if (annotation)
                {
                    auto& recordType = targetReflection.recordTypes.emplace_back();

                    build_fully_qualified_name(recordType.name, cursor);

                    // We may want to parse the annotation for some metadata
                    clang_visitChildren(cursor, add_fields, &recordType);
                }
            }

            return CXChildVisit_Recurse;
        }

        static CXChildVisitResult find_annotation(CXCursor cursor, CXCursor, CXClientData userdata)
        {
            if (cursor.kind == CXCursor_AnnotateAttr)
            {
                clang_string spelling{clang_getCursorSpelling(cursor)};

                if (spelling.view().starts_with("_oblo_reflect"))
                {
                    // Returns the annotation to the caller
                    *reinterpret_cast<clang_string*>(userdata) = std::move(spelling);
                    return CXChildVisit_Break;
                }
            }

            // We only search among the direct children here
            return CXChildVisit_Continue;
        }

        static CXChildVisitResult add_fields(CXCursor cursor, CXCursor, CXClientData userdata)
        {
            if (cursor.kind == CXCursor_FieldDecl)
            {
                auto& recordType = *reinterpret_cast<oblo::record_type*>(userdata);

                const clang_string spelling{clang_getCursorSpelling(cursor)};
                auto& field = recordType.fields.emplace_back();
                field.name = spelling.view();
            }

            // We only search among the direct children here
            return CXChildVisit_Continue;
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

    oblo::clang_worker worker;
    oblo::dynamic_array<const char*> clangArguments;
    clangArguments.reserve(1024);

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

        clangArguments.clear();

        // Macro used to distinguish the codegen pass in annotation macros
        clangArguments.emplace_back("-DOBLO_CODEGEN");

        for (auto&& include : includesIt->value.GetArray())
        {
            clangArguments.emplace_back(include.GetString());
        }

        for (auto&& define : definesIt->value.GetArray())
        {
            clangArguments.emplace_back(define.GetString());
        }

        const auto result = worker.parse_code(sourceFileIt->value.GetString(), clangArguments);

        if (!result)
        {
            ++errors;
            continue;
        }

        // TODO: Generate reflection code and output
    }

    return 0;
}