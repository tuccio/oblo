#pragma once

#include <oblo/core/forward.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    class data_document;
    class node_graph_context;

    struct node_property_descriptor;

    class node_interface
    {
    public:
        virtual ~node_interface() = default;

        virtual void on_create(const node_graph_context& g) = 0;
        virtual void on_input_change(const node_graph_context& g) = 0;

        virtual void fetch_properties_descriptors(
            dynamic_array<node_property_descriptor>& outPropertyDescriptors) const = 0;

        virtual void store(data_document& doc, u32 nodeIndex) const = 0;
        virtual void load(const data_document& doc, u32 nodeIndex) = 0;
    };
}