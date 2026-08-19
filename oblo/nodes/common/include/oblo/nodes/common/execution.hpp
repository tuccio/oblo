#pragma once

#include <oblo/core/uuid.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>

namespace oblo
{
    inline void add_node_execution_pins(const node_graph_context& g, bool in, bool out)
    {
        if (in)
        {
            const h32 pin = g.add_in_pin({
                .id = "d1d0f3b1-656a-49e5-89bb-e5802b6ebafe"_uuid,
            });

            g.set_deduced_type(pin, get_node_primitive_type_id<node_primitive_kind::execution>());
        }

        if (out)
        {
            const h32 pin = g.add_out_pin({
                .id = "a672953d-95f8-4002-a257-d204fe3c5736"_uuid,
            });

            g.set_deduced_type(pin, get_node_primitive_type_id<node_primitive_kind::execution>());
        }
    }

    inline void add_node_execution_ast_node(dynamic_array<h32<ast_node>>& outputs)
    {
        outputs.emplace_back();
    }
}