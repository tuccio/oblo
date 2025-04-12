using Oblo.Ecs;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Oblo.Behaviour
{
    public class BehaviourSystem
    {
        private struct BehaviourEntity
        {
            public EntityId EntityId;
            public IBehaviour Behaviour;
        }

        private List<BehaviourEntity> _entities = new();

        public void RegisterBehaviour(uint entityId, Assembly assembly)
        {
            var behaviourInterface = typeof(IBehaviour);

            foreach (Type type in assembly.GetTypes())
            {
                if (!type.IsAbstract && behaviourInterface.IsAssignableFrom(type))
                {
                    var ctor = type.GetConstructor(Type.EmptyTypes);

                    if (ctor is not null)
                    {
                        IBehaviour? behaviour = Activator.CreateInstance(type) as IBehaviour;

                        if (behaviour is not null)
                        {
                            _entities.Add(new BehaviourEntity { EntityId = new EntityId(entityId), Behaviour = behaviour });
                        }

                        break;
                    }
                }
            }
        }

        public void Update(IntPtr entityRegistry)
        {
            Bindings.oblo_ecs_register_types(entityRegistry);

            var ctx = new UpdateContext();

            // TODO
            ctx.DeltaTime = TimeSpan.FromMilliseconds(16);

            foreach (BehaviourEntity e in _entities)
            {
                ctx.Entity = new Entity(entityRegistry, e.EntityId);

                e.Behaviour.OnUpdate(ctx);
            }
        }

        [UnmanagedCallersOnly]
        private static IntPtr Create()
        {
            BehaviourSystem system = new();

            var pinned = GCHandle.Alloc(system, GCHandleType.Normal);

            return GCHandle.ToIntPtr(pinned);
        }

        [UnmanagedCallersOnly]
        private static void Destroy(IntPtr ptr)
        {
            GCHandle.FromIntPtr(ptr).Free();
        }

        [UnmanagedCallersOnly]
        private static void Update(IntPtr self, IntPtr entityRegistry)
        {
            var system = GCHandle.FromIntPtr(self).Target as BehaviourSystem;

            system?.Update(entityRegistry);
        }

        [UnmanagedCallersOnly]
        private static void RegisterBehaviour(IntPtr self, UInt32 entityId, IntPtr assemblyData, UInt32 assemblySize)
        {
            try
            {
                byte[] managedArray = new byte[assemblySize];
                Marshal.Copy(assemblyData, managedArray, 0, (int)assemblySize);

                var assembly = Assembly.Load(managedArray);

                var system = GCHandle.FromIntPtr(self).Target as BehaviourSystem;
                system?.RegisterBehaviour(entityId, assembly);
            }
            catch (Exception ex)
            {
                Log.Error($"Failed to load assembly: {ex.Message}");
            }

        }

        private class UpdateContext : IUpdateContext
        {
            public Entity Entity { get; set; }

            public TimeSpan DeltaTime { get; set; }

        }
    }

}
