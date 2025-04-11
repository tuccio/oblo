#include <oblo/core/types.hpp>
#include <oblo/ecs/entity_registry.hpp>

namespace oblo
{
    namespace
    {
        template <typename T>
        OBLO_FORCEINLINE T& make_property_ref(
            ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset)
        {
            byte* const componentPtr = registry->try_get(ecs::entity{entityId}, ecs::component_type{componentTypeId});
            OBLO_ASSERT(componentPtr);

            return *reinterpret_cast<T*>(componentPtr + offset);
        }
    }
}

extern "C"
{
    using namespace oblo;

    DOTNET_BINDINGS_API void oblo_ecs_raw_get_float(
        ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset, f32* result)
    {
        *result = make_property_ref<f32>(registry, entityId, componentTypeId, offset);
    }

    DOTNET_BINDINGS_API void oblo_ecs_raw_set_float(
        ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset, f32 value)
    {
        make_property_ref<f32>(registry, entityId, componentTypeId, offset) = value;
    }

    DOTNET_BINDINGS_API void oblo_ecs_raw_get_int(
        ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset, i32* result)
    {
        *result = make_property_ref<i32>(registry, entityId, componentTypeId, offset);
    }

    DOTNET_BINDINGS_API void oblo_ecs_raw_set_int(
        ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset, i32 value)
    {
        make_property_ref<i32>(registry, entityId, componentTypeId, offset) = value;
    }

    DOTNET_BINDINGS_API void oblo_ecs_raw_get_uint(
        ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset, u32* result)
    {
        *result = make_property_ref<u32>(registry, entityId, componentTypeId, offset);
    }

    DOTNET_BINDINGS_API void oblo_ecs_raw_set_uint(
        ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset, u32 value)
    {
        make_property_ref<u32>(registry, entityId, componentTypeId, offset) = value;
    }

    DOTNET_BINDINGS_API void oblo_ecs_raw_get_double(
        ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset, f64* result)
    {
        *result = make_property_ref<f64>(registry, entityId, componentTypeId, offset);
    }

    DOTNET_BINDINGS_API void oblo_ecs_raw_set_double(
        ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset, f64 value)
    {
        make_property_ref<f64>(registry, entityId, componentTypeId, offset) = value;
    }

    DOTNET_BINDINGS_API void oblo_ecs_raw_get_bool(
        ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset, u8* result)
    {
        *result = u8(make_property_ref<bool>(registry, entityId, componentTypeId, offset));
    }

    DOTNET_BINDINGS_API void oblo_ecs_raw_set_bool(
        ecs::entity_registry* registry, u32 entityId, u32 componentTypeId, u32 offset, u8 value)
    {
        make_property_ref<bool>(registry, entityId, componentTypeId, offset) = bool(value);
    }
}