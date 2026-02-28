#include "reflection_worker.hpp"

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>

namespace oblo::gen
{
    oblo::expected<> oblo::gen::reflection_worker::generate(
        const cstring_view sourceFile, const cstring_view outputFile, const target_data& target)
    {
        reset();

        // First check if we already have a file, we will check if the hash of it matches to decide whether or not to
        // write to it, since any update might cause a rebuild
        bool hasOldContentHash{};
        hash_type oldContentHash{};

        if (const auto e = filesystem::load_text_file_into_memory(m_content, outputFile))
        {
            hasOldContentHash = true;
            oldContentHash = hashed_string_view{e->data(), e->size()}.hash();
        }

        // Reset again after reading the old file, we are going to write the new content now
        reset();

        m_content.append(R"(
#include <oblo/reflection/registration/registrant.hpp>

#include <oblo/core/uuid.hpp>
#include <oblo/reflection/attributes/color.hpp>
#include <oblo/reflection/attributes/range.hpp>
#include <oblo/reflection/concepts/gpu_component.hpp>
#include <oblo/reflection/concepts/pretty_name.hpp>
#include <oblo/reflection/concepts/resource_type.hpp>
#include <oblo/reflection/tags/ecs.hpp>
#include <oblo/reflection/tags/script_api.hpp>
#include <oblo/reflection/tags/serialization.hpp>
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

        if (hasOldContentHash)
        {
            const auto newContentHash = m_content.as<hashed_string_view>().hash();

            if (newContentHash == oldContentHash)
            {
                // Nothing to do, the output matches. This way we avoid rebuilding reflection files to often.
                return no_error;
            }
        }

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
        m_content.append("{");

        indent();
        new_line();

        m_content.append("auto&& classBuilder = reg.add_class<");
        m_content.append(r.name);
        m_content.append(">();");

        new_line();

        for (auto& field : r.fields)
        {
            m_content.append("classBuilder.add_field(&");
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

            if (field.flags.contains(field_flags::clamp_min))
            {
                indent();
                new_line();
                m_content.format(".add_attribute<::oblo::reflection::clamp_min>(double{{{}}})",
                    t.numberAttributeData[field.attrClampMin]);
                deindent();
            }

            if (field.flags.contains(field_flags::clamp_max))
            {
                indent();
                new_line();
                m_content.format(".add_attribute<::oblo::reflection::clamp_max>(double{{{}}})",
                    t.numberAttributeData[field.attrClampMax]);
                deindent();
            }

            m_content.append(";");

            new_line();
        }

        m_content.append("classBuilder.add_ranged_type_erasure();");
        new_line();

        bool addPrettyName = false;

        if (r.flags.contains(record_flags::ecs_component))
        {
            m_content.append("classBuilder.add_tag<::oblo::ecs::component_type_tag>();");
            new_line();

            addPrettyName = true;
        }

        if (r.flags.contains(record_flags::ecs_tag))
        {
            m_content.append("classBuilder.add_tag<::oblo::ecs::tag_type_tag>();");
            new_line();

            addPrettyName = true;
        }

        if (r.flags.contains(record_flags::script_api))
        {
            m_content.append("classBuilder.add_tag<::oblo::reflection::script_api>();");
            new_line();

            addPrettyName = true;
        }

        if (r.flags.contains(record_flags::transient))
        {
            m_content.append("classBuilder.add_tag<::oblo::reflection::transient_type_tag>();");
            new_line();
        }

        if (r.flags.contains(record_flags::uuid) && r.attrUuid >= 0)
        {
            m_content.format("classBuilder.add_concept<::oblo::uuid>(\"{}\"_uuid);", t.stringAttributeData[r.attrUuid]);
            new_line();

            addPrettyName = true;
        }

        if (addPrettyName)
        {
            m_content.format("classBuilder.add_concept<::oblo::reflection::pretty_name>({{ \"{}\" }});", r.identifier);
            new_line();
        }

        if (r.attrGpuComponent >= 0)
        {
            m_content.append("classBuilder.add_concept(::oblo::reflection::gpu_component{.bufferName = \"");
            m_content.append(t.stringAttributeData[r.attrGpuComponent]);
            m_content.append("\"_hsv});");
        }

        deindent();
        new_line();

        m_content.append("}");

        new_line();

        if (r.flags.contains(record_flags::resource))
        {
            m_content.format(
                R"(reg.add_class<resource_ref<{0}>>().add_concept(::oblo::reflection::resource_type{{.typeId = get_type_id<{0}>(), .typeUuid = ::oblo::resource_type<{0}>,}}).add_field(&resource_ref<{0}>::id, "id");)",
                r.name);

            new_line();
        }
    }

    void reflection_worker::generate_enum(const enum_type& e)
    {
        m_content.append("{");

        indent();
        new_line();

        m_content.append("auto&& enumBuilder = reg.add_enum<");
        m_content.append(e.name);
        m_content.append(">();");

        new_line();

        for (auto& enumerator : e.enumerators)
        {
            m_content.append("enumBuilder.add_enumerator(\"");
            m_content.append(enumerator);
            m_content.append("\", ");
            m_content.append(e.name);
            m_content.append("::");
            m_content.append(enumerator);
            m_content.append(");");

            new_line();
        }

        m_content.append(";");

        deindent();
        new_line();

        m_content.append("}");
        new_line();
    }
}
