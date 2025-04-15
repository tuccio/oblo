using System;
using System.Runtime.CompilerServices;
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

        public static Uuid Parse(string str)
        {
            return FromGuid(Guid.Parse(str));
        }

        public static Uuid FromGuid(in Guid guid)
        {
            ReadOnlySpan<Guid> guidSpan = MemoryMarshal.CreateSpan(ref Unsafe.AsRef(in guid), 1);
            ReadOnlySpan<byte> guidBytes = MemoryMarshal.Cast<Guid, byte>(guidSpan);

            // Reshuffle to big-endian (UUID format)
            ReadOnlySpan<byte> uuidBytes = stackalloc byte[16]
            {
                // Reverse Data1 (4 bytes)
                guidBytes[3], guidBytes[2], guidBytes[1], guidBytes[0],
                // Reverse Data2 (2 bytes)
                guidBytes[5], guidBytes[4],
                // Reverse Data3 (2 bytes)
                guidBytes[7], guidBytes[6],
                // Data4 (8 bytes) is already in correct order
                guidBytes[8], guidBytes[9], guidBytes[10], guidBytes[11],
                guidBytes[12], guidBytes[13], guidBytes[14], guidBytes[15]
            };

            // Reorder from Win32 GUID layout to standard UUID layout
            return new Uuid(uuidBytes);
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
