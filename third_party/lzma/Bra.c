/* Bra.c -- Branch converter for x86 code
2023-04-02 : Igor Pavlov : Public domain */

#include "Precomp.h"
#include "Bra.h"

#define Test86MSByte(b) ((((b) + 1) & 0xFE) == 0)

SizeT x86_Convert(Byte *data, SizeT size, UInt32 ip, UInt32 *state, int encoding)
{
  SizeT pos = 0;
  UInt32 mask = *state & 7;

  if (size < 5)
    return 0;

  size -= 4;
  ip += 5;

  for (;;)
  {
    Byte *p = data + pos;
    const Byte *limit = data + size;

    for (; p < limit; p++)
      if ((*p & 0xFE) == 0xE8)
        break;

    {
      SizeT d = (SizeT)(p - data - pos);
      pos = (SizeT)(p - data);

      if (p >= limit)
      {
        *state = (d > 2 ? 0 : mask >> (unsigned)d);
        return pos;
      }

      if (d > 2)
        mask = 0;
      else
      {
        mask >>= (unsigned)d;
        if (mask != 0 && (mask > 4 || mask == 3 || Test86MSByte(p[(size_t)(mask >> 1) + 1])))
        {
          mask = (mask >> 1) | 4;
          pos++;
          continue;
        }
      }
    }

    if (Test86MSByte(p[4]))
    {
      UInt32 v = ((UInt32)p[4] << 24) | ((UInt32)p[3] << 16) |
                 ((UInt32)p[2] << 8) | ((UInt32)p[1]);
      UInt32 cur = ip + (UInt32)pos;
      pos += 5;

      if (encoding)
        v += cur;
      else
        v -= cur;

      if (Test86MSByte((Byte)(v >> 24)))
      {
        Byte b = (Byte)(v >> 24);
        v = (v & 0x00FFFFFF) | ((UInt32)(0 - (b & 1)) & 0xFF000000);
      }

      p[1] = (Byte)v;
      p[2] = (Byte)(v >> 8);
      p[3] = (Byte)(v >> 16);
      p[4] = (Byte)(~(((v >> 24) & 1) - 1));
      mask = 0;
    }
    else
    {
      mask = (mask >> 1) | 4;
      pos++;
    }
  }
}
