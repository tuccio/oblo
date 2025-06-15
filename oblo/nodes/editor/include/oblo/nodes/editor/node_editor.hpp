#pragma once

namespace oblo
{
    class node_graph;

    class node_editor
    {
    public:
        void init(node_graph& g);

        void update();

    private:
        node_graph* m_graph{};
    };
}