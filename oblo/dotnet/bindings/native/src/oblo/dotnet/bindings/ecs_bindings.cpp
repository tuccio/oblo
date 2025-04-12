#include <oblo/core/types.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    namespace
    {
        template <typename T>
        OBLO_FORCEINLINE T& make_property_ref(
            ecs::entity_registry* registry, ecs::entity entityId, ecs::component_type componentTypeId, u32 offset)
        {
            byte* const componentPtr = registry->try_get(entityId, componentTypeId);
            OBLO_ASSERT(componentPtr);

            return *reinterpret_cast<T*>(componentPtr + offset);
        }

        const ecs::type_registry* g_bindingsTypeRegistry{};
    }
}

extern "C"
{
    using namespace oblo;

    DOTNET_BINDINGS_API void oblo_ecs_register_types(ecs::entity_registry* registry)
    {
        // TODO: We need a static type registry we can use, for now we reference the first
        g_bindingsTypeRegistry = &registry->get_type_registry();
    }

    DOTNET_BINDINGS_API ecs::component_type oblo_ecs_find_component_type(const char* typeName)
    {
        return g_bindingsTypeRegistry->find_component(type_id{.name = typeName});
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_get_float(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        f32* result)
    {
        *result = make_property_ref<f32>(registry, entityId, componentTypeId, offset);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_set_float(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        f32 value)
    {
        make_property_ref<f32>(registry, entityId, componentTypeId, offset) = value;
        registry->notify(entityId);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_get_int(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        i32* result)
    {
        *result = make_property_ref<i32>(registry, entityId, componentTypeId, offset);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_set_int(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        i32 value)
    {
        make_property_ref<i32>(registry, entityId, componentTypeId, offset) = value;
        registry->notify(entityId);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_get_uint(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        u32* result)
    {
        *result = make_property_ref<u32>(registry, entityId, componentTypeId, offset);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_set_uint(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        u32 value)
    {
        make_property_ref<u32>(registry, entityId, componentTypeId, offset) = value;
        registry->notify(entityId);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_get_double(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        f64* result)
    {
        *result = make_property_ref<f64>(registry, entityId, componentTypeId, offset);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_set_double(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        f64 value)
    {
        make_property_ref<f64>(registry, entityId, componentTypeId, offset) = value;
        registry->notify(entityId);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_get_bool(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        u8* result)
    {
        *result = u8(make_property_ref<bool>(registry, entityId, componentTypeId, offset));
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_set_bool(
        ecs::entity_registry* registry, ecs::entity entityId, ecs::component_type componentTypeId, u32 offset, u8 value)
    {
        make_property_ref<bool>(registry, entityId, componentTypeId, offset) = bool(value);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_get_vec3(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        vec3* result)
    {
        *result = make_property_ref<vec3>(registry, entityId, componentTypeId, offset);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_set_vec3(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        const vec3* value)
    {
        make_property_ref<vec3>(registry, entityId, componentTypeId, offset) = *value;
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_get_quaternion(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        quaternion* result)
    {
        *result = make_property_ref<quaternion>(registry, entityId, componentTypeId, offset);
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_set_quaternion(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        const quaternion* value)
    {
        make_property_ref<quaternion>(registry, entityId, componentTypeId, offset) = *value;
    }
}