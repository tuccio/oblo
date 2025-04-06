#include "reflection_worker.hpp"

#include <oblo/core/filesystem/file.hpp>

namespace oblo::gen
{
    oblo::expected<> oblo::gen::reflection_worker::generate(
        const cstring_view sourceFile, const cstring_view outputFile, const target_data& target)
    {
        reset();

        m_content.append(R"(
#include <oblo/reflection/registration/registrant.hpp>

#include <oblo/reflection/attributes/color.hpp>
#include <oblo/reflection/concepts/gpu_component.hpp>
#include <oblo/reflection/tags/ecs.hpp>
)");
        new_line();

        m_content.append("#include \"");
        m_content.append(sourceFile);
        m_content.append("\"");
        new_line();

        new_line();

        m_content.append("namespace oblo::reflection::gen");
        new_line();

        m_content.append("{");
        indent();
        new_line();

        m_content.append("void register_");
        m_content.append(target.name);
        m_content.append("([[maybe_unused]] reflection_registry::registrant& reg)");

        new_line();
        m_content.append("{");

        indent();
        new_line();

        for (const auto& record : target.recordTypes)
        {
            generate_record(target, record);
        }

        for (const auto& enumType : target.enumTypes)
        {
            generate_enum(enumType);
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

    void reflection_worker::reset()
    {
        m_content.clear();
        m_content.reserve(1u << 14);
        m_indentation = 0;
    }

    void reflection_worker::new_line()
    {
        m_content.append('\n');

        for (i32 i = 0; i < m_indentation; ++i)
        {
            m_content.append("    ");
        }
    }

    void reflection_worker::indent(i32 i)
    {
        m_indentation += i;
    }

    void reflection_worker::deindent(i32 i)
    {
        m_indentation -= i;
    }

    void reflection_worker::generate_record(const target_data& t, const record_type& r)
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

            if (field.flags.contains(field_flags::linear_color))
            {
                indent();
                new_line();
                m_content.append(".add_attribute<::oblo::linear_color_tag>()");
                deindent();
            }

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

        if (r.attrGpuComponent >= 0)
        {
            m_content.append(".add_concept(::oblo::gpu_component{.bufferName = \"");
            m_content.append(t.stringAttributeData[r.attrGpuComponent]);
            m_content.append("\"_hsv})");
        }

        m_content.append(";");

        deindent();
        new_line();
    }

    void reflection_worker::generate_enum(const enum_type& e)
    {
        m_content.append("reg.add_enum<");
        m_content.append(e.name);
        m_content.append(">()");

        indent();
        new_line();

        for (auto& enumerator : e.enumerators)
        {
            m_content.append(".add_enumerator(\"");
            m_content.append(enumerator);
            m_content.append("\", ");
            m_content.append(e.name);
            m_content.append("::");
            m_content.append(enumerator);
            m_content.append(")");

            new_line();
        }

        m_content.append(";");

        deindent();
        new_line();
    }
}
