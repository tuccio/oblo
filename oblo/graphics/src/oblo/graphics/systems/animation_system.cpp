#include <oblo/graphics/systems/animation_system.hpp>

#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/graphics/components/animation_component.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
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

            out = nlerp(s1, s2, alpha);
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
            animation_time_t currentTime,
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

        m_propertyRegistry = ctx.services->find<const property_registry>();
        OBLO_ASSERT(m_propertyRegistry);

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
                    progress.loop = animComponent.loop;
                }
            }
        }

        deferred.apply(*ctx.entities);

        // Iterate over all progress components, move progress forward, interpolate and apply using reflection
        for (auto&& chunk : ctx.entities->range<animation_progress_component>())
        {
            for (auto&& [e, progress] : chunk.zip<ecs::entity, animation_progress_component>())
            {
                if (progress.currentStatus != animation_status::play)
                {
                    continue;
                }

                progress.jointAnimations.clear();

                if (!progress.animationPtr)
                {
                    continue;
                }

                const animation& anim = *progress.animationPtr;

                // We calculate the "global" time for this animation
                // When looping, each channel uses this time modulo the duration, which will be the "local" one
                const time newGlobalTime = {.hns = progress.progressHns + ctx.dt.hns};
                const animation_time_t newGlobalAnimTime = to_f32_seconds(newGlobalTime);

                progress.progressHns = newGlobalTime.hns;

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

                    animation_time_t channelAnimTime = newGlobalAnimTime;

                    // When looping, consider the highest keyframe time and fmod to loop around
                    if (progress.loop && newGlobalAnimTime > keyframes->back())
                    {
                        channelAnimTime = std::fmod(to_f32_seconds(newGlobalTime), keyframes->back());
                    }

                    // Find the previous and next sample to interpolate between
                    const auto nextSampleIt = std::upper_bound(keyframes->begin(), keyframes->end(), channelAnimTime);

                    const usize nextSampleIdx = nextSampleIt == keyframes->end()
                        ? keyframes->size() - 1
                        : usize(nextSampleIt - keyframes->begin());

                    const usize previousSampleIdx = nextSampleIdx == 0 ? 0 : nextSampleIdx - 1;

                    animation_sample_buffer result;

                    if (!interpolate_sample(result,
                            *keyframes,
                            channelAnimTime,
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

                        // For now joint animations are appended to the this array, which is then processed by the
                        // mesh_system, which applies to the joints
                        auto& jointAnimation = progress.jointAnimations.emplace_back();

                        // NOTE: We rely on keeping the animation_ptr alive for this to avoid copies
                        // Maybe it will be better to resolve an id for the joint already at this level, but the
                        // mesh_sytem is the one that caches the name->index mapping for joints
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
                            jointAnimation.target = animation_progress_component::joint_animation::property::rotation;
                        }
                        else if (*propertyName == animation_data::properties::joint_scale)
                        {
                            OBLO_ASSERT(channel.format == data_format::vec3);
                            jointAnimation.scale = result.v3;
                            jointAnimation.target = animation_progress_component::joint_animation::property::scale;
                        }
                        else [[unlikely]]
                        {
                            log::error("Unknown joint property {}", *propertyName);
                            continue;
                        }
                    }
                    else if (channel.target == animation_target::component)
                    {
                        if (channel.propertyArrayIndices.begin != channel.propertyArrayIndices.end) [[unlikely]]
                        {
                            log::error("Animated array properties are not supported yet");
                            continue;
                        }

                        // Just a naive approach for now, find the property and set it
                        const ecs::type_registry& typeRegistry = ctx.entities->get_type_registry();

                        const ecs::component_type componentType = typeRegistry.find_component(channel.componentUuid);

                        if (!componentType) [[unlikely]]
                        {
                            log::error("Unable to find component type with uuid {}", channel.componentUuid);
                            continue;
                        }

                        const ecs::component_type_desc& componentTypeDesc =
                            typeRegistry.get_component_type_desc(componentType);

                        byte* const componentPtr = ctx.entities->try_get(e, componentType);

                        if (!componentPtr) [[unlikely]]
                        {
                            log::debug("Entity {} has no component {} to animate",
                                e.value,
                                componentTypeDesc.type.name);
                            continue;
                        }

                        const property_tree* const propertyTree = m_propertyRegistry->try_get(componentTypeDesc.type);

                        if (!propertyTree) [[unlikely]]
                            [[unlikely]]
                            {
                                log::error("Unable to find property tree for component type {}",
                                    componentTypeDesc.type.name);
                                continue;
                            }

                        const property_node* propertyNode{};
                        const property* property{};

                        const bool hasPropertyOrNode =
                            find_property_or_node_by_path(*propertyTree, *propertyName, &propertyNode, &property);

                        if (!hasPropertyOrNode) [[unlikely]]
                        {
                            log::error("Unable to find property {} in component type {}",
                                *propertyName,
                                componentTypeDesc.type.name);
                            continue;
                        }

                        const pair animationSizeAndAlignment = get_size_and_alignment(channel.format);

                        if (property)
                        {
                            const pair propertySizeAndAlignment = get_size_and_alignment(property->kind);

                            if (propertySizeAndAlignment != animationSizeAndAlignment) [[unlikely]]
                            {
                                log::error("Mismatching types in animation of property {} in component type {} on "
                                           "entity {}",
                                    *propertyName,
                                    componentTypeDesc.type.name,
                                    e.value);
                                continue;
                            }

                            std::memcpy(componentPtr + property->offset, &result, propertySizeAndAlignment.first);
                        }
                        else if (propertyNode)
                        {
                            // We can treat certain kinds of nodes as property, e.g. vec3 has xyz propertiees, but we
                            // can set it as a whole too
                            data_format format = data_format::enum_max;

                            if (propertyNode->type == get_type_id<vec3>())
                            {
                                format = data_format::vec3;
                            }
                            else if (propertyNode->type == get_type_id<quaternion>() ||
                                propertyNode->type == get_type_id<vec4>())
                            {
                                format = data_format::vec4;
                            }

                            if (format == data_format::enum_max) [[unlikely]]
                            {
                                log::error("Property node type is not handled: {}", propertyNode->type.name);
                                continue;
                            }

                            const pair propertySizeAndAlignment = get_size_and_alignment(format);

                            if (propertySizeAndAlignment != animationSizeAndAlignment) [[unlikely]]
                            {
                                log::error("Mismatching types in animation of property {} in component type {} on "
                                           "entity {}",
                                    *propertyName,
                                    componentTypeDesc.type.name,
                                    e.value);
                                continue;
                            }

                            std::memcpy(componentPtr + propertyNode->offset, &result, propertySizeAndAlignment.first);
                        }
                    }
                }
            }
        }
    }
}