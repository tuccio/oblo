#include <oblo/editor/windows/inspector.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/overload.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/tag_type_desc.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/component_factory.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/ui/widgets.hpp>
#include <oblo/editor/utility/entity_utility.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/properties/attributes.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/visit.hpp>
#include <oblo/reflection/reflection_registry.hpp>

#include <format>

#include <IconsFontAwesome6.h>

#include <imgui.h>

namespace oblo
{
    struct linear_color_tag;
}

namespace oblo::editor
{
    namespace
    {
        void build_quaternion_editor(const property_node& node, std::byte* const data)
        {
            auto* const q = new (data) quaternion;
            auto [z, y, x] = quaternion::to_euler_zyx_intrinsic(degrees_tag{}, *q);

            float values[] = {x, y, z};

            bool anyChange{false};

            ImGui::PushID(int(hash_mix(node.offset, 0)));
            anyChange |= ui::dragfloat_n_xyz(node.name.c_str(), values, 3, .1f);
            ImGui::PopID();

            if (anyChange)
            {
                *q = quaternion::from_euler_zyx_intrinsic(degrees_tag{}, {values[2], values[1], values[0]});
            }
        }

        void build_vec3_editor(const property_node& node, std::byte* const data)
        {
            auto* const v = new (data) vec3;

            ImGui::PushID(int(hash_mix(node.offset, 0)));
            ui::dragfloat_n_xyz(node.name.c_str(), &v->x, 3, .1f);
            ImGui::PopID();
        }

        void build_linear_color_editor(const property_node& node, std::byte* const data)
        {
            auto* const v = new (data) vec3;
            ImGui::ColorEdit3(node.name.c_str(), &v->x);
        }

        void build_property_grid(
            const reflection::reflection_registry& refl, const property_tree& tree, std::byte* const data)
        {
            auto* ptr = data;

            visit(tree,
                overload{
                    [&ptr, &tree](const property_node& node, const property_node_start)
                    {
                        ptr += node.offset;

                        if (node.type == get_type_id<vec3>())
                        {
                            if (find_attribute<linear_color_tag>(tree, node))
                            {
                                build_linear_color_editor(node, ptr);
                                return visit_result::sibling;
                            }

                            build_vec3_editor(node, ptr);
                            return visit_result::sibling;
                        }

                        ImGui::TextUnformatted(node.name.c_str());

                        if (node.type == get_type_id<quaternion>())
                        {
                            build_quaternion_editor(node, ptr);
                            return visit_result::sibling;
                        }

                        return visit_result::recurse;
                    },
                    [&ptr](const property_node& node, const property_node_finish) { ptr -= node.offset; },
                    [](const property_node&, const property_array&, auto&&) { return visit_result::sibling; },
                    [&ptr, &refl](const property& property)
                    {
                        const auto makeId = [&property] { return (int(hash_mix(property.offset, property.parent))); };

                        byte* const propertyPtr = ptr + property.offset;

                        if (property.isEnum)
                        {
                            const auto e = refl.find_enum(property.type);

                            if (e)
                            {
                                const auto names = refl.get_enumerator_names(e);
                                const auto values = refl.get_enumerator_values(e);

                                const u32 size = refl.get_type_data(e).size;

                                const char* preview = "<Undefined>";

                                for (usize i = 0; i < names.size(); ++i)
                                {
                                    const auto it = values.begin() + i * size;

                                    if (std::equal(it, it + size, propertyPtr))
                                    {
                                        preview = names[i].data();
                                        break;
                                    }
                                }

                                ImGui::PushID(makeId());

                                if (ImGui::BeginCombo(property.name.c_str(), preview))
                                {
                                    for (usize i = 0; i < names.size(); ++i)
                                    {
                                        bool selected{};

                                        if (ImGui::Selectable(names[i].data(), &selected) && selected)
                                        {
                                            const auto it = values.begin() + i * size;
                                            std::memcpy(propertyPtr, &*it, size);
                                        }
                                    }

                                    ImGui::EndCombo();
                                }

                                ImGui::PopID();

                                return visit_result::recurse;
                            }
                        }

                        switch (property.kind)
                        {
                        case property_kind::f32:
                            ImGui::PushID(makeId());
                            ImGui::DragFloat(property.name.c_str(), reinterpret_cast<float*>(propertyPtr), 0.1f);
                            ImGui::PopID();
                            break;

                        case property_kind::u32:
                            ImGui::PushID(makeId());
                            ImGui::DragScalar(property.name.c_str(), ImGuiDataType_U32, propertyPtr);
                            ImGui::PopID();
                            break;

                        case property_kind::boolean:
                            ImGui::PushID(makeId());
                            ImGui::Checkbox(property.name.c_str(), reinterpret_cast<bool*>(propertyPtr));
                            ImGui::PopID();
                            break;

                        case property_kind::uuid:
                            ImGui::PushID(makeId());

                            {
                                char buf[36];
                                const auto& v = *reinterpret_cast<uuid*>(propertyPtr);
                                v.format_to(buf);

                                ImGui::TextUnformatted(buf, buf + array_size(buf));
                            }

                            ImGui::PopID();
                            break;

                        default:
                            ImGui::TextUnformatted(property.name.c_str());
                            break;
                        }

                        return visit_result::recurse;
                    },
                });
        }
    }

