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
#include <oblo/editor/services/editor_world.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/ui/artifact_picker.hpp>
#include <oblo/editor/ui/property_table.hpp>
#include <oblo/editor/utility/entity_utility.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/math/mat4.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/properties/attributes.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/visit.hpp>
#include <oblo/reflection/concepts/resource_type.hpp>
#include <oblo/reflection/reflection_registry.hpp>

#include <IconsFontAwesome6.h>

#include <imgui.h>

namespace oblo
{
    struct linear_color_tag;
}

namespace oblo::editor
{
    struct inspector::string_buffer
    {
        char buffer[256];
    };

    namespace
    {
        struct inspector_context
        {
            const reflection::reflection_registry& reflection;
            ui::artifact_picker& artifactPicker;
            deque<inspector::string_buffer>& stringBuffers;
        };

        bool build_quaternion_editor(const property_node& node, std::byte* const data)
        {
            auto* const q = new (data) quaternion;
            return ui::property_table::add(int(hash_mix(node.offset, 0)), node.name, *q);
        }

        bool build_vec3_editor(const property_node& node, std::byte* const data)
        {
            auto* const v = new (data) vec3;
            return ui::property_table::add(int(hash_mix(node.offset, 0)), node.name, *v);
        }

        bool build_linear_color_editor(const property_node& node, std::byte* const data)
        {
            auto* const v = new (data) vec3;
            return ui::property_table::add_color(int(hash_mix(node.offset, 0)), node.name, *v);
        }

        bool build_property_table(const inspector_context& ctx, const property_tree& tree, std::byte* const data)
        {
            bool modified = false;

            auto* ptr = data;

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            usize nextStringBufferIdx = 0;

            if (ui::property_table::begin())
            {
                visit(tree,
                    overload{
                        [&ptr, &tree, &modified](const property_node& node, const property_node_start)
                        {
                            ptr += node.offset;

                            if (node.type == get_type_id<vec3>())
                            {
                                if (find_attribute<linear_color_tag>(tree, node))
                                {
                                    modified |= build_linear_color_editor(node, ptr);
                                    return visit_result::sibling;
                                }

                                modified |= build_vec3_editor(node, ptr);
                                return visit_result::sibling;
                            }

                            if (node.type == get_type_id<quaternion>())
                            {
                                modified |= build_quaternion_editor(node, ptr);
                                return visit_result::sibling;
                            }

                            if (node.type == get_type_id<mat4>())
                            {
                                auto* const v = new (ptr) mat4;
                                modified |= ui::property_table::add(int(hash_mix(node.offset, 0)), node.name, *v);
                                return visit_result::sibling;
                            }

                            if (node.type == get_type_id<radians>())
                            {
                                auto* const r = new (ptr) radians;
                                modified |= ui::property_table::add(int(hash_mix(node.offset, 0)), node.name, *r);
                                return visit_result::sibling;
                            }

                            if (node.type == get_type_id<degrees>())
                            {
                                auto* const r = new (ptr) degrees;
                                modified |= ui::property_table::add(int(hash_mix(node.offset, 0)), node.name, *r);
                                return visit_result::sibling;
                            }

                            return visit_result::recurse;
                        },
                        [&ptr](const property_node& node, const property_node_finish) { ptr -= node.offset; },
                        [](const property_node&, const property_array&, auto&&) { return visit_result::sibling; },
                        [&ptr, &ctx, &tree, &modified, &nextStringBufferIdx](const property& property)
                        {
                            const auto makeId = [&property]
                            { return (int(hash_mix(property.offset, property.parent))); };

                            byte* const propertyPtr = ptr + property.offset;

                            if (property.isEnum)
                            {
                                modified |= ui::property_table::add_enum(makeId(),
                                    property.name,
                                    propertyPtr,
                                    property.type,
                                    ctx.reflection);

                                return visit_result::recurse;
                            }

                            switch (property.kind)
                            {
                            case property_kind::f32:
                                modified |= ui::property_table::add(makeId(), property.name, *new (propertyPtr) f32);
                                break;

                            case property_kind::u32:
                                modified |= ui::property_table::add(makeId(), property.name, *new (propertyPtr) u32);
                                break;

                            case property_kind::boolean:
                                modified |= ui::property_table::add(makeId(), property.name, *new (propertyPtr) bool);
                                break;

                            case property_kind::uuid: {
                                const auto parentType = ctx.reflection.find_type(tree.nodes[property.parent].type);

                                if (const auto resourceRef =
                                        ctx.reflection.find_concept<reflection::resource_type>(parentType))
                                {
                                    modified |= ui::property_table::add(makeId(),
                                        tree.nodes[property.parent].name,
                                        *new (propertyPtr) uuid,
                                        ctx.artifactPicker,
                                        resourceRef->typeUuid);
                                }
                                else
                                {
                                    modified |=
                                        ui::property_table::add(makeId(), property.name, *new (propertyPtr) uuid);
                                }
                            }

                            break;

                            case property_kind::string: {
                                property_value_wrapper srcWrapper;
                                srcWrapper.assign_from(property.kind, propertyPtr);
                                const auto src = srcWrapper.get_string();

                                if (nextStringBufferIdx >= ctx.stringBuffers.size())
                                {
                                    ctx.stringBuffers.resize(nextStringBufferIdx + 1);
                                }

                                auto& stringBuffer = ctx.stringBuffers[nextStringBufferIdx];
                                ++nextStringBufferIdx;

                                const auto numChars = min(src.size(), sizeof(inspector::string_buffer) - 1);
                                std::memcpy(stringBuffer.buffer, src.data(), numChars);
                                stringBuffer.buffer[numChars] = '\0';

                                modified |= ui::property_table::add_input_text(makeId(),
                                    property.name,
                                    stringBuffer.buffer,
                                    sizeof(inspector::string_buffer));

                                if (modified)
                                {
                                    property_value_wrapper inputTextWrapper{string_view{stringBuffer.buffer}};
                                    inputTextWrapper.assign_to(property.kind, propertyPtr);
                                }

                                break;
                            }

                            default:
                                ui::property_table::add_empty(property.name);
                                break;
                            }

                            return visit_result::recurse;
                        },
                    });

                ui::property_table::end();
            }

            return modified;
        }
    }

