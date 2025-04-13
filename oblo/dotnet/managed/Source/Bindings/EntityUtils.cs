using Oblo.Ecs;
using System.Numerics;

namespace Oblo
{
    public static class EntityUtils
    {
        public static Entity CreateNamedPhysicalEntity(this EntityRegistry registry, string name, Vector3 position, Quaternion rotation, Vector3 scale)
        {
            ReadOnlySpan<ComponentTypeId> componentTypes = stackalloc[] {
                ComponentTraits<NameComponent>.TypeId,
                ComponentTraits<PositionComponent>.TypeId,
                ComponentTraits<RotationComponent>.TypeId,
                ComponentTraits<ScaleComponent>.TypeId,
                ComponentTraits<GlobalTransformComponent>.TypeId,
            };

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

            return newEntity;
        }
    }
}