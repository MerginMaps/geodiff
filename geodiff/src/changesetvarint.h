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


int readVarint32( const unsigned char *ptr, u32 *value );

int writeVarint32( unsigned char *ptr, int value );

#endif // CHANGESETVARINT_H
