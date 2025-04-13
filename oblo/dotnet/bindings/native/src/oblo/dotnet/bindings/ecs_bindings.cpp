#include <oblo/core/types.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

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

        ecs::component_and_tag_sets make_type_set(const ecs::component_type* components, i32 componentsCount)
        {
            ecs::component_and_tag_sets types{};

            for (i32 i = 0; i < componentsCount; ++i)
            {
                types.components.add(components[i]);
            }

            return types;
        }
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

    DOTNET_BINDINGS_API u32 oblo_ecs_get_entity_index_mask()
    {
        return ~handle_pool<ecs::entity::value_type, ecs::entity_generation_bits>::get_gen_mask();
    }

    DOTNET_BINDINGS_API u32 oblo_ecs_entity_exists(ecs::entity_registry* registry, ecs::entity entityId)
    {
        return u32{registry->contains(entityId)};
    }

    DOTNET_BINDINGS_API u32 oblo_ecs_entity_create(
        ecs::entity_registry* registry, const ecs::component_type* components, i32 componentsCount)
    {
        const ecs::component_and_tag_sets types = make_type_set(components, componentsCount);

        ecs::entity ids[1];
        registry->create(types, 1, ids);

        return ids[0].value;
    }

    DOTNET_BINDINGS_API void oblo_ecs_entity_destroy(ecs::entity_registry* registry, ecs::entity entityId)
    {
        return registry->destroy(entityId);
    }

    DOTNET_BINDINGS_API void oblo_ecs_entity_destroy_hierarchy(ecs::entity_registry* registry, ecs::entity entityId)
    {
        ecs_utility::destroy_hierarchy(*registry, entityId);
    }

    DOTNET_BINDINGS_API void oblo_ecs_entity_reparent(
        ecs::entity_registry* registry, ecs::entity entityId, ecs::entity newParent)
    {
        ecs_utility::reparent_entity(*registry, entityId, newParent);
    }

    DOTNET_BINDINGS_API u32 oblo_ecs_component_exists(
        ecs::entity_registry* registry, ecs::entity entityId, ecs::component_type componentTypeId)
    {
        return u32{registry->try_get(entityId, componentTypeId) != nullptr};
    }

    DOTNET_BINDINGS_API void oblo_ecs_component_add(ecs::entity_registry* registry,
        ecs::entity entityId,
        const ecs::component_type* components,
        i32 componentsCount)
    {
        const ecs::component_and_tag_sets types = make_type_set(components, componentsCount);
        registry->add(entityId, types);
    }

    DOTNET_BINDINGS_API void oblo_ecs_component_remove(
        ecs::entity_registry* registry, ecs::entity entityId, ecs::component_type componentTypeId)
    {
        ecs::component_and_tag_sets types{};
        types.components.add(componentTypeId);
        registry->remove(entityId, types);
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
        registry->notify(entityId);
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
        registry->notify(entityId);
    }

    DOTNET_BINDINGS_API const char* oblo_ecs_property_get_string(
        ecs::entity_registry* registry, ecs::entity entityId, ecs::component_type componentTypeId, u32 offset, u32* len)
    {
        property_value_wrapper w{};
        w.assign_from(property_kind::string, &make_property_ref<byte>(registry, entityId, componentTypeId, offset));

        const auto view = w.get_string();
        *len = view.size32();
        return view.data();
    }

    DOTNET_BINDINGS_API void oblo_ecs_property_set_string(ecs::entity_registry* registry,
        ecs::entity entityId,
        ecs::component_type componentTypeId,
        u32 offset,
        const char* utf8,
        u32 len)
    {
        property_value_wrapper w{string_view{utf8, len}};
        w.assign_to(property_kind::string, &make_property_ref<byte>(registry, entityId, componentTypeId, offset));
        registry->notify(entityId);
    }
}