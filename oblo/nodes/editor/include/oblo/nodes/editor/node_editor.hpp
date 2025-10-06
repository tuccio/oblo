#pragma once

#include <oblo/core/unique_ptr.hpp>

namespace oblo
{
    class node_graph;

    class node_editor
    {
    public:
        node_editor();
        node_editor(const node_editor&) = delete;
        node_editor(node_editor&&) noexcept;
        ~node_editor();

        node_editor& operator=(const node_editor&) = delete;
        node_editor& operator=(node_editor&&) noexcept;

        void init(node_graph& g);

        void update();

    private:
        struct impl;
        unique_ptr<impl> m_impl;
    };
}