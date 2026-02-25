#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/gpu/forward.hpp>

namespace oblo::gpu
{
    class image_state_tracker
    {
    public:
        image_state_tracker();
        image_state_tracker(const image_state_tracker&) = delete;
        image_state_tracker(image_state_tracker&&) noexcept = default;
        image_state_tracker& operator=(const image_state_tracker&) = delete;
        image_state_tracker& operator=(image_state_tracker&&) noexcept = default;

        ~image_state_tracker();

        void add_tracking(h32<image> handle, pipeline_sync_stage initialPipeline, image_resource_state initialState);
        void remove_tracking(h32<image> handle);

        expected<image_state_transition> add_transition(
            h32<image> handle, pipeline_sync_stage newPipeline, image_resource_state newState);

        void clear();

        [[nodiscard]] bool try_get_state(h32<image> handle, image_resource_state& state) const;

    private:
        struct tracked_state;

    private:
        h32_flat_extpool_dense_map<image, tracked_state> m_state;
    };
}