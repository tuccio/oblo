using System.Reflection;
using System.Runtime.CompilerServices;

namespace Oblo
{
    internal static class AssemblyLoader
    {
        [ModuleInitializer]
        public static void Init()
        {
            AppDomain.CurrentDomain.AssemblyResolve += CurrentDomain_AssemblyResolve;
        }

        private static Assembly CurrentDomain_AssemblyResolve(object? sender, ResolveEventArgs args)
        {
            string? assemblyName = new AssemblyName(args.Name).Name;

            if (assemblyName == "Oblo.Managed")
            {
                return Assembly.GetExecutingAssembly();
            }

            if (assemblyName is null)
            {
                throw new ArgumentException();
            }

            return Assembly.LoadFile($"{assemblyName}.dll");
        }
    }
}
