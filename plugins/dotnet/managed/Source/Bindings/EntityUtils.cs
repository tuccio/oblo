using Oblo.Ecs;
using System.Numerics;

namespace Oblo
{
    public static class EntityUtils
    {
        public static Entity CreateNamedPhysicalEntity(this EntityRegistry registry, string name, EntityId parent, Vector3 position, Quaternion rotation, Vector3 scale)
        {
            ReadOnlySpan<ComponentTypeId> componentTypes = stackalloc[] {
                ComponentTraits<NameComponent>.TypeId,
                ComponentTraits<PositionComponent>.TypeId,
                ComponentTraits<RotationComponent>.TypeId,
                ComponentTraits<ScaleComponent>.TypeId,
                ComponentTraits<GlobalTransformComponent>.TypeId,
                ComponentTraits<ParentComponent>.TypeId,
            };

            if (!parent.IsValid)
            {
                // Remove the parent component if not necessary
                componentTypes = componentTypes.Slice(0, componentTypes.Length - 1);
            }

            EntityId newEntityId = registry.Create(componentTypes);

            Entity newEntity = new Entity(registry, newEntityId);

            var nameComponent = newEntity.GetComponent<NameComponent>();
            var positionComponent = newEntity.GetComponent<PositionComponent>();
            var rotationComponent = newEntity.GetComponent<RotationComponent>();
            var scaleComponent = newEntity.GetComponent<ScaleComponent>();

            nameComponent.Value = name;
            positionComponent.Value = position;
            rotationComponent.Value = rotation;
            scaleComponent.Value = scale;

            // NOTE: Global transform is uninitialized at this point

            if (parent.IsValid)
            {
                Bindings.oblo_ecs_entity_reparent(registry.NativeHandle, newEntityId.Value, parent.Value);
            }

            return newEntity;
        }

        public static void DestroyHierarchy(this EntityRegistry registry, EntityId root)
        {
            Bindings.oblo_ecs_entity_destroy_hierarchy(registry.NativeHandle, root.Value);
        }

        public static void DestroyHierarchy(this Entity entity)
        {
            entity.EntityRegistry.DestroyHierarchy(entity.Id);
        }
    }
}