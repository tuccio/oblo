#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_interface.hpp>
#include <oblo/nodes/zero_properties_node.hpp>

namespace oblo
{
    template <typename Base>
    class binary_operator_base : public zero_properties_node
    {
    public:
        void on_create(const node_graph_context& g)
        {
            g.add_in_pin({
                .id = "fcd0651c-6bf6-4933-84a3-d6ca05a60ae6"_uuid,
                .name = "A",
            });

            g.add_in_pin({
                .id = "fe917388-c132-420b-84df-bf1108a8992c"_uuid,
                .name = "B",
            });

            g.add_out_pin({
                .id = "4fe1662d-42c3-46d8-8351-57b23e33cb3c"_uuid,
                .name = "Result",
            });
        }

        void on_input_change(const node_graph_context&)
        {
            // TODO: Deduce output
        }
    };

    class add_operator final : public binary_operator_base<add_operator>
    {
    public:
        static constexpr uuid id = "13514366-b0af-4a25-a4c6-384bd7277a35"_uuid;
        static constexpr cstring_view name = "Add";
    };

    class mul_operator final : public binary_operator_base<mul_operator>
    {
    public:
        static constexpr uuid id = "f8b0b90c-3f18-4235-b694-c45f9657a317"_uuid;
        static constexpr cstring_view name = "Multiply";
    };
}