#include <oblo/graphics/systems/animation_system.hpp>

#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/graphics/components/animation_component.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/resources/animation.hpp>
#include <oblo/scene/resources/animation_data.hpp>

#include <algorithm>

namespace oblo
{
    namespace
    {
        template <typename T>
        [[nodiscard]] bool read_samples(
            std::span<const byte> samples, usize previousSampleIdx, usize nextSampleIdx, T& s1, T& s2)
        {
            OBLO_ASSERT(previousSampleIdx <= nextSampleIdx);

            if ((nextSampleIdx + 1) * sizeof(T) > samples.size())
            {
                return false;
            }

            const T* const array = reinterpret_cast<const T*>(samples.data());
            s1 = array[previousSampleIdx];
            s2 = array[nextSampleIdx];

            return true;
        };

        template <typename T>
        [[nodiscard]] bool linear_interpolate_vector(
            T& out, std::span<const byte> samples, usize previousSampleIdx, usize nextSampleIdx, f32 alpha)
        {
            T s1, s2;

            if (!read_samples(samples, previousSampleIdx, nextSampleIdx, s1, s2))
            {
                return false;
            }

            out = s1 * (1 - alpha) + s2 * alpha;
            return true;
        }

        [[nodiscard]] bool linear_interpolate_quaternion(
            quaternion& out, std::span<const byte> samples, usize previousSampleIdx, usize nextSampleIdx, f32 alpha)
        {
            quaternion s1, s2;

            if (!read_samples(samples, previousSampleIdx, nextSampleIdx, s1, s2))
            {
                return false;
            }

            out = slerp(s1, s2, alpha);
            return true;
        }

        union animation_sample_buffer {
            vec2 v2;
            vec3 v3;
            vec4 v4;
            quaternion q;
        };

        [[nodiscard]] bool interpolate_sample(animation_sample_buffer& buffer,
            std::span<const animation_time_t> keyframes,
            time currentTimeHns,
            std::span<const byte> samples,
            usize previousSampleIdx,
            usize nextSampleIdx,
            const animation_channel& channel)
        {
            switch (channel.interpolation)
            {
            case animation_interpolation::linear: {
                const animation_time_t nextTime = keyframes[nextSampleIdx];
                const animation_time_t previousTime = keyframes[previousSampleIdx];
                const animation_time_t currentTime = to_f32_seconds(currentTimeHns);

                // Avoid dividing by zero or even close to zero, we just ignore very small durations
                const f32 duration = nextTime - previousTime;
                const f32 alpha = duration < 1e-5f ? 0.f : max(0.f, min(1.f, (currentTime - previousTime) / duration));

                switch (channel.dataKind)
                {
                case animation_data_kind::any_vector:
                    switch (channel.format)
                    {
                    case data_format::vec2:
                        return linear_interpolate_vector(buffer.v2, samples, previousSampleIdx, nextSampleIdx, alpha);

                    case data_format::vec3:
                        return linear_interpolate_vector(buffer.v3, samples, previousSampleIdx, nextSampleIdx, alpha);

                    case data_format::vec4:
                        return linear_interpolate_vector(buffer.v4, samples, previousSampleIdx, nextSampleIdx, alpha);
                    default:
                        break;
                    }
                    break;

                case animation_data_kind::quaternion:
                    if (channel.format == data_format::vec4)
                    {
                        return linear_interpolate_quaternion(buffer.q,
                            samples,
                            previousSampleIdx,
                            nextSampleIdx,
                            alpha);
                    }

                    break;
                }
            }

            break;

            default:
                OBLO_ASSERT(false, "Interpolation type is not implemented yet");
                return false;
            }

            OBLO_ASSERT(false, "The combination of format and interpolation is not implemented yet");
            return false;
        }
    }

    void animation_system::first_update(const ecs::system_update_context& ctx)
    {
        m_resourceRegistry = ctx.services->find<const resource_registry>();
        OBLO_ASSERT(m_resourceRegistry);

        update(ctx);
    }

    void animation_system::update(const ecs::system_update_context& ctx)
    {
        ecs::deferred deferred{ctx.frameAllocator};

        // Process entities with animation_component but without animation_progress_component
        for (auto&& chunk : ctx.entities->range<const animation_component>().exclude<animation_progress_component>())
        {
            for (auto&& [e, animComponent] : chunk.zip<ecs::entity, animation_component>())
            {
                resource_ptr animationRes = m_resourceRegistry->get_resource(animComponent.animation);

                if (!animationRes)
                {
                    continue;
                }

                animationRes.load_start_async();

                if (animationRes.is_successfully_loaded())
                {
                    animation_progress_component& progress = deferred.add<animation_progress_component>(e);
                    progress.animationPtr = std::move(animationRes);
                    progress.progressHns = 0;
                    progress.currentStatus = animComponent.statusOnLoad;
                }
            }
        }

        deferred.apply(*ctx.entities);

        for (auto&& chunk : ctx.entities->range<animation_progress_component>())
        {
            for (auto&& [e, progress] : chunk.zip<ecs::entity, animation_progress_component>())
            {
                if (progress.currentStatus == animation_status::play)
                {
                    progress.jointAnimations.clear();

                    if (progress.animationPtr)
                    {
                        const animation& anim = *progress.animationPtr;

                        const time newTime = {.hns = progress.progressHns + ctx.dt.hns};
                        const animation_time_t newAnimTime = to_f32_seconds(newTime);

                        progress.progressHns = newTime.hns;

                        for (const animation_channel& channel : anim.channels)
                        {
                            const expected keyframes = animation_data::get_channel_keyframes(anim, channel);
                            const expected samples = animation_data::get_channel_data(anim, channel);
                            const expected propertyName = animation_data::get_channel_property_name(anim, channel);

                            if (!samples || !keyframes || !propertyName)
                            {
                                log::error("Unable to retrieve animation data");
                                continue;
                            }

                            if (samples->empty())
                            {
                                continue;
                            }

                            animation_sample_buffer result;

                            const auto nextSampleIt =
                                std::upper_bound(keyframes->begin(), keyframes->end(), newAnimTime);

                            const usize nextSampleIdx = nextSampleIt == keyframes->end()
                                ? keyframes->size() - 1
                                : usize(nextSampleIt - keyframes->begin());

                            const usize previousSampleIdx = nextSampleIdx == 0 ? 0 : nextSampleIdx - 1;

                            if (!interpolate_sample(result,
                                    *keyframes,
                                    newTime,
                                    *samples,
                                    previousSampleIdx,
                                    nextSampleIdx,
                                    channel)) [[unlikely]]
                            {
                                log::error("Failed to interpolate animation sample");
                                continue;
                            }

                            if (channel.target == animation_target::joint)
                            {
                                const expected jointName = animation_data::get_channel_joint_name(anim, channel);

                                if (!jointName)
                                {
                                    log::error("Unable to retrieve joint info");
                                    continue;
                                }

                                auto& jointAnimation = progress.jointAnimations.emplace_back();

                                jointAnimation.jointName = *jointName;

                                if (*propertyName == animation_data::properties::joint_translation)
                                {
                                    OBLO_ASSERT(channel.format == data_format::vec3);
                                    jointAnimation.translation = result.v3;
                                    jointAnimation.target =
                                        animation_progress_component::joint_animation::property::translation;
                                }
                                else if (*propertyName == animation_data::properties::joint_rotation)
                                {
                                    OBLO_ASSERT(channel.format == data_format::vec4);
                                    jointAnimation.rotation = result.q;
                                    jointAnimation.target =
                                        animation_progress_component::joint_animation::property::rotation;
                                }
                                else if (*propertyName == animation_data::properties::joint_scale)
                                {
                                    OBLO_ASSERT(channel.format == data_format::vec3);
                                    jointAnimation.scale = result.v3;
                                    jointAnimation.target =
                                        animation_progress_component::joint_animation::property::scale;
                                }
                                else [[unlikely]]
                                {
                                    log::error("Unknown joint property {}", *propertyName);
                                    continue;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}