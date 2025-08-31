#include <oblo/nodes/node_graph_registry.hpp>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_primitive_type.hpp>

namespace oblo
{
    node_graph_registry::node_graph_registry() = default;

    node_graph_registry::~node_graph_registry() = default;

    bool node_graph_registry::register_node(node_descriptor desc)
    {
        const auto [it, inserted] = m_descriptors.emplace(desc.id, std::move(desc));
        return inserted;
    }

    bool node_graph_registry::register_primitive_type(node_primitive_type desc)
    {
        const u32 kindIdx = u32(desc.kind);

        if (!m_primitiveKindToTypeId[kindIdx].is_nil())
        {
            return false;
        }

        const auto [it, inserted] = m_primitiveTypes.emplace(desc.id, std::move(desc));

        if (inserted)
        {
            m_primitiveKindToTypeId[kindIdx] = desc.id;
        }

        return inserted;
    }

    const node_descriptor* node_graph_registry::find_node(const uuid& id) const
    {
        const auto it = m_descriptors.find(id);
        return it == m_descriptors.end() ? nullptr : &it->second;
    }

    const node_primitive_type* node_graph_registry::find_primitive_type(const uuid& id) const
    {
        const auto it = m_primitiveTypes.find(id);
        return it == m_primitiveTypes.end() ? nullptr : &it->second;
    }

    void node_graph_registry::fetch_nodes(dynamic_array<const node_descriptor*>& outNodes) const
    {
        outNodes.reserve(outNodes.size() + m_descriptors.size());

        for (const auto& [id, desc] : m_descriptors)
        {
            outNodes.emplace_back(&desc);
        }
    }

    void node_graph_registry::fetch_primitive_types(dynamic_array<const node_primitive_type*>& outPrimitiveTypes) const
    {
        outPrimitiveTypes.reserve(outPrimitiveTypes.size() + m_primitiveTypes.size());

        for (const auto& [id, desc] : m_primitiveTypes)
        {
            outPrimitiveTypes.emplace_back(&desc);
        }
    }

    namespace
    {
        struct promotion_rules
        {
            static constexpr u32 N = u32(node_primitive_kind::enum_max);

            node_primitive_kind rule[N][N];

            consteval promotion_rules()
            {
                for (u32 i = 0; i < N; ++i)
                {
                    for (u32 j = 0; j < N; ++j)
                    {
                        rule[i][j] = i == j ? static_cast<node_primitive_kind>(i) : node_primitive_kind::enum_max;
                    }
                }

                rule[u32(node_primitive_kind::boolean)][u32(node_primitive_kind::i32)] = node_primitive_kind::i32;
                rule[u32(node_primitive_kind::i32)][u32(node_primitive_kind::boolean)] = node_primitive_kind::i32;

                rule[u32(node_primitive_kind::i32)][u32(node_primitive_kind::f32)] = node_primitive_kind::f32;
                rule[u32(node_primitive_kind::f32)][u32(node_primitive_kind::i32)] = node_primitive_kind::f32;
            }

            constexpr node_primitive_kind at(node_primitive_kind a, node_primitive_kind b) const
            {
                return rule[u32(a)][u32(b)];
            }
        };

        static constexpr promotion_rules g_PromotionRules;
    }

    uuid node_graph_registry::find_promotion_rule(const uuid& lhs, const uuid& rhs) const
    {
        const auto lhsIt = m_primitiveTypes.find(lhs);
        const auto rhsIt = m_primitiveTypes.find(rhs);

        if (lhsIt == m_primitiveTypes.end() || rhsIt == m_primitiveTypes.end())
        {
            return {};
        }

        const node_primitive_kind outKind = g_PromotionRules.at(lhsIt->second.kind, rhsIt->second.kind);

        if (outKind == node_primitive_kind::enum_max)
        {
            return {};
        }

        return m_primitiveKindToTypeId[u32(outKind)];
    }
}
