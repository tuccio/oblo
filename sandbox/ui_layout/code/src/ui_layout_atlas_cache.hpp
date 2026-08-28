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

    // Owns the GPU-resident atlas textures for a ui::context's texture storage.
    //
    // Mirrors the texture registry in oblo/app/imgui, but specialized for the UI's glyph
    // atlases. The render node calls sync() each frame with the per-frame texture commands
    // and CPU-side atlas data; the cache creates/updates/destroys retained (persistent)
    // GPU textures accordingly and keeps them uploaded. The render node then queries
    // get_resident() to obtain a bindless handle (h32<resident_texture>) for the atlas
    // referenced by a draw command, which the shader samples through the global bindless
    // descriptor set (g_Textures2D).
    struct ui_atlas_cache
    {
    public:
        // Applies the per-frame texture commands (create/update/destroy) against the given
        // frame graph build context, using the CPU-side atlas data for uploads.
        void sync(const frame_graph_build_context& ctx,
            span<const ui::texture> textures,
            span<const ui::texture_command> commands);

        // Returns the bindless handle for the given atlas, acquiring it against the frame
        // graph if it hasn't been acquired yet this frame. Returns an invalid handle when
        // the atlas is unknown (the shader treats that as a solid rectangle).
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
        };

        unordered_map<h32<ui::texture>, h32<retained_texture>, hash<h32<ui::texture>>> m_atlases;
        unordered_map<h32<ui::texture>, h32<resident_texture>, hash<h32<ui::texture>>> m_resident;

        dynamic_array<pending_upload> m_uploads;

        h32<transfer_pass_instance> m_transferPass;
    };
}
