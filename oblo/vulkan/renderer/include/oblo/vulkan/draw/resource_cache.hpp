#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>

#include <unordered_map>

namespace oblo
{
    template <typename T>
    struct resource_ref;

    class resource_registry;
    class texture;
}

namespace oblo::vk
{
    class staging_buffer;
    class texture_registry;
    struct resident_texture;

    class resource_cache
    {
    public:
        resource_cache();
        resource_cache(const resource_cache&) = delete;
        resource_cache(resource_cache&&) noexcept = delete;
        resource_cache& operator=(const resource_cache&) = delete;
        resource_cache& operator=(resource_cache&&) noexcept = delete;
        ~resource_cache();

        void init(resource_registry& resources, texture_registry& textureRegistry, staging_buffer& staging);

        h32<resident_texture> get_or_add(const resource_ref<texture>& t);

    private:
        struct cached_texture;

    private:
        resource_registry* m_resources{};
        staging_buffer* m_staging{};
        texture_registry* m_textureRegistry{};
        std::unordered_map<uuid, cached_texture> m_textures;
    };
}