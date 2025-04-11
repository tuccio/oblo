using System.Runtime.InteropServices;

namespace Oblo.Ecs
{
    public record struct EntityId(uint Value);
    public record struct ComponentTypeId(uint Value);
    public record struct Entity(IntPtr EentityRegistry, EntityId Id)
    {
        T GetComponent<T>()
        {
            // TODO
            return default;
        }
    }


    public static class Bindings
    {
        private const string ImportLibrary = "oblo_dotnet_bindings";

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out float result)
        {
            oblo_ecs_raw_get_float(e.EentityRegistry, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, float value)
        {
            oblo_ecs_raw_set_float(e.EentityRegistry, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_ecs_raw_get_float(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out float result);

        [SuppressGCTransition]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_ecs_raw_set_float(IntPtr registry, uint entityId, uint componentTypeId, uint offset, float value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out int result)
        {
            oblo_ecs_raw_get_int(e.EentityRegistry, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, int value)
        {
            oblo_ecs_raw_set_int(e.EentityRegistry, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_ecs_raw_get_int(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out int result);

        [SuppressGCTransition]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_ecs_raw_set_int(IntPtr registry, uint entityId, uint componentTypeId, uint offset, int value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out uint result)
        {
            oblo_ecs_raw_get_uint(e.EentityRegistry, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, uint value)
        {
            oblo_ecs_raw_set_uint(e.EentityRegistry, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_ecs_raw_get_uint(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out uint result);

        [SuppressGCTransition]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_ecs_raw_set_uint(IntPtr registry, uint entityId, uint componentTypeId, uint offset, uint value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out double result)
        {
            oblo_ecs_raw_get_double(e.EentityRegistry, e.Id.Value, componentTypeId.Value, offset, out result);
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, double value)
        {
            oblo_ecs_raw_set_double(e.EentityRegistry, e.Id.Value, componentTypeId.Value, offset, value);
        }

        [SuppressGCTransition]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_ecs_raw_get_double(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out double result);

        [SuppressGCTransition]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_ecs_raw_set_double(IntPtr registry, uint entityId, uint componentTypeId, uint offset, double value);

        public static void GetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, out bool result)
        {
            byte raw;
            oblo_ecs_raw_get_bool(e.EentityRegistry, e.Id.Value, componentTypeId.Value, offset, out raw);
            result = raw != 0;
        }

        public static void SetComponentPropertyRaw(Entity e, ComponentTypeId componentTypeId, uint offset, bool value)
        {
            oblo_ecs_raw_set_bool(e.EentityRegistry, e.Id.Value, componentTypeId.Value, offset, (byte)(value ? 1 : 0));
        }

        [SuppressGCTransition]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_ecs_raw_get_bool(IntPtr registry, uint entityId, uint componentTypeId, uint offset, out byte result);

        [SuppressGCTransition]
        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_ecs_raw_set_bool(IntPtr registry, uint entityId, uint componentTypeId, uint offset, byte value);
    }

}
