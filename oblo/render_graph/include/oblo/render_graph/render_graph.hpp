#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <vector>

namespace oblo
{
    class render_graph_builder_impl;

    class render_graph
    {
        friend class render_graph_builder_impl;

    public:
        render_graph() = default;
        render_graph(const render_graph&) = delete;
        render_graph(render_graph&&) noexcept = default;
        render_graph& operator=(const render_graph&) = delete;
        render_graph& operator=(render_graph&&) noexcept = default;
        ~render_graph() = default;

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
        struct node
        {
            void* ptr;
            type_id typeId;
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