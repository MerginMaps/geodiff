/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "changesetwriter.h"

#include "geodiffutils.hpp"
#include "changesetvarint.h"
#include "portableendian.h"

#include <assert.h>
#include <memory.h>

#include <sstream>


bool ChangesetWriter::open( const std::string &filename )
{
#ifdef WIN32
  mFile.open( stringToWString( filename ), std::ios::out | std::ios::binary );
#else
  mFile.open( filename, std::ios::out | std::ios::binary );
#endif
  if ( !mFile.is_open() )
    return false;

  return true;
}

void ChangesetWriter::beginTable( const ChangesetTable &table )
{
  mCurrentTable = table;

  writeByte( 'T' );
  writeVarint( table.columnCount() );
  for ( size_t i = 0; i < table.columnCount(); ++i )
    writeByte( table.primaryKeys[i] );
  writeNullTerminatedString( table.name );
}

void ChangesetWriter::writeEntry( const ChangesetEntry &entry )
{
  if ( entry.op != ChangesetEntry::OpInsert && entry.op != ChangesetEntry::OpUpdate && entry.op != ChangesetEntry::OpDelete )
    throw GeoDiffException( "wrong op for changeset entry" );
  writeByte( entry.op );
  writeByte( 0 );  // "indirect" always false

  if ( entry.op != ChangesetEntry::OpInsert )
    writeRowValues( entry.oldValues );
  if ( entry.op != ChangesetEntry::OpDelete )
    writeRowValues( entry.newValues );
}

void ChangesetWriter::writeByte( char c )
{
  mFile.write( &c, 1 );
}

void ChangesetWriter::writeVarint( int n )
{
  unsigned char output[9];  // 1-9 bytes
  int numBytes = putVarint32( output, n );
  mFile.write( ( char * )output, numBytes );
}

void ChangesetWriter::writeNullTerminatedString( const std::string &str )
{
  mFile.write( str.c_str(), str.size() + 1 );
}

void ChangesetWriter::writeRowValues( const std::vector<Value> &values )
{
  if ( values.size() != mCurrentTable.columnCount() )
    throw GeoDiffException( "wrong number of rows in the entry" );

  for ( size_t i = 0; i < mCurrentTable.columnCount(); ++i )
  {
    Value::Type type = values[i].type();
    writeByte( type );
    if ( type == Value::TypeInt ) // 0x01
    {
      // 64-bit int (big endian)
      uint64_t x;
      int64_t v = values[i].getInt();
      memcpy( &x, &v, 8 );
      x = htobe64( x ); // convert host to big endian
      mFile.write( ( char * )&x, 8 );
    }
    else if ( type == Value::TypeDouble ) // 0x02
    {
      // 64-bit double (big endian)
      int64_t x;
      double v = values[i].getDouble();
      memcpy( &x, &v, 8 );
      x = htobe64( x ); // convert host to big endian
      mFile.write( ( char * )&x, 8 );
    }
    else if ( type == Value::TypeText || type == Value::TypeBlob ) // 0x03 or 0x04
    {
      const std::string &str = values[i].getString();
      writeVarint( str.size() );
      mFile.write( str.c_str(), str.size() );
    }
    else if ( type == Value::TypeNull ) // 0x05
    {
      // nothing extra to write
    }
    else if ( type == Value::TypeUndefined )  // undefined value  (different from NULL)
    {
      // nothing extra to write
    }
    else
    {
      throw GeoDiffException( "unexpected entry type" );
    }
  }
}
