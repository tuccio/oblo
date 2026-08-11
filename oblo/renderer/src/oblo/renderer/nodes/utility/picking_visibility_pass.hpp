#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/picking_configuration.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo
{
    struct draw_buffer_data;

    // Renders the scene into a visibility buffer for picking purposes, only rasterizing a small region around the
    // cursor. Instances whose entity is listed in the picking configuration are skipped, which allows excluding e.g.
    // the entity currently being dragged by a gizmo.
    struct picking_visibility_pass
    {
        pin::data<vec2u> inResolution;
        pin::data<std::span<draw_buffer_data>> inDrawData;
        pin::data<std::span<pin::buffer>> inDrawCallBuffer;

        pin::buffer inCameraBuffer;
        pin::buffer inMeshDatabase;
        pin::buffer inEntitySetBuffer;

        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::data<picking_configuration> inPickingConfiguration;

        pin::texture outVisibilityBuffer;
        pin::texture outDepthBuffer;

        // GPU buffer holding the entity ids to skip, uploaded every frame from inPickingConfiguration.
        pin::buffer excludedEntities;

        h32<render_pass> renderPass;
        h32<render_pass_instance> passInstance;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}
