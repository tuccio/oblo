#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <vector>

namespace oblo
{
    using render_graph_execute = void (*)(void*);

    class render_graph
    {
    public:
        void execute();

        void* find_node(type_id type);

        template <typename T>
        T* find_node()
        {
            return static_cast<T*>(find_node(get_type_id<T>()));
        }

        void* find_input(std::string_view name, type_id type);

        template <typename T>
        T* find_input(std::string_view name)
        {
            return static_cast<T*>(find_input(name, get_type_id<T>()));
        }

    private:
        friend class render_graph_builder;

    private:
        struct node
        {
            void* ptr;
            type_id typeId;
            render_graph_execute execute;
        };

        struct input
        {
            void* ptr;
            type_id typeId;
            std::string name;
        };

        std::vector<std::byte> m_nodeStorage;
        std::vector<std::byte> m_pinStorage;
        std::vector<node> m_nodes;
        std::vector<input> m_inputs;
    };
}