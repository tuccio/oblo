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
#include <oblo/editor/utility/data_inspector.hpp>
#include <oblo/editor/utility/entity_utility.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <IconsFontAwesome6.h>

#include <imgui.h>

namespace oblo::editor
{
    struct inspector::data_inspector_ctx
    {
        explicit data_inspector_ctx(const reflection::reflection_registry& reflection, asset_registry& assetRegistry) :
            picker{assetRegistry}
        {
            inspector.init(&reflection, &picker);
        }

        ui::artifact_picker picker;
        data_inspector inspector;
    };

    inspector::inspector() = default;

    inspector::~inspector() = default;

    bool inspector::init(const window_update_context& ctx)
    {
        m_propertyRegistry = ctx.services.find<const property_registry>();
        m_reflection = ctx.services.find<const reflection::reflection_registry>();
        m_editorWorld = ctx.services.find<const editor_world>();
        m_factory = ctx.services.find<component_factory>();

        auto* assetRegistry = ctx.services.find<asset_registry>();

        if (!m_propertyRegistry || !m_reflection || !m_editorWorld || !m_factory || !assetRegistry)
        {
            return false;
        }

        m_ctx = allocate_unique<data_inspector_ctx>(*m_reflection, *assetRegistry);

        return true;
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

            m_ctx->inspector.begin();

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

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

                    for (const ecs::component_type type : components)
                    {
                        const auto& desc = typeRegistry.get_component_type_desc(type);

                        builder.clear().append(desc.type.name);

                        ImGui::PushID(static_cast<int>(type.value));

                        const auto isComponentExpanded = ImGui::CollapsingHeader(builder.c_str(),
                            ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen);

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

                                if (m_ctx->inspector.build_property_table(*propertyTree, data))
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

            m_ctx->inspector.end();

            ImGui::PopStyleVar(1);

            ImGui::End();
        }

        return open;
    }
}