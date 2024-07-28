#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>

#include <vulkan/vulkan_core.h>

namespace oblo::vk
{
    struct frame_graph_pin_storage;
    struct texture;

    enum class pass_kind : u8;
    enum class texture_usage : u8;

    class image_layout_tracker
    {
    public:
        using handle_type = h32<frame_graph_pin_storage>;

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
        h32_flat_extpool_dense_map<frame_graph_pin_storage, image_layout> m_state;
    };
}