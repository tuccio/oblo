#include <oblo/editor/windows/inspector.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/overload.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/utility/entity_utility.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/visit.hpp>

#include <imgui.h>

namespace oblo::editor
{
    namespace
    {
        void build_quaternion_editor(const property_node& node, std::byte* const data)
        {
            auto* const q = new (data) quaternion;
            auto [z, y, x] = quaternion::to_euler_zyx_intrinsic(degrees_tag{}, *q);

            bool anyChange{false};

            ImGui::PushID(int(hash_mix(node.offset, 0)));
            anyChange |= ImGui::DragFloat("x", &x, 0.1f);

            ImGui::PushID(int(hash_mix(node.offset, 1)));
            anyChange |= ImGui::DragFloat("y", &y, 0.1f);

            ImGui::PushID(int(hash_mix(node.offset, 2)));
            anyChange |= ImGui::DragFloat("z", &z, 0.1f);

            if (anyChange)
            {
                *q = quaternion::from_euler_zyx_intrinsic(degrees_tag{}, {z, y, x});
            }
        }

        void build_property_grid(const property_tree& tree, std::byte* const data)
        {
            auto* ptr = data;

            visit(tree,
                overload{
                    [&ptr, data](const property_node& node, const property_node_start)
                    {
                        ptr += node.offset;
                        ImGui::TextUnformatted(node.name.c_str());

                        if (node.type == get_type_id<quaternion>())
                        {
                            build_quaternion_editor(node, data);
                            return property_visit_result::sibling;
                        }

                        return property_visit_result::recurse;
                    },
                    [&ptr](const property_node& node, const property_node_finish) { ptr -= node.offset; },
                    [&ptr](const property& property)
                    {
                        switch (property.kind)
                        {
                        case property_kind::f32:
                            ImGui::PushID(int(hash_mix(property.offset, property.parent)));
                            ImGui::DragFloat(property.name.c_str(),
                                reinterpret_cast<float*>(ptr + property.offset),
                                0.1f);
                            break;

                        default:
                            ImGui::TextUnformatted(property.name.c_str());
                            break;
                        }

                        return property_visit_result::recurse;
                    },
                });
        }
    }

    void inspector::init(const window_update_context& ctx)
    {
        m_propertyRegistry = ctx.services.find<property_registry>();
        m_registry = ctx.services.find<ecs::entity_registry>();
        m_selection = ctx.services.find<selected_entities>();
    }

    bool inspector::update(const window_update_context&)
    {
        bool open{true};

        if (ImGui::Begin("Inspector", &open))
        {
            const std::span selectedEntities = m_selection->get();

            for (const auto e : selectedEntities)
            {
                if (e && m_registry->contains(e))
                {
                    auto* const name = entity_utility::get_name_cstr(*m_registry, e);
                    ImGui::TextUnformatted(name);

                    const auto& typeRegistry = m_registry->get_type_registry();
                    const std::span components = m_registry->get_component_types(e);

                    for (const ecs::component_type type : components)
                    {
                        const auto& desc = typeRegistry.get_component_type_desc(type);

                        char name[128];
                        const auto length = min<usize>(array_size(name) - 1, desc.type.name.size());
                        std::memcpy(name, desc.type.name.data(), length);

                        name[length] = '\0';

                        if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto* const propertyTree = m_propertyRegistry->try_get(desc.type);

                            if (propertyTree)
                            {
                                auto* const data = m_registry->try_get(e, type);

                                build_property_grid(*propertyTree, data);
                            }
                        }
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