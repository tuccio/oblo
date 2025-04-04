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
            generate_record(target, record);
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

    inline void reflection_worker::reset()
    {
        m_content.clear();
        m_content.reserve(1u << 14);
        m_indentation = 0;
    }

    inline void reflection_worker::new_line()
    {
        m_content.append('\n');

        for (i32 i = 0; i < m_indentation; ++i)
        {
            m_content.append("    ");
        }
    }

    inline void reflection_worker::indent(i32 i)
    {
        m_indentation += i;
    }

    inline void reflection_worker::deindent(i32 i)
    {
        m_indentation -= i;
    }

    inline void reflection_worker::generate_forward_declarations()
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

    inline void reflection_worker::generate_record(const target_data& t, const record_type& r)
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
}