    void inspector::init(const window_update_context& ctx)
    {
        m_propertyRegistry = ctx.services.find<property_registry>();
        m_reflection = ctx.services.find<const reflection::reflection_registry>();
        m_registry = ctx.services.find<ecs::entity_registry>();
        m_selection = ctx.services.find<selected_entities>();
        m_factory = ctx.services.find<component_factory>();
    }

    bool inspector::update(const window_update_context&)
    {
        string_builder builder;

        bool open{true};

        if (ImGui::Begin("Inspector", &open))
        {
            const std::span selectedEntities = m_selection->get();

            const auto& typeRegistry = m_registry->get_type_registry();

            ImGui::SetNextItemWidth(ImGui::GetWindowWidth());

            if (!selectedEntities.empty() && ImGui::BeginCombo("Add Component", nullptr, ImGuiComboFlags_NoPreview))
            {
                ecs::component_type type{};

                for (const auto& component : typeRegistry.get_component_types())
                {
                    ++type.value;

                    builder.clear().append(component.type.name);

                    if (ImGui::Selectable(builder.c_str()))
                    {
                        for (const auto e : selectedEntities)
                        {
                            m_factory->add(*m_registry, e, type);
                        }
                    }
                }

                ImGui::EndCombo();
            }

            for (const auto e : selectedEntities)
            {
                if (e && m_registry->contains(e))
                {
                    auto* const entityName = entity_utility::get_name_cstr(*m_registry, e);
                    ImGui::TextUnformatted(entityName);

                    builder.clear().format("[Entity id: {}]", e.value);

                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

                    ImGui::SameLine();
                    ImGui::TextUnformatted(builder.c_str());

                    ImGui::PopStyleColor();

                    const std::span components = m_registry->get_component_types(e);

                    for (const ecs::component_type type : components)
                    {
                        const auto& desc = typeRegistry.get_component_type_desc(type);

                        builder.clear().append(desc.type.name);

                        ImGui::PushID(static_cast<int>(type.value));

                        const auto isComponentExpanded = ImGui::CollapsingHeader(builder.c_str(),
                            ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_DefaultOpen);

                        const auto headerY = ImGui::GetItemRectMin().y;
                        const auto hamburgerX = ImGui::GetItemRectMax().x - 24;

                        ImGui::SetCursorScreenPos({hamburgerX, headerY});

                        bool wasDeleted{false};

                        ImGui::Button(ICON_FA_ELLIPSIS_VERTICAL);

                        if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft))
                        {
                            if (ImGui::MenuItem("Reset"))
                            {
                                auto& typeDesc = m_registry->get_type_registry().get_component_type_desc(type);

                                byte* ptr;
                                m_registry->get(e, {&type, 1}, {&ptr, 1});

                                typeDesc.destroy(ptr, 1);
                                typeDesc.create(ptr, 1);
                            }

                            if (ImGui::MenuItem("Delete"))
                            {
                                ecs::component_and_tag_sets types{};
                                types.components.add(type);

                                m_registry->remove(e, types);

                                wasDeleted = true;
                            }

                            ImGui::EndPopup();
                        }

                        if (isComponentExpanded && !wasDeleted)
                        {
                            auto* const propertyTree = m_propertyRegistry->try_get(desc.type);

                            if (propertyTree)
                            {
                                auto* const data = m_registry->try_get(e, type);

                                ImGui::PushID(int(type.value));
                                build_property_grid(*m_reflection, *propertyTree, data);
                                ImGui::PopID();
                            }
                        }

                        ImGui::PopID();
                    }

                    // Just pick the first entity for now
                    break;
                }
            }

            ImGui::End();
        }

        return open;
    }
}