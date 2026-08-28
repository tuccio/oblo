#include <ui_layout_atlas_cache.hpp>

#include <oblo/core/span.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/staging_buffer.hpp>
#include <oblo/renderer/graph/enums.hpp>
#include <oblo/renderer/graph/frame_graph_context.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    namespace
    {
        gpu::image_format to_gpu_format(ui::texture_format format)
        {
            switch (format)
            {
            case ui::texture_format::r8_unorm:
                return gpu::image_format::r8_unorm;
            default:
                return gpu::image_format::r8_unorm;
            }
        }

        const ui::texture* find_texture(span<const ui::texture> textures, h32<ui::texture> id)
        {
            for (const auto& t : textures)
            {
                if (t.id == id)
                {
                    return &t;
                }
            }

            return nullptr;
        }
    }

    void ui_atlas_cache::sync(const frame_graph_build_context& ctx,
        span<const ui::texture> textures,
        span<const ui::texture_command> commands)
    {
        m_transferPass = {};

        m_uploads.clear();
        m_resident.clear();

        for (const auto& cmd : commands)
        {
            switch (cmd.kind)
            {
            case ui::texture_command_kind::create: {
                const h32 retained = ctx.create_retained_texture(
                    {
                        .width = cmd.create.width,
                        .height = cmd.create.height,
                        .format = to_gpu_format(cmd.create.format),
                        .debugLabel = "UI::Atlas",
                    },
                    texture_usage::transfer_destination | texture_usage::shader_read);

                m_atlases[cmd.create.id] = retained;
            }
            break;

            case ui::texture_command_kind::update: {
                const auto it = m_atlases.find(cmd.update.id);

                if (it == m_atlases.end())
                {
                    break;
                }

                const ui::texture* const texturePtr = find_texture(textures, cmd.update.id);

                if (!texturePtr)
                {
                    break;
                }

                if (!m_transferPass)
                {
                    m_transferPass = ctx.transfer_pass();
                }

                const pin::texture resource = ctx.get_resource(it->second);

                ctx.acquire(resource, texture_usage::transfer_destination);

                const auto staged = ctx.stage_upload_image(as_bytes(span{texturePtr->data}), 1);

                m_uploads.push_back({resource, staged});
            }
            break;

            case ui::texture_command_kind::destroy: {
                const auto it = m_atlases.find(cmd.destroy.id);

                if (it != m_atlases.end())
                {
                    ctx.destroy_retained_texture(it->second);
                    m_atlases.erase(it);
                }
            }
            break;

            default:
                break;
            }
        }
    }

    h32<resident_texture> ui_atlas_cache::get_resident(const frame_graph_build_context& ctx, h32<ui::texture> id)
    {
        if (!id)
        {
            return {};
        }

        const auto cached = m_resident.find(id);

        if (cached != m_resident.end())
        {
            return cached->second;
        }

        const auto it = m_atlases.find(id);

        if (it == m_atlases.end())
        {
            return {};
        }

        const h32 t = ctx.get_resource(it->second);
        const h32 resident = ctx.acquire_bindless(t, texture_usage::shader_read);
        m_resident.emplace(id, resident);

        return resident;
    }

    void ui_atlas_cache::execute(const frame_graph_execute_context& ctx)
    {
        if (!m_transferPass)
        {
            return;
        }

        if (ctx.begin_pass(m_transferPass))
        {
            for (const auto& upload : m_uploads)
            {
                ctx.upload(upload.resource, upload.staged);
            }

            ctx.end_pass();
        }
    }

    void ui_atlas_cache::clear()
    {
        m_atlases.clear();
        m_resident.clear();
        m_uploads.clear();
        m_transferPass = {};
    }
}
