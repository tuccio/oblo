#include <oblo/renderer/graph/node_common.hpp>
#include <oblo/renderer/nodes/shadows/shadow_output.hpp>

namespace oblo
{
    void shadow_output::build(const frame_graph_build_context& ctx)
    {
        const auto& cfg = ctx.access(inConfig);

        ctx.push(outShadowSink, {outShadow, cfg.lightIndex});
    }
}