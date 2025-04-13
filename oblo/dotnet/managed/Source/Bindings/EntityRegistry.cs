using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Oblo.Ecs
{
    public readonly struct EntityRegistry
    {
        private readonly IntPtr _entityRegistry;

        public IntPtr NativeHandle => _entityRegistry;

        public EntityRegistry(IntPtr nativeRegistry)
        {
            _entityRegistry = nativeRegistry;
        }

        public EntityId Create(ReadOnlySpan<ComponentTypeId> components)
        {
            return new EntityId(Bindings.oblo_ecs_entity_create(NativeHandle, MemoryMarshal.GetReference(components), components.Length));
        }

        public void Destroy(EntityId entityId)
        {
            Bindings.oblo_ecs_entity_destroy(NativeHandle, entityId.Value);
        }
    }
}