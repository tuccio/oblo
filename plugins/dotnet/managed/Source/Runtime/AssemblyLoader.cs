using System.Reflection;

namespace Oblo
{
    internal static class AssemblyLoader
    {
        private static bool _isInitialized = false;

        public static void Init()
        {
            if (!_isInitialized)
            {
                AppDomain.CurrentDomain.AssemblyResolve += CurrentDomain_AssemblyResolve;
                _isInitialized = true;
            }
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
