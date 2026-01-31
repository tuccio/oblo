#include "clang_worker.hpp"

#include <oblo/core/string/string_builder.hpp>

#include <charconv>

namespace oblo::gen
{
    namespace
    {
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

        void build_fully_qualified_name(string_builder& out, CXCursor cursor)
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

        CXChildVisitResult find_annotation(CXCursor cursor, CXCursor, CXClientData userdata)
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

        enum class annotation_property_result
        {
            expect_none,
            expect_number,
            expect_string,
        };

        annotation_property_result process_annotation_property(
            record_type& r, hashed_string_view property, i32** outIdx)
        {
            if (property == "Component"_hsv)
            {
                r.flags.set(record_flags::ecs_component);
                return annotation_property_result::expect_none;
            }

            if (property == "Tag"_hsv)
            {
                r.flags.set(record_flags::ecs_tag);
                return annotation_property_result::expect_none;
            }

            if (property == "Resource"_hsv)
            {
                r.flags.set(record_flags::resource);
                return annotation_property_result::expect_none;
            }

            if (property == "GpuComponent"_hsv)
            {
                *outIdx = &r.attrGpuComponent;
                return annotation_property_result::expect_string;
            }

            if (property == "ScriptAPI"_hsv)
            {
                r.flags.set(record_flags::script_api);
                return annotation_property_result::expect_none;
            }

            if (property == "Transient"_hsv)
            {
                r.flags.set(record_flags::transient);
                return annotation_property_result::expect_none;
            }

            if (property == "UUID"_hsv)
            {
                r.flags.set(record_flags::uuid);
                *outIdx = &r.attrUuid;
                return annotation_property_result::expect_string;
            }

            return annotation_property_result::expect_none;
        }

        annotation_property_result process_annotation_property(field_type& f, hashed_string_view property, i32** outIdx)
        {
            if (property == "ClampMin"_hsv)
            {
                f.flags.set(field_flags::clamp_min);
                *outIdx = &f.attrClampMin;
                return annotation_property_result::expect_number;
            }

            if (property == "ClampMax"_hsv)
            {
                f.flags.set(field_flags::clamp_max);
                *outIdx = &f.attrClampMax;
                return annotation_property_result::expect_number;
            }

            if (property == "LinearColor"_hsv)
            {
                f.flags.set(field_flags::linear_color);
                return annotation_property_result::expect_none;
            }

            return annotation_property_result::expect_none;
        }

        template <typename T>
        void parse_annotation(target_data& t, T& r, string_view annotation)
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

                i32* expectIdx = nullptr;

                const auto expectResult = process_annotation_property(r, property, &expectIdx);

                if (expectIdx)
                {
                    *expectIdx = -1;
                }

                if (e == annotation.end())
                {
                    break;
                }

                it = e + 1;

                if (expectResult == annotation_property_result::expect_string && expectIdx)
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

                    *expectIdx = narrow_cast<i32>(t.stringAttributeData.size());
                    t.stringAttributeData.emplace_back(string_view{stringBegin, narrow_cast<usize>(it - stringBegin)});

                    ++it;
                }
                else if (expectResult == annotation_property_result::expect_number && expectIdx)
                {
                    while ((std::isspace(*it) || *it == '=') && it != annotation.end())
                    {
                        ++it;
                    }

                    double number;

                    const auto [end, ec] = std::from_chars(it, annotation.end(), number);

                    if (ec == std::errc{})
                    {
                        *expectIdx = narrow_cast<i32>(t.numberAttributeData.size());
                        t.numberAttributeData.emplace_back(number);
                    }

                    if (end == annotation.end())
                    {
                        break;
                    }

                    it = end + 1;
                }
            }
        }

        struct add_field_ctx
        {
            target_data* target;
            record_type* record;
        };

        CXChildVisitResult add_fields(CXCursor cursor, CXCursor, CXClientData userdata)
        {
            if (cursor.kind == CXCursor_FieldDecl && clang_getCXXAccessSpecifier(cursor) == CX_CXXPublic)
            {
                auto& ctx = *reinterpret_cast<add_field_ctx*>(userdata);

                const clang_string spelling{clang_getCursorSpelling(cursor)};
                auto& field = ctx.record->fields.emplace_back();
                field.name = spelling.view();

                clang_string annotation{};
                clang_visitChildren(cursor, find_annotation, &annotation);

                if (annotation)
                {
                    parse_annotation(*ctx.target, field, annotation.view());
                }
            }

            // We only search among the direct children here
            return CXChildVisit_Continue;
        }

        CXChildVisitResult add_enumerators(CXCursor cursor, CXCursor, CXClientData userdata)
        {
            if (cursor.kind == CXCursor_EnumConstantDecl)
            {
                auto& enumType = *reinterpret_cast<enum_type*>(userdata);

                const clang_string spelling{clang_getCursorSpelling(cursor)};
                enumType.enumerators.emplace_back(spelling.view());
            }

            // We only search among the direct children here
            return CXChildVisit_Continue;
        }

        CXChildVisitResult visit_translation_unit(CXCursor cursor, CXCursor, CXClientData userdata)
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
                    parse_annotation(targetReflection, recordType, annotation.view());

                    string_builder fullyQualifiedName;
                    build_fully_qualified_name(fullyQualifiedName, cursor);

                    recordType.name = fullyQualifiedName;

                    const auto displayName = clang_getCursorDisplayName(cursor);
                    recordType.identifier = clang_getCString(displayName);

                    add_field_ctx ctx{.target = &targetReflection, .record = &recordType};

                    clang_visitChildren(cursor, add_fields, &ctx);
                }
            }
            else if (clang_isCursorDefinition(cursor) && cursor.kind == CXCursor_EnumDecl)
            {
                // TODO: We should determine whether the definition belongs to the project (i.e. is the file it's
                // defined in within the project directory?)

                clang_string annotation{};
                clang_visitChildren(cursor, find_annotation, &annotation);

                if (annotation)
                {
                    auto& enumType = targetReflection.enumTypes.emplace_back();

                    string_builder fullyQualifiedName;
                    build_fully_qualified_name(fullyQualifiedName, cursor);

                    enumType.name = fullyQualifiedName;

                    clang_visitChildren(cursor, add_enumerators, &enumType);
                }
            }

            return CXChildVisit_Recurse;
        }
    }

    clang_worker::clang_worker()
    {
        m_index = clang_createIndex(0, 0);
    }

    clang_worker::~clang_worker()
    {
        if (m_index)
        {
            clang_disposeIndex(m_index);
            m_index = {};
        }
    }

    expected<target_data> clang_worker::parse_code(cstring_view sourceFile, const dynamic_array<const char*> args)
    {
        m_errors.clear();

        const CXTranslationUnit tu = clang_parseTranslationUnit(m_index,
            sourceFile.c_str(),
            args.data(),
            int(args.size()),
            nullptr,
            0,
            CXTranslationUnit_SkipFunctionBodies);

        if (!tu)
        {
            return "Code generation failed"_err;
        }

        if (const u32 numErrors = clang_getNumDiagnostics(tu))
        {
            bool hasErrors = false;

            u32 displayOptions = clang_defaultDiagnosticDisplayOptions();

            for (u32 i = 0; i < numErrors; ++i)
            {
                const CXDiagnostic diag = clang_getDiagnostic(tu, i);

                if (clang_getDiagnosticSeverity(diag) >= CXDiagnostic_Error)
                {
                    const clang_string str{clang_formatDiagnostic(diag, displayOptions)};

                    m_errors.append(str.view());
                    m_errors.append('\n');

                    hasErrors = true;
                }

                clang_disposeDiagnostic(diag);
            }

            if (hasErrors)
            {
                return "Code generation failed"_err;
            }
        }

        target_data targetData;

        const CXCursor rootCursor = clang_getTranslationUnitCursor(tu);
        clang_visitChildren(rootCursor, visit_translation_unit, &targetData);

        clang_disposeTranslationUnit(tu);

        return targetData;
    }

    cstring_view clang_worker::get_errors() const
    {
        return m_errors.view();
    }
}