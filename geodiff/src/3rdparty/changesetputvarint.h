/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef CHANGESETPUTVARINT_H
#define CHANGESETPUTVARINT_H

#include <stdint.h>

// Contents of this file is entirely based on code from sqlite3
//
// the following macro should be used for varint writing:
// - putVarint32


typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;


#define putVarint32(A,B)  \
  (u8)(((u32)(B)<(u32)0x80)?(*(A)=(unsigned char)(B)),1:\
       sqlite3PutVarint((A),(B)))

/*
** Write a 64-bit variable-length integer to memory starting at p[0].
** The length of data write will be between 1 and 9 bytes.  The number
** of bytes written is returned.
**
** A variable-length integer consists of the lower 7 bits of each byte
** for all bytes that have the 8th bit set and one byte with the 8th
** bit clear.  Except, if we get to the 9th byte, it stores the full
** 8 bits and is the last byte.
*/
static int putVarint64( unsigned char *p, u64 v )
{
  int i, j, n;
  u8 buf[10];
  if ( v & ( ( ( u64 )0xff000000 ) << 32 ) )
  {
    p[8] = ( u8 )v;
    v >>= 8;
    for ( i = 7; i >= 0; i-- )
    {
      p[i] = ( u8 )( ( v & 0x7f ) | 0x80 );
      v >>= 7;
    }
    return 9;
  }
  n = 0;
  do
  {
    buf[n++] = ( u8 )( ( v & 0x7f ) | 0x80 );
    v >>= 7;
  }
  while ( v != 0 );
  buf[0] &= 0x7f;
  assert( n <= 9 );
  for ( i = 0, j = n - 1; j >= 0; j--, i++ )
  {
    p[i] = buf[j];
  }
  return n;
}

static int sqlite3PutVarint( unsigned char *p, u64 v )
{
  if ( v <= 0x7f )
  {
    p[0] = v & 0x7f;
    return 1;
  }
  if ( v <= 0x3fff )
  {
    p[0] = ( ( v >> 7 ) & 0x7f ) | 0x80;
    p[1] = v & 0x7f;
    return 2;
  }
  return putVarint64( p, v );
}

#endif // CHANGESETPUTVARINT_H
