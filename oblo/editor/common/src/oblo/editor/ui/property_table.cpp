#include <oblo/editor/ui/property_table.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/editor/ui/artifact_picker.hpp>
#include <oblo/editor/ui/widgets.hpp>
#include <oblo/math/angle.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/reflection/reflection_registry.hpp>

namespace oblo::editor::ui
{
    namespace
    {
        void setup_property_width()
        {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        }

        void setup_property(string_view name)
        {
            constexpr f32 rowHeight = 28.f;

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);

            ImGui::Dummy({0, rowHeight});
            ImGui::SameLine();

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (rowHeight - ImGui::GetTextLineHeight()) * .5f);

            ImGui::TextUnformatted(name.begin(), name.end());

            ImGui::TableSetColumnIndex(1);

            setup_property_width();
        }
    }

    bool property_table::begin()
    {
        if (ImGui::BeginTable("#property_table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            return true;
        }

        return false;
    }

    void property_table::end()
    {
        ImGui::EndTable();
    }

    void editor::ui::property_table::add_empty(string_view name)
    {
        setup_property(name);
    }

    bool property_table::add(id_t id, string_view name, bool& v)
    {
        setup_property(name);

        ImGui::PushID(id);
        const bool r = ImGui::Checkbox("", &v);
        ImGui::PopID();
        return r;
    }

    bool property_table::add(id_t id, string_view name, u32& v)
    {
        setup_property(name);

        ImGui::PushID(id);
        const bool r = ImGui::DragScalar("", ImGuiDataType_U32, &v);
        ImGui::PopID();
        return r;
    }

    bool property_table::add(id_t id, string_view name, f32& v)
    {
        setup_property(name);

        ImGui::PushID(id);
        const bool r = ImGui::DragFloat("", &v, 0.1f);
        ImGui::PopID();
        return r;
    }

    bool property_table::add(id_t id, string_view name, vec2& v)
    {
        setup_property(name);

        ImGui::PushID(id);
        const bool r = ui::dragfloat_n_xyzw("", &v.x, 2, .1f);
        ImGui::PopID();
        return r;
    }

    bool property_table::add(id_t id, string_view name, vec3& v)
    {
        setup_property(name);

        ImGui::PushID(id);
        const bool r = ui::dragfloat_n_xyzw("", &v.x, 3, .1f);
        ImGui::PopID();
        return r;
    }

    bool property_table::add(id_t id, string_view name, vec4& v)
    {
        setup_property(name);

        ImGui::PushID(id);
        const bool r = ui::dragfloat_n_xyzw("", &v.x, 4, .1f);
        ImGui::PopID();
        return r;
    }

    bool property_table::add(id_t id, string_view name, mat4& v)
    {
        setup_property(name);

        auto t = transpose(v);

        ImGui::PushID(id);

        bool modified = false;

        for (u32 i = 0; i < array_size(t.columns); ++i)
        {
            ImGui::PushID(i);
            setup_property_width();
            const bool r = ui::dragfloat_n_xyzw("", &t.columns[i].x, 4, .1f);
            modified |= r;
            ImGui::PopID();
        }

        ImGui::PopID();
        return modified;
    }

    bool property_table::add(id_t id, string_view name, quaternion& v)
    {
        setup_property(name);

        auto [z, y, x] = quaternion::to_euler_zyx_intrinsic(degrees_tag{}, v);

        float values[] = {x, y, z};

        bool anyChange{false};

        ImGui::PushID(id);
        anyChange |= ui::dragfloat_n_xyzw("", values, 3, .1f);
        ImGui::PopID();

        if (anyChange)
        {
            v = quaternion::from_euler_zyx_intrinsic(degrees_tag{}, {values[2], values[1], values[0]});
        }

        return anyChange;
    }

    bool property_table::add(id_t id, string_view name, degrees& v)
    {
        return add(id, name, v.value);
    }

    bool property_table::add(id_t id, string_view name, radians& v)
    {
        degrees d{v};
        const bool r = add(id, name, d);

        if (r)
        {
            v = radians{d};
        }

        return r;
    }

    bool property_table::add_color(id_t id, string_view name, vec3& v)
    {
        setup_property(name);

        ImGui::PushID(id);
        const bool r = ImGui::ColorEdit3("", &v.x);
        ImGui::PopID();
        return r;
    }

    bool property_table::add(id_t id, string_view name, uuid& anyUuid)
    {
        setup_property(name);

        constexpr u32 size = 36;
        char buf[size];
        anyUuid.format_to(buf);

        ImGui::PushID(id);
        ImGui::TextUnformatted(buf, buf + size);
        ImGui::PopID();

        return false;
    }

    bool property_table::add(id_t id, string_view name, uuid& artifactId, artifact_picker& picker, const uuid& typeUuid)
    {
        setup_property(name);

        if (picker.draw(id, typeUuid, artifactId))
        {
            artifactId = picker.get_current_ref();
            return true;
        }

        return false;
    }

    bool property_table::add_enum(
        id_t id, string_view name, void* v, const type_id& typeId, const reflection::reflection_registry& reflection)
    {
        bool modified = false;

        setup_property(name);

        const auto e = reflection.find_enum(typeId);

        if (e)
        {
            const auto names = reflection.get_enumerator_names(e);
            const auto values = reflection.get_enumerator_values(e);

            const u32 size = reflection.get_type_data(e).size;

            const char* preview = "<Undefined>";

            for (usize i = 0; i < names.size(); ++i)
            {
                const auto it = values.begin() + i * size;

                if (std::memcmp(&*it, v, size) == 0)
                {
                    preview = names[i].data();
                    break;
                }
            }

            ImGui::PushID(id);

            if (ImGui::BeginCombo("", preview))
            {
                for (usize i = 0; i < names.size(); ++i)
                {
                    bool selected{};

                    if (ImGui::Selectable(names[i].data(), &selected) && selected)
                    {
                        const auto it = values.begin() + i * size;
                        std::memcpy(v, &*it, size);
                        modified = true;
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::PopID();
        }

        return modified;
    }

    bool property_table::add_input_text(id_t id, string_view name, char* buffer, usize size)
    {
        setup_property(name);

        ImGui::PushID(id);
        const bool modified = ImGui::InputText("", buffer, size);
        ImGui::PopID();

        return modified;
    }
}