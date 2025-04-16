using System.Runtime.InteropServices;

namespace Oblo
{
    public static class Log
    {
        public static void Debug(string message)
        {
            oblo_log_debug(message);
        }

        public static void Info(string message)
        {
            oblo_log_info(message);
        }

        public static void Warn(string message)
        {
            oblo_log_warn(message);
        }

        public static void Error(string message)
        {
            oblo_log_error(message);
        }

        private const string ImportLibrary = "oblo_dotnet_bindings";

        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_log_debug([MarshalAs(UnmanagedType.LPUTF8Str)] string message);

        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_log_info([MarshalAs(UnmanagedType.LPUTF8Str)] string message);

        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_log_warn([MarshalAs(UnmanagedType.LPUTF8Str)] string message);

        [DllImport(ImportLibrary, CallingConvention = CallingConvention.Cdecl)]
        private static extern void oblo_log_error([MarshalAs(UnmanagedType.LPUTF8Str)] string message);
    }
}