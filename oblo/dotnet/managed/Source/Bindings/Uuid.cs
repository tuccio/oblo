using System;
using System.Runtime.InteropServices;

namespace Oblo
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public readonly struct Uuid
    {
        // RFC 4122 standard layout: 16 bytes
        public readonly byte Byte0;
        public readonly byte Byte1;
        public readonly byte Byte2;
        public readonly byte Byte3;
        public readonly byte Byte4;
        public readonly byte Byte5;
        public readonly byte Byte6;
        public readonly byte Byte7;
        public readonly byte Byte8;
        public readonly byte Byte9;
        public readonly byte Byte10;
        public readonly byte Byte11;
        public readonly byte Byte12;
        public readonly byte Byte13;
        public readonly byte Byte14;
        public readonly byte Byte15;

        public Uuid(ReadOnlySpan<byte> bytes)
        {
            if (bytes.Length != 16)
            {
                throw new ArgumentException("UUID must be exactly 16 bytes", nameof(bytes));
            }

            Byte0 = bytes[0];
            Byte1 = bytes[1];
            Byte2 = bytes[2];
            Byte3 = bytes[3];
            Byte4 = bytes[4];
            Byte5 = bytes[5];
            Byte6 = bytes[6];
            Byte7 = bytes[7];
            Byte8 = bytes[8];
            Byte9 = bytes[9];
            Byte10 = bytes[10];
            Byte11 = bytes[11];
            Byte12 = bytes[12];
            Byte13 = bytes[13];
            Byte14 = bytes[14];
            Byte15 = bytes[15];
        }

        public static Uuid FromGuid(in Guid guid)
        {
            // In .NET 10 we could use TryWriteBytes instead
            Guid copy = guid;

            Span<Guid> guidSpan = MemoryMarshal.CreateSpan(ref copy, 1);
            ReadOnlySpan<byte> bytes = MemoryMarshal.Cast<Guid, byte>(guidSpan);

            // Reorder from Win32 GUID layout to standard UUID layout
            return new Uuid(bytes);
        }

        public Guid ToGuid()
        {
            Span<byte> bytes = stackalloc byte[16];

            bytes[0] = Byte3;
            bytes[1] = Byte2;
            bytes[2] = Byte1;
            bytes[3] = Byte0;
            bytes[4] = Byte5;
            bytes[5] = Byte4;
            bytes[6] = Byte7;
            bytes[7] = Byte6;
            bytes[8] = Byte8;
            bytes[9] = Byte9;
            bytes[10] = Byte10;
            bytes[11] = Byte11;
            bytes[12] = Byte12;
            bytes[13] = Byte13;
            bytes[14] = Byte14;
            bytes[15] = Byte15;

            return new Guid(bytes);
        }

        public override string ToString()
        {
            return ToGuid().ToString();
        }

        public bool Equals(Uuid other)
        {
            for (int i = 0; i < 16; i++)
            {
                if (GetByte(i) != other.GetByte(i))
                    return false;
            }
            return true;
        }

        public byte GetByte(int index) => index switch
        {
            0 => Byte0,
            1 => Byte1,
            2 => Byte2,
            3 => Byte3,
            4 => Byte4,
            5 => Byte5,
            6 => Byte6,
            7 => Byte7,
            8 => Byte8,
            9 => Byte9,
            10 => Byte10,
            11 => Byte11,
            12 => Byte12,
            13 => Byte13,
            14 => Byte14,
            15 => Byte15,
            _ => throw new IndexOutOfRangeException()
        };
    }
}
