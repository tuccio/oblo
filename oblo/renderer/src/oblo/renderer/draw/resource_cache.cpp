#include <oblo/renderer/draw/resource_cache.hpp>

#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/log/log.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/renderer/draw/texture_registry.hpp>

namespace oblo
{
    namespace
    {
        bool load_and_add(
            texture_registry& registry, const resource_ptr<texture>& resource, h32<resident_texture>& handle)
        {
            if (resource.is_successfully_loaded())
            {
                handle = registry.add(*resource, resource.get_name());
                return true;
            }

            resource.load_start_async();
            handle = registry.acquire();
            return false;
        }
    }

    struct resource_cache::async_load
    {
        uuid id;
        h32<resident_texture> handle;
        resource_ptr<texture> resource;
    };

    struct resource_cache::cached_texture
    {
        h32<resident_texture> handle;
    };

    resource_cache::resource_cache() = default;

    resource_cache::~resource_cache() = default;

    void resource_cache::init(texture_registry& textureRegistry)
    {
        m_textureRegistry = &textureRegistry;
    }

    void resource_cache::update()
    {
        for (auto it = m_asyncLoads.begin(); it != m_asyncLoads.end();)
        {
            if (it->resource.is_currently_loading())
            {
                ++it;
            }
            else
            {
                if (!it->resource.is_successfully_loaded())
                {
                    log::debug("Failed to load async texture {}", it->resource.get_id());
                    it = m_asyncLoads.erase_unordered(it);
                    continue;
                }

                if (m_textureRegistry->set_texture(it->handle, *it->resource, it->resource.get_name()))
                {
                    it = m_asyncLoads.erase_unordered(it);
                }
                else
                {
                    // We assume we are just out of space and we can retry next frame, we should return an error code
                    // instead
                    ++it;
                }
            }
        }
    }

    h32<resident_texture> resource_cache::get_or_add(const texture_resource_ptr& resource)
    {
        if (!resource)
        {
            return {};
        }

        const auto id = resource.get_id();

        if (const auto it = m_textures.find(id); it != m_textures.end())
        {
            return it->second.handle;
        }

        h32<resident_texture> handle;
        const auto isLoaded = load_and_add(*m_textureRegistry, resource, handle);

        if (handle)
        {
            m_textures.emplace(id, handle);

            if (!isLoaded)
            {
                m_asyncLoads.emplace_back(id, handle, resource);
            }
        }

        return handle;
    }
}