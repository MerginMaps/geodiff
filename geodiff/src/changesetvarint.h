/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef CHANGESETVARINT_H
#define CHANGESETVARINT_H

#include <stdint.h>

// Contents of this file is entirely based on code from sqlite3
//
// the following two macros should be used for varint reading/writing:
// - getVarint32
// - putVarint32


typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;



#define getVarint32(A,B)  \
  (u8)((*(A)<(u8)0x80)?((B)=(u32)*(A)),1:sqlite3GetVarint32((A),(u32 *)&(B)))
#define putVarint32(A,B)  \
  (u8)(((u32)(B)<(u32)0x80)?(*(A)=(unsigned char)(B)),1:\
       sqlite3PutVarint((A),(B)))


/*
** Bitmasks used by sqlite3GetVarint().  These precomputed constants
** are defined here rather than simply putting the constant expressions
** inline in order to work around bugs in the RVT compiler.
**
** SLOT_2_0     A mask for  (0x7f<<14) | 0x7f
**
** SLOT_4_2_0   A mask for  (0x7f<<28) | SLOT_2_0
*/
#define SLOT_2_0     0x001fc07f
#define SLOT_4_2_0   0xf01fc07f

/*
** SQLITE_MAX_U32 is a u64 constant that is the maximum u64 value
** that can be stored in a u32 without loss of data.  The value
** is 0x00000000ffffffff.  But because of quirks of some compilers, we
** have to specify the value in the less intuitive manner shown:
*/
#define SQLITE_MAX_U32  ((((u64)1)<<32)-1)


