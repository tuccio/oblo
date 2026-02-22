#include <oblo/gpu/image_state_tracker.hpp>

#include <oblo/gpu/structs.hpp>

namespace oblo::gpu
{
    struct image_state_tracker::tracked_state
    {
        pipeline_sync_stage pipeline;
        image_resource_state state;
    };

    image_state_tracker::image_state_tracker() = default;

    image_state_tracker::~image_state_tracker() = default;

    void image_state_tracker::add_tracking(
        h32<image> handle, pipeline_sync_stage initialPipeline, image_resource_state initialState)
    {
        m_state.emplace(handle, tracked_state{initialPipeline, initialState});
    }

    void image_state_tracker::remove_tracking(h32<image> handle)
    {
        m_state.erase(handle);
    }

    expected<image_state_transition> image_state_tracker::add_transition(
        h32<image> handle, pipeline_sync_stage newPipeline, image_resource_state newState)
    {
        tracked_state* const prev = m_state.try_find(handle);

        if (!prev)
        {
            return "Image not found, image_state_tracker::add_tracking should be called first"_err;
        }

        const image_state_transition r{
            .image = handle,
            .previousState = prev->state,
            .nextState = newState,
            .previousPipeline = prev->pipeline,
            .nextPipeline = newPipeline,
        };

        prev->pipeline = newPipeline;
        prev->state = newState;

        return r;
    }

    void image_state_tracker::clear()
    {
        m_state.clear();
    }
}