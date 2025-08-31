#pragma once

#include <oblo/core/any.hpp>
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

    using instantiate_node_fn = unique_ptr<node_interface> (*)(const any& userdata);

    struct node_descriptor
    {
        /// @brief The unique identifier of the node type. It must be persistent for serialization purposes.
        uuid id{};

        /// @brief A user-friendly name for the node type.
        string name;

        /// @brief Creates an isntance of the node.
        instantiate_node_fn instantiate{};

        /// @brief Userdata that will be passed to the instantiate call.
        any userdata;
    };
}