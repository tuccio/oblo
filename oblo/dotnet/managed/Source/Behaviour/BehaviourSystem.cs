using System.Runtime.InteropServices;

namespace Oblo
{
    public class BehaviourSystem
    {
        [UnmanagedCallersOnly]
        public static IntPtr Create()
        {
            BehaviourSystem system = new();

            var pinned = GCHandle.Alloc(system, GCHandleType.Pinned);

            return GCHandle.ToIntPtr(pinned);
        }

        [UnmanagedCallersOnly]
        public static void Destroy(IntPtr ptr)
        {
            GCHandle.FromIntPtr(ptr).Free();
        }

        [UnmanagedCallersOnly]
        public static void Update(IntPtr ptr)
        {
            var system = GCHandle.FromIntPtr(ptr).Target as BehaviourSystem;

            system?.Update();
        }

        void Update()
        {
            Log.Info($"Update called on {nameof(BehaviourSystem)}");
        }
    }
}