/*
** Read a 64-bit variable-length integer from memory starting at p[0].
** Return the number of bytes read.  The value is stored in *v.
*/
static u8 sqlite3GetVarint( const unsigned char *p, u64 *v )
{
  u32 a, b, s;

  if ( ( ( signed char * )p )[0] >= 0 )
  {
    *v = *p;
    return 1;
  }
  if ( ( ( signed char * )p )[1] >= 0 )
  {
    *v = ( ( u32 )( p[0] & 0x7f ) << 7 ) | p[1];
    return 2;
  }

  /* Verify that constants are precomputed correctly */
  assert( SLOT_2_0 == ( ( 0x7f << 14 ) | ( 0x7f ) ) );
  assert( SLOT_4_2_0 == ( ( 0xfU << 28 ) | ( 0x7f << 14 ) | ( 0x7f ) ) );

  a = ( ( u32 )p[0] ) << 14;
  b = p[1];
  p += 2;
  a |= *p;
  /* a: p0<<14 | p2 (unmasked) */
  if ( !( a & 0x80 ) )
  {
    a &= SLOT_2_0;
    b &= 0x7f;
    b = b << 7;
    a |= b;
    *v = a;
    return 3;
  }

  /* CSE1 from below */
  a &= SLOT_2_0;
  p++;
  b = b << 14;
  b |= *p;
  /* b: p1<<14 | p3 (unmasked) */
  if ( !( b & 0x80 ) )
  {
    b &= SLOT_2_0;
    /* moved CSE1 up */
    /* a &= (0x7f<<14)|(0x7f); */
    a = a << 7;
    a |= b;
    *v = a;
    return 4;
  }

  /* a: p0<<14 | p2 (masked) */
  /* b: p1<<14 | p3 (unmasked) */
  /* 1:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
  /* moved CSE1 up */
  /* a &= (0x7f<<14)|(0x7f); */
  b &= SLOT_2_0;
  s = a;
  /* s: p0<<14 | p2 (masked) */

  p++;
  a = a << 14;
  a |= *p;
  /* a: p0<<28 | p2<<14 | p4 (unmasked) */
  if ( !( a & 0x80 ) )
  {
    /* we can skip these cause they were (effectively) done above
    ** while calculating s */
    /* a &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
    /* b &= (0x7f<<14)|(0x7f); */
    b = b << 7;
    a |= b;
    s = s >> 18;
    *v = ( ( u64 )s ) << 32 | a;
    return 5;
  }

  /* 2:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
  s = s << 7;
  s |= b;
  /* s: p0<<21 | p1<<14 | p2<<7 | p3 (masked) */

  p++;
  b = b << 14;
  b |= *p;
  /* b: p1<<28 | p3<<14 | p5 (unmasked) */
  if ( !( b & 0x80 ) )
  {
    /* we can skip this cause it was (effectively) done above in calc'ing s */
    /* b &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
    a &= SLOT_2_0;
    a = a << 7;
    a |= b;
    s = s >> 18;
    *v = ( ( u64 )s ) << 32 | a;
    return 6;
  }

  p++;
  a = a << 14;
  a |= *p;
  /* a: p2<<28 | p4<<14 | p6 (unmasked) */
  if ( !( a & 0x80 ) )
  {
    a &= SLOT_4_2_0;
    b &= SLOT_2_0;
    b = b << 7;
    a |= b;
    s = s >> 11;
    *v = ( ( u64 )s ) << 32 | a;
    return 7;
  }

  /* CSE2 from below */
  a &= SLOT_2_0;
  p++;
  b = b << 14;
  b |= *p;
  /* b: p3<<28 | p5<<14 | p7 (unmasked) */
  if ( !( b & 0x80 ) )
  {
    b &= SLOT_4_2_0;
    /* moved CSE2 up */
    /* a &= (0x7f<<14)|(0x7f); */
    a = a << 7;
    a |= b;
    s = s >> 4;
    *v = ( ( u64 )s ) << 32 | a;
    return 8;
  }

  p++;
  a = a << 15;
  a |= *p;
  /* a: p4<<29 | p6<<15 | p8 (unmasked) */

  /* moved CSE2 up */
  /* a &= (0x7f<<29)|(0x7f<<15)|(0xff); */
  b &= SLOT_2_0;
  b = b << 8;
  a |= b;

  s = s << 4;
  b = p[-4];
  b &= 0x7f;
  b = b >> 3;
  s |= b;

  *v = ( ( u64 )s ) << 32 | a;

  return 9;
}

/*
** Read a 32-bit variable-length integer from memory starting at p[0].
** Return the number of bytes read.  The value is stored in *v.
**
** If the varint stored in p[0] is larger than can fit in a 32-bit unsigned
** integer, then set *v to 0xffffffff.
**
** A MACRO version, getVarint32, is provided which inlines the
** single-byte case.  All code should use the MACRO version as
** this function assumes the single-byte case has already been handled.
*/
static u8 sqlite3GetVarint32( const unsigned char *p, u32 *v )
{
  u32 a, b;

  /* The 1-byte case.  Overwhelmingly the most common.  Handled inline
  ** by the getVarin32() macro */
  a = *p;
  /* a: p0 (unmasked) */
#ifndef getVarint32
  if ( !( a & 0x80 ) )
  {
    /* Values between 0 and 127 */
    *v = a;
    return 1;
  }
#endif

  /* The 2-byte case */
  p++;
  b = *p;
  /* b: p1 (unmasked) */
  if ( !( b & 0x80 ) )
  {
    /* Values between 128 and 16383 */
    a &= 0x7f;
    a = a << 7;
    *v = a | b;
    return 2;
  }

  /* The 3-byte case */
  p++;
  a = a << 14;
  a |= *p;
  /* a: p0<<14 | p2 (unmasked) */
  if ( !( a & 0x80 ) )
  {
    /* Values between 16384 and 2097151 */
    a &= ( 0x7f << 14 ) | ( 0x7f );
    b &= 0x7f;
    b = b << 7;
    *v = a | b;
    return 3;
  }

  {
    u64 v64;
    u8 n;

    p -= 2;
    n = sqlite3GetVarint( p, &v64 );
    assert( n > 3 && n <= 9 );
    if ( ( v64 & SQLITE_MAX_U32 ) != v64 )
    {
      *v = 0xffffffff;
    }
    else
    {
      *v = ( u32 )v64;
    }
    return n;
  }
}

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

#endif // CHANGESETVARINT_H
