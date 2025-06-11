#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    class data_document;
}

namespace oblo::script
{
    struct pin_descriptor
    {
        uuid id{};
        string name;
    };

    class node_graph;
    struct node_vertex;

    struct node_handle;

    class node_interface;

    using instantiate_node_fn = unique_ptr<node_interface> (*)();

    struct node_descriptor
    {
        uuid id{};
        string name;
        instantiate_node_fn instantiate{};
    };

    class node_graph_context;

    // Alternative with interface

    class node_interface
    {
    public:
        virtual ~node_interface() = default;

        virtual void on_create(const node_graph_context& g) = 0;
        virtual void on_change(const node_graph_context& g) = 0;

        virtual void store(data_document& doc, u32 nodeIndex) = 0;
        virtual void load(const data_document& doc, u32 nodeIndex) const = 0;
    };
}