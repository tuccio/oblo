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

        [StructLayout(LayoutKind.Sequential)]
        internal struct UpdateNativeContext
        {
            public IntPtr EntityRegistry;
            public Int64 DeltaTime;
        }

        private UpdateContext _updateContext = new();
        private List<int> _entitiesMap = new();
        private List<BehaviourEntity> _entities = new();

        internal void RegisterBehaviour(uint entityId, Assembly assembly)
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
                            var id = new EntityId(entityId);

                            var index = (int)id.ExtractIndex();

                            if (index >= _entitiesMap.Count)
                            {
                                _entitiesMap.Capacity = Math.Max(index + 1, (int)(_entitiesMap.Capacity * 1.5f));

                                while (_entitiesMap.Count <= index)
                                {
                                    _entitiesMap.Add(0);
                                }
                            }

                            var denseIndex = _entitiesMap[index];

                            if (denseIndex < _entities.Count && _entities[denseIndex].EntityId == id)
                            {
                                _entities[denseIndex] = new BehaviourEntity { EntityId = new EntityId(entityId), Behaviour = behaviour };
                            }
                            else
                            {
                                _entities.Add(new BehaviourEntity { EntityId = new EntityId(entityId), Behaviour = behaviour });
                            }
                        }

                        break;
                    }
                }
            }
        }

        internal void Update(in UpdateNativeContext nativeContext)
        {
            IntPtr entityRegistry = nativeContext.EntityRegistry;

            Bindings.oblo_ecs_register_types(entityRegistry);

            _updateContext.DeltaTime = TimeSpan.FromTicks(nativeContext.DeltaTime);

            for (int i = 0; i < _entities.Count;)
            {
                var e = _entities[i];

                _updateContext.Entity = new Entity(entityRegistry, e.EntityId);

                if (!_updateContext.Entity.GetComponent<BehaviourComponent>().IsAlive)
                {
                    int last = _entities.Count - 1;
                    _entities[i] = _entities[last];
                    _entities.RemoveAt(last);
                    continue;
                }

                e.Behaviour.OnUpdate(_updateContext);
                ++i;
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
        private static void Update(IntPtr self, UpdateNativeContext ctx)
        {
            var system = GCHandle.FromIntPtr(self).Target as BehaviourSystem;

            system?.Update(ctx);
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
