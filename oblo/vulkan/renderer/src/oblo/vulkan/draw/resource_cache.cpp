#include <oblo/vulkan/draw/resource_cache.hpp>

#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/vulkan/draw/texture_registry.hpp>

namespace oblo::vk
{
    struct resource_cache::cached_texture
    {
        h32<resident_texture> handle;
    };

    resource_cache::resource_cache() = default;

    resource_cache::~resource_cache() = default;

    void resource_cache::init(resource_registry& resources, texture_registry& textureRegistry)
    {
        m_resources = &resources;
        m_textureRegistry = &textureRegistry;
    }

    h32<resident_texture> resource_cache::get_or_add(const texture_resource_ref& t)
    {
        if (const auto it = m_textures.find(t.id); it != m_textures.end())
        {
            return it->second.handle;
        }

        const auto resource = m_resources->get_resource(t.id).as<texture>();

        if (!resource)
        {
            return {};
        }

        const auto handle = m_textureRegistry->add(*resource.get());

        if (handle)
        {
            m_textures.emplace(t.id, handle);
        }

        return handle;
    }
}