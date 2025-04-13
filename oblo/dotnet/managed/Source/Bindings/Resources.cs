using Oblo;
using System.Runtime.InteropServices;

namespace Oblo
{
    public interface IResource
    { }

    [StructLayout(LayoutKind.Sequential)]
    public struct ResourceRef<T> where T : struct, IResource
    {
        public Uuid Id => _id;

        private Uuid _id;

        public ResourceRef(Uuid id)
        {
            _id = id;
        }
    }
}
