#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/renderer/gpu_temporary_aliases.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo
{
    struct transient_texture_resource;
    struct texture;

    enum class pass_kind : u8;

    class image_layout_tracker
    {
    public:
        using handle_type = h32<transient_texture_resource>;

    public:
        static VkImageLayout deduce_layout(texture_usage usage);

    public:
        image_layout_tracker();
        image_layout_tracker(const image_layout_tracker&) = delete;
        image_layout_tracker(image_layout_tracker&&) noexcept = default;
        image_layout_tracker& operator=(const image_layout_tracker&) = delete;
        image_layout_tracker& operator=(image_layout_tracker&&) noexcept = default;

        ~image_layout_tracker();

        void start_tracking(handle_type handle, const texture& t);
        bool add_transition(VkImageMemoryBarrier2& outBarrier, handle_type handle, pass_kind pass, texture_usage usage);

        void clear();

        expected<VkImageLayout> try_get_layout(handle_type handle) const;

    private:
        struct image_layout;

    private:
        h32_flat_extpool_dense_map<transient_texture_resource, image_layout> m_state;
    };
}