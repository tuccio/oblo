#include <ui_layout_graph.hpp>

#include <ui_layout_render_node.hpp>

namespace oblo::ui_layout_view
{
    frame_graph_template create(const frame_graph_registry& registry)
    {
        frame_graph_template graph;

        graph.init(registry);

        const auto render = graph.add_node<ui_layout_render_node>();

        graph.make_input(render, &ui_layout_render_node::inResolution, InResolution);
        graph.make_input(render, &ui_layout_render_node::inDrawCommands, InDrawCommands);
        graph.make_input(render, &ui_layout_render_node::inTextures, InTextures);
        graph.make_input(render, &ui_layout_render_node::inTextureCommands, InTextureCommands);
        graph.make_input(render, &ui_layout_render_node::inAtlasCache, InAtlasCache);
        graph.make_output(render, &ui_layout_render_node::outImage, OutLayoutImage);

        return graph;
    }
}