using System.Runtime.InteropServices;

namespace Oblo.Runtime
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