    inspector::inspector() = default;

    inspector::~inspector() = default;

    void inspector::init(const window_update_context& ctx)
    {
        m_propertyRegistry = ctx.services.find<const property_registry>();
        m_reflection = ctx.services.find<const reflection::reflection_registry>();
        m_editorWorld = ctx.services.find<const editor_world>();
        m_factory = ctx.services.find<component_factory>();

        auto* assetRegistry = ctx.services.find<asset_registry>();
        m_artifactPicker = allocate_unique<ui::artifact_picker>(*assetRegistry);
    }

    bool inspector::update(const window_update_context&)
    {
        string_builder builder;

        bool open{true};

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());

        if (ImGui::Begin("Inspector", &open))
        {
            auto* const selectionService = m_editorWorld->get_selected_entities();
            const std::span selectedEntities = selectionService->get();

            auto* const entityRegistry = m_editorWorld->get_entity_registry();

            const auto& typeRegistry = entityRegistry->get_type_registry();

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
                            m_factory->add(*entityRegistry, e, type);
                        }
                    }
                }

                ImGui::EndCombo();
            }

            for (const auto e : selectedEntities)
            {
                if (e && entityRegistry->contains(e))
                {
                    const f32 availableWidth = ImGui::GetContentRegionAvail().x;

                    auto* const entityName = entity_utility::get_name_cstr(*entityRegistry, e);
                    ImGui::TextUnformatted(entityName);

                    builder.clear().format("[Entity id: {}]", e.value);

                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

                    ImGui::SameLine();

                    ImGui::SetCursorPosX(availableWidth - ImGui::CalcTextSize(builder.begin(), builder.end()).x);

                    ImGui::TextUnformatted(builder.c_str());

                    ImGui::PopStyleColor();

                    const std::span components = entityRegistry->get_component_types(e);

                    const inspector_context inspectorContext = {
                        .reflection = *m_reflection,
                        .artifactPicker = *m_artifactPicker,
                        .stringBuffers = m_stringBuffers,
                    };

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

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
                                auto& typeDesc = entityRegistry->get_type_registry().get_component_type_desc(type);

                                byte* ptr;
                                entityRegistry->get(e, {&type, 1}, {&ptr, 1});

                                typeDesc.destroy(ptr, 1);
                                typeDesc.create(ptr, 1);

                                entityRegistry->notify(e);
                            }

                            if (ImGui::MenuItem("Delete"))
                            {
                                ecs::component_and_tag_sets types{};
                                types.components.add(type);

                                entityRegistry->remove(e, types);

                                wasDeleted = true;
                            }

                            ImGui::EndPopup();
                        }

                        if (isComponentExpanded && !wasDeleted)
                        {
                            auto* const propertyTree = m_propertyRegistry->try_get(desc.type);

                            if (propertyTree)
                            {
                                auto* const data = entityRegistry->try_get(e, type);

                                ImGui::PushID(int(type.value));

                                if (build_property_table(inspectorContext, *propertyTree, data))
                                {
                                    entityRegistry->notify(e);
                                }

                                ImGui::PopID();
                            }
                        }

                        ImGui::PopID();
                    }

                    ImGui::PopStyleVar(1);

                    // Just pick the first entity for now
                    break;
                }
            }

            ImGui::PopStyleVar(1);

            ImGui::End();
        }

        return open;
    }
}