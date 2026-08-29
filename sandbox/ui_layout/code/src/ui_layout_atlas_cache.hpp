#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/unordered_map.hpp>
#include <oblo/gpu/staging_buffer.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/ui/texture.hpp>

namespace oblo
{
    class frame_graph_build_context;
    class frame_graph_execute_context;

    struct ui_atlas_cache
    {
    public:
        void sync(const frame_graph_build_context& ctx,
            span<const ui::texture> textures,
            span<const ui::texture_command> commands);

        h32<resident_texture> get_resident(const frame_graph_build_context& ctx, h32<ui::texture> id);

        // Runs the pending atlas uploads queued during sync(). Call once per frame from the
        // render node's execute().
        void execute(const frame_graph_execute_context& ctx);

        // Drops all owned atlases. Must be called before the owning frame graph is destroyed.
        void clear();

    private:
        h32<retained_texture> get_or_create_retained(
            const frame_graph_build_context& ctx, h32<ui::texture> id, const ui::texture& data);

        struct pending_upload
        {
            pin::texture resource;
            gpu::staging_buffer_span staged;
            u32 x;
            u32 y;
            u32 width;
            u32 height;
        };

        unordered_map<h32<ui::texture>, h32<retained_texture>, hash<h32<ui::texture>>> m_atlases;
        unordered_map<h32<ui::texture>, h32<resident_texture>, hash<h32<ui::texture>>> m_resident;

        dynamic_array<pending_upload> m_uploads;

        h32<transfer_pass_instance> m_transferPass;
    };
}
