#include <clang-c/Index.h>

#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/file_ptr.hpp>
#include <oblo/core/flags.hpp>
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

    enum class record_flags : u8
    {
        ecs_component,
        ecs_tag,
        enum_max,
    };

    struct record_type
    {
        string_builder name;
        flags<record_flags> flags;
        deque<field_type> fields;

        string attrGpuComponent;
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
                    parse_annotation(recordType, annotation.view());

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

        static void parse_annotation(record_type& r, string_view annotation)
        {
            for (auto it = annotation.begin(); it != annotation.end();)
            {
                // Trim on the left
                while (std::isspace(*it) && it != annotation.end())
                {
                    ++it;
                }

                if (it == annotation.end())
                {
                    break;
                }

                auto e = it;

                // Get the whole property name
                while (!std::isspace(*e) && *e != ',' && *e != '=' && e != annotation.end())
                {
                    ++e;
                }

                const hashed_string_view property{it, narrow_cast<usize>(e - it)};

                string* expectString = {};

                if (property == "Component"_hsv)
                {
                    r.flags.set(record_flags::ecs_component);
                }
                else if (property == "Tag"_hsv)
                {
                    r.flags.set(record_flags::ecs_tag);
                }
                else if (property == "GpuComponent"_hsv)
                {
                    expectString = &r.attrGpuComponent;
                }

                if (e == annotation.end())
                {
                    break;
                }

                it = e + 1;

                if (expectString)
                {
                    while (*it != '"' && it != annotation.end())
                    {
                        ++it;
                    }

                    if (it == annotation.end())
                    {
                        break;
                    }

                    const auto stringBegin = ++it;

                    // We might need to consider escapes here, but it's unsupported for now
                    while (*it != '"' && it != annotation.end())
                    {
                        ++it;
                    }

                    if (it == annotation.end())
                    {
                        break;
                    }

                    *expectString = string_view{stringBegin, narrow_cast<usize>(it - stringBegin)};

                    ++it;
                }
            }
        }

    private:
        CXIndex m_index{};
    };

    class reflection_worker
    {
    public:
        oblo::expected<> generate(
            const cstring_view sourceFile, const cstring_view outputFile, const target_data& target)
        {
            reset();

            m_content.append(R"(
#include <oblo/reflection/registration/registrant.hpp>

// TODO: Move this somewhere else
#include <oblo/scene/reflection/gpu_component.hpp>
)");
            new_line();

            m_content.append("#include \"");
            m_content.append(sourceFile);
            m_content.append("\"");
            new_line();

            new_line();

            generate_forward_declarations();

            m_content.append("namespace oblo::reflection::gen");
            new_line();

            m_content.append("{");
            indent();
            new_line();

            m_content.append("void register_reflection([[maybe_unused]] reflection_registry::registrant& reg)");

            new_line();
            m_content.append("{");

            indent();
            new_line();

            for (const auto& record : target.recordTypes)
            {
                generate_record(record);
            }

            deindent();
            new_line();

            new_line();
            m_content.append("}");

            deindent();
            new_line();
            m_content.append("}");
            new_line();

            return filesystem::write_file(outputFile, as_bytes(std::span{m_content}), {});
        }

    private:
        void reset()
        {
            m_content.clear();
            m_content.reserve(1u << 14);
            m_indentation = 0;
        }

        void new_line()
        {
            m_content.append('\n');

            for (i32 i = 0; i < m_indentation; ++i)
            {
                m_content.append("    ");
            }
        }

        void indent(i32 i = 1)
        {
            m_indentation += i;
        }

        void deindent(i32 i = 1)
        {
            m_indentation -= i;
        }

        void generate_forward_declarations()
        {
            m_content.append(R"(
namespace oblo::ecs
{
    struct component_type_tag;
    struct tag_type_tag;
}
)");

            new_line();
        }

        void generate_record(const record_type& r)
        {
            m_content.append("reg.add_class<");
            m_content.append(r.name);
            m_content.append(">()");

            indent();
            new_line();

            for (auto& field : r.fields)
            {
                m_content.append(".add_field(&");
                m_content.append(r.name);
                m_content.append("::");
                m_content.append(field.name);
                m_content.append(", \"");
                m_content.append(field.name);
                m_content.append("\")");

                new_line();
            }

            m_content.append(".add_ranged_type_erasure()");
            new_line();

            if (r.flags.contains(record_flags::ecs_component))
            {
                m_content.append(".add_tag<::oblo::ecs::component_type_tag>()");
                new_line();
            }

            if (r.flags.contains(record_flags::ecs_tag))
            {
                m_content.append(".add_tag<::oblo::ecs::tag_type_tag>()");
                new_line();
            }

            if (!r.attrGpuComponent.empty())
            {
                m_content.append(".add_concept(::oblo::gpu_component{.bufferName = \"");
                m_content.append(r.attrGpuComponent);
                m_content.append("\"_hsv})");
            }

            m_content.append(";");

            deindent();
            new_line();
        }

    private:
        string_builder m_content;
        i32 m_indentation{};
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

    struct thread_context
    {
        oblo::clang_worker parser;
        oblo::reflection_worker generator;
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
            oblo::report_error("Failed to parse file {}", sourceFile);
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