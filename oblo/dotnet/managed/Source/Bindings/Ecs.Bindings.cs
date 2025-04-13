using System.Numerics;
using System.Reflection.Emit;
using System.Runtime.InteropServices;
using System.Security;

namespace Oblo.Ecs
{
    public static class Bindings
    {
        private const string ImportLibrary = "oblo_dotnet_bindings";

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out float result)
        {
            oblo_ecs_property_get_float(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, in float value)
        {
            oblo_ecs_property_set_float(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_get_float(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out float result);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_set_float(IntPtr registry, uint entityId, uint componentTypeId, uint offset, float value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out int result)
        {
            oblo_ecs_property_get_int(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, in int value)
        {
            oblo_ecs_property_set_int(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_get_int(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out int result);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_set_int(IntPtr registry, uint entityId, uint componentTypeId, uint offset, int value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out uint result)
        {
            oblo_ecs_property_get_uint(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, in uint value)
        {
            oblo_ecs_property_set_uint(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_get_uint(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out uint result);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_set_uint(IntPtr registry, uint entityId, uint componentTypeId, uint offset, in uint value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out double result)
        {
            oblo_ecs_property_get_double(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, in double value)
        {
            oblo_ecs_property_set_double(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_get_double(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out double result);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_set_double(IntPtr registry, uint entityId, uint componentTypeId, uint offset, double value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out bool result)
        {
            byte raw;
            oblo_ecs_property_get_bool(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, out raw);
            result = raw != 0;
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, in bool value)
        {
            oblo_ecs_property_set_bool(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, (byte)(value ? 1 : 0));
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_get_bool(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out byte result);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_set_bool(IntPtr registry, uint entityId, uint componentTypeId, uint offset, byte value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out byte result)
        {
            oblo_ecs_property_get_byte(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, in byte value)
        {
            oblo_ecs_property_set_byte(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_get_byte(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out byte result);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_set_byte(IntPtr registry, uint entityId, uint componentTypeId, uint offset, byte value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out Vector3 result)
        {
            oblo_ecs_property_get_vec3(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, in Vector3 value)
        {
            oblo_ecs_property_set_vec3(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_get_vec3(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out Vector3 result);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_set_vec3(IntPtr registry, uint entityId, uint componentTypeId, uint offset, in Vector3 value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out Quaternion result)
        {
            oblo_ecs_property_get_quaternion(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, in Quaternion value)
        {
            oblo_ecs_property_set_quaternion(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_get_quaternion(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out Quaternion result);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_set_quaternion(IntPtr registry, uint entityId, uint componentTypeId, uint offset, in Quaternion value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out string result)
        {
            var ptr = oblo_ecs_property_get_string(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, out int len);
            result = Marshal.PtrToStringUTF8(ptr, len);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, in string value)
        {
            oblo_ecs_property_set_string(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, value, value.Length);
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr oblo_ecs_property_get_string(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out int len);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_set_string(IntPtr registry, uint entityId, uint componentTypeId, uint offset, string value, int len);

        public static void GetComponentPropertyRaw<T>(Entity e, ComponentTypeId componentTypeId, uint offset, out ResourceRef<T> result) where T : struct, IResource
        {
            oblo_ecs_property_get_guid(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, out Guid id);
            result = new ResourceRef<T>(id);
        }

        public static void SetComponentPropertyRaw<T>(Entity e, ComponentTypeId componentTypeId, uint offset, in ResourceRef<T> value) where T : struct, IResource
        {
            oblo_ecs_property_set_guid(e.EntityRegistry.NativeHandle, e.Id.Value, componentTypeId.Value, offset, value.Id);
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_get_guid(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out Guid result);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_property_set_guid(IntPtr registry, uint entityId, uint componentTypeId, uint offset, in Guid value);

        public static T AddComponent<T>(Entity entity) where T : struct, IComponent
        {
            var typeId = ComponentTraits<T>.TypeId;
            oblo_ecs_component_add(entity.EntityRegistry.NativeHandle, entity.Id.Value, typeId, 1);
            return ComponentFactory<T>.Create(entity);
        }

        public static void RemoveComponent<T>(Entity entity) where T : struct, IComponent
        {
            oblo_ecs_component_remove(entity.EntityRegistry.NativeHandle, entity.Id.Value, ComponentTraits<T>.TypeId.Value);
        }

        public static T GetComponent<T>(Entity entity) where T : struct, IComponent
        {
            return ComponentFactory<T>.Create(entity);
        }

        public static bool HasComponent<T>(Entity entity) where T : IComponent
        {
            return oblo_ecs_component_exists(entity.EntityRegistry.NativeHandle, entity.Id.Value, ComponentTraits<T>.TypeId.Value);
        }

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern bool oblo_ecs_entity_exists(IntPtr registry, uint entityId);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint oblo_ecs_entity_create(IntPtr registry, in ComponentTypeId components, int componentsCount);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_entity_destroy(IntPtr registry, uint entityId);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_entity_destroy_hierarchy(IntPtr registry, uint entityId);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_entity_reparent(IntPtr registry, uint entityId, uint newParentId);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern bool oblo_ecs_component_exists(IntPtr registry, uint entityId, uint componentTypeId);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_component_add(IntPtr registry, uint entityId, in ComponentTypeId components, int componentsCount);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_component_remove(IntPtr registry, uint entityId, uint componentTypeId);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void oblo_ecs_register_types(IntPtr registry);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint oblo_ecs_find_component_type(string typeName);

        [SuppressGCTransition]
        [SuppressUnmanagedCodeSecurity]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint oblo_ecs_get_entity_index_mask();

        internal static class ComponentFactory<T> where T : struct, IComponent
        {
            public static T Create(Entity e)
            {
                T c = default;
                Initialize(ref c, e);
                return c;
            }

            private delegate void InitializeDelegate(ref T component, Entity e);

            private static InitializeDelegate Initialize { get; } = CreateInitializeDelegate();

            private static InitializeDelegate CreateInitializeDelegate()
            {
                var dynamicMethod = new DynamicMethod(string.Empty, typeof(void), new[] { typeof(T).MakeByRefType(), typeof(Entity) }, true);

                ILGenerator ilGenerator = dynamicMethod.GetILGenerator();

                var entityField = typeof(T).GetField("_entity", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);

                if (entityField is null)
                {
                    throw new InvalidOperationException();
                }

                ilGenerator.Emit(OpCodes.Ldarg_0); // Load 'ref T component' (first argument)
                ilGenerator.Emit(OpCodes.Ldarg_1); // Load 'Entity e' (second argument)
                ilGenerator.Emit(OpCodes.Stfld, entityField); // Set the _entity field

                // Complete the method
                ilGenerator.Emit(OpCodes.Ret);

                // Create a delegate from the dynamically generated method
                return (InitializeDelegate)dynamicMethod.CreateDelegate(typeof(InitializeDelegate));
            }
        }
    }
}
