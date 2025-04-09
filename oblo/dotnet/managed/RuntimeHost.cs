using System.Runtime.InteropServices;

namespace Oblo.Managed
{
    public class RuntimeHost
    {
        [UnmanagedCallersOnly]
        public static void Init()
        {
            Console.WriteLine("Called RuntimeHost.Init()");
        }
    }
}
