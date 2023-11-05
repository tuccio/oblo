#include <oblo/editor/windows/inspector.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/overload.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/utility/entity_utility.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/visit.hpp>

#include <imgui.h>

namespace oblo::editor
{
    namespace
    {
        void build_property_grid(const property_tree& tree)
        {
            // TODO
            visit(tree,
                overload{
                    [](const property_node& node, const property_node_start)
                    {
                        ImGui::TextUnformatted(node.name.c_str());
                        return property_visit_result::recurse;
                    },
                    [](const property_node&, const property_node_finish) {},
                    [](const property& property)
                    {
                        ImGui::TextUnformatted(property.name.c_str());
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

                        if (ImGui::CollapsingHeader(name))
                        {
                            auto* const propertyTree = m_propertyRegistry->try_get(desc.type);

                            if (propertyTree)
                            {
                                build_property_grid(*propertyTree);
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