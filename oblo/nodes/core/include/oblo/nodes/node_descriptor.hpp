#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/nodes/node_interface.hpp>

namespace oblo
{
    class data_document;

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
}