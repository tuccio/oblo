using System;
using System.Runtime.InteropServices;

namespace Oblo
{
    public interface IResource
    { }

    [StructLayout(LayoutKind.Sequential)]
    public struct ResourceRef<T> where T : struct, IResource
    {
        public Guid Id => _id;

        private Guid _id;

        public ResourceRef(Guid id)
        {
            _id = id;
        }
    }
}
