namespace Oblo.Ecs
{
    public readonly record struct EntityId(uint Value)
    {
        static uint EntityIndexMask = Bindings.oblo_ecs_get_entity_index_mask();

        public uint ExtractIndex()
        {
            return Value & EntityIndexMask;
        }
    }


    public readonly record struct ComponentTypeId(uint Value);

    public interface IComponent
    {
        public Entity Entity { get; }

        public ComponentTypeId TypeId { get; }

        public bool IsAlive { get; }
    }

    public readonly record struct Entity(EntityRegistry EntityRegistry, EntityId Id)
    {
        public T AddComponent<T>() where T : struct, IComponent
        {
            return Bindings.AddComponent<T>(this);
        }

        public void RemoveComponent<T>() where T : struct, IComponent
        {
            Bindings.RemoveComponent<T>(this);
        }

        public T GetComponent<T>() where T : struct, IComponent
        {
            return Bindings.GetComponent<T>(this);
        }

        public bool IsAlive => Bindings.oblo_ecs_entity_exists(EntityRegistry.NativeHandle, Id.Value);
    }
}
