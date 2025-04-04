#include "clang_worker.hpp"

#include <oblo/core/string/string_builder.hpp>

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

        void parse_annotation(target_data& t, record_type& r, string_view annotation)
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

                i32* expectString = nullptr;

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

                    *expectString = narrow_cast<i32>(t.stringAttributeData.size());
                    t.stringAttributeData.emplace_back(string_view{stringBegin, narrow_cast<usize>(it - stringBegin)});

                    ++it;
                }
            }
        }

        CXChildVisitResult add_fields(CXCursor cursor, CXCursor, CXClientData userdata)
        {
            if (cursor.kind == CXCursor_FieldDecl)
            {
                auto& recordType = *reinterpret_cast<record_type*>(userdata);

                const clang_string spelling{clang_getCursorSpelling(cursor)};
                auto& field = recordType.fields.emplace_back();
                field.name = spelling.view();
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

                    string_builder fullyQualifiedName;
                    build_fully_qualified_name(fullyQualifiedName, cursor);

                    recordType.name = fullyQualifiedName;
                    parse_annotation(targetReflection, recordType, annotation.view());

                    // We may want to parse the annotation for some metadata
                    clang_visitChildren(cursor, add_fields, &recordType);
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
}