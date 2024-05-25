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
    class texture_registry;
    struct resident_texture;

    using texture_resource_ref = resource_ref<oblo::texture>;

    class resource_cache
    {
    public:
        resource_cache();
        resource_cache(const resource_cache&) = delete;
        resource_cache(resource_cache&&) noexcept = delete;
        resource_cache& operator=(const resource_cache&) = delete;
        resource_cache& operator=(resource_cache&&) noexcept = delete;
        ~resource_cache();

        void init(resource_registry& resources, texture_registry& textureRegistry);

        h32<resident_texture> get_or_add(const texture_resource_ref& t);

    private:
        struct cached_texture;

    private:
        resource_registry* m_resources{};
        texture_registry* m_textureRegistry{};
        std::unordered_map<uuid, cached_texture> m_textures;
    };
}