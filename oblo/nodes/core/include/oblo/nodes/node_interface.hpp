#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    class data_document;
    class node_graph_context;

    class node_interface
    {
    public:
        virtual ~node_interface() = default;

        virtual void on_create(const node_graph_context& g) = 0;
        virtual void on_input_change(const node_graph_context& g) = 0;

        virtual void fill_properties_schema(data_document& doc, u32 nodeIndex) const = 0;
        virtual void store_properties(data_document& doc, u32 nodeIndex) const = 0;
        virtual void load_properties(const data_document& doc, u32 nodeIndex) = 0;
    };
}