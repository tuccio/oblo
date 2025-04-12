namespace Oblo.Ecs
{
    public record struct EntityId(uint Value);

    public record struct ComponentTypeId(uint Value);

    public interface IComponent
    {
        public Entity Entity { get; }

        public ComponentTypeId TypeId { get; }

        bool IsAlive => Bindings.oblo_ecs_component_exists(Entity.EntityRegistry, Entity.Id.Value, TypeId.Value);
    }

    public record struct Entity(IntPtr EntityRegistry, EntityId Id)
    {
        public T GetComponent<T>() where T : struct, IComponent
        {
            return Bindings.GetComponent<T>(this);
        }
    }
}
