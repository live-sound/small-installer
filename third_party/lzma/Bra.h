/* Bra.h -- Branch converter for x86 code
2023-04-02 : Igor Pavlov : Public domain */

#ifndef ZIP7_INC_BRA_H
#define ZIP7_INC_BRA_H

#include "7zTypes.h"

EXTERN_C_BEGIN

/*
x86_Convert:
  Converts x86 CALL/JMP relative addresses to absolute (encoding=1)
  or absolute back to relative (encoding=0).

  data     - buffer to filter in-place
  size     - buffer size
  ip       - virtual address base (use 0)
  state    - pointer to UInt32, must be initialized to 0 before first call
  encoding - 1 for encode (before compression), 0 for decode (after decompression)

  Returns: number of bytes processed (may be < size for boundary)
*/
SizeT x86_Convert(Byte *data, SizeT size, UInt32 ip, UInt32 *state, int encoding);

EXTERN_C_END

#endif
