using System.Drawing;
using System.Reflection;
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
        public static void Update(IntPtr self)
        {
            var system = GCHandle.FromIntPtr(self).Target as BehaviourSystem;

            system?.Update();
        }

        [UnmanagedCallersOnly]
        public static void RegisterBehaviour(IntPtr self, UInt32 entityId, IntPtr assemblyData, UInt32 assemblySize)
        {
            Log.Info($"{nameof(RegisterBehaviour)} called");

            var system = GCHandle.FromIntPtr(self).Target as BehaviourSystem;

            try
            {
                byte[] managedArray = new byte[assemblySize];
                Marshal.Copy(assemblyData, managedArray, 0, (int)assemblySize);

                var assembly = Assembly.Load(managedArray);

                foreach (Type type in assembly.GetTypes())
                {
                    Log.Info($"Found type: {type.FullName}");
                }
            }
            catch (Exception ex)
            {
                Log.Error("Failed to parse assembly");
            }
        }

        void Update()
        {
            Log.Info($"Update called on C# {nameof(BehaviourSystem)}");
        }
    }
}
