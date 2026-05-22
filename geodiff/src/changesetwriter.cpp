/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "changesetwriter.h"

#include "changeset.h"
#include "geodiffutils.hpp"
#include "changesetputvarint.h"
#include "portableendian.h"

#include <assert.h>
#include <memory.h>

#include <sstream>
#include <variant>

void ChangesetWriter::open( const std::string &filename )
{
#ifdef WIN32
  mFile.open( stringToWString( filename ), std::ios::out | std::ios::binary );
#else
  mFile.open( filename, std::ios::out | std::ios::binary );
#endif
  if ( !mFile.is_open() )
    throw GeoDiffException( "Unable to open changeset file for writing: " + filename );
}

void ChangesetWriter::beginTable( const ChangesetTable &table )
{
  mCurrentTable = table;

  writeByte( ( int ) ChangesetEntryType::OpTableRecord );
  writeVarint( ( int ) table.columnCount() );
  for ( size_t i = 0; i < table.columnCount(); ++i )
    writeByte( table.primaryKeys[i] );
  writeNullTerminatedString( table.name );
}

void ChangesetWriter::writeEntry( const ChangesetEntry &entry )
{
  if ( const ChangesetDataEntry *dataEntry = std::get_if<ChangesetDataEntry>( &entry ) )
    writeDataEntry( *dataEntry );
  else if ( const ChangesetCreateTableEntry *ctEntry = std::get_if<ChangesetCreateTableEntry>( &entry ) )
    writeCreateTableEntry( *ctEntry );
  else if ( const ChangesetDropTableEntry *dtEntry = std::get_if<ChangesetDropTableEntry>( &entry ) )
    writeDropTableEntry( *dtEntry );
  else if ( const ChangesetAddColumnEntry *acEntry = std::get_if<ChangesetAddColumnEntry>( &entry ) )
    writeAddColumnEntry( *acEntry );
  else if ( const ChangesetDropColumnEntry *dcEntry = std::get_if<ChangesetDropColumnEntry>( &entry ) )
    writeDropColumnEntry( *dcEntry );
  else
    throw GeoDiffException( "Tried to write unhandled changeset entry type! " +
                            std::to_string( entry.index() ) );
}

void ChangesetWriter::writeByte( char c )
{
  mFile.write( &c, 1 );
}

void ChangesetWriter::writeVarint( int n )
{
  unsigned char output[9];  // 1-9 bytes
  int numBytes = putVarint32( output, n );
  mFile.write( reinterpret_cast<char *>( output ), numBytes );
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
    writeByte( ( char ) type );
    if ( type == Value::TypeInt ) // 0x01
    {
      // 64-bit int (big endian)
      uint64_t x;
      int64_t v = values[i].getInt();
      memcpy( &x, &v, 8 );
      x = htobe64( x ); // convert host to big endian
      mFile.write( reinterpret_cast<char *>( &x ), 8 );
    }
    else if ( type == Value::TypeDouble ) // 0x02
    {
      // 64-bit double (big endian)
      int64_t x;
      double v = values[i].getDouble();
      memcpy( &x, &v, 8 );
      x = htobe64( x ); // convert host to big endian
      mFile.write( reinterpret_cast<char *>( &x ), 8 );
    }
    else if ( type == Value::TypeText || type == Value::TypeBlob ) // 0x03 or 0x04
    {
      const std::string &str = values[i].getString();
      writeVarint( static_cast<int>( str.size() ) );
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

void ChangesetWriter::writeColumnInfo( const TableColumnInfo &column )
{
  writeNullTerminatedString( column.name );
  writeByte( static_cast<char>( column.type.baseType ) );
  writeByte( ( column.isPrimaryKey << 0 )
             | ( column.isNotNull << 1 )
             | ( column.isAutoIncrement << 2 )
             | ( column.isGeometry << 3 )
             | ( column.geomHasZ << 4 )
             | ( column.geomHasM << 5 ) );
  writeNullTerminatedString( column.geomType );
  writeVarint( column.geomSrsId );
}


void ChangesetWriter::writeDataEntry( const ChangesetDataEntry &entry )
{
  if ( entry.op != ChangesetDataEntry::OpInsert && entry.op != ChangesetDataEntry::OpUpdate && entry.op != ChangesetDataEntry::OpDelete )
    throw GeoDiffException( "wrong op for changeset entry" );
  writeByte( ( char ) entry.op );
  writeByte( 0 );  // "indirect" always false

  if ( entry.op != ( int ) ChangesetEntryType::OpInsert )
    writeRowValues( entry.oldValues );
  if ( entry.op != ( int ) ChangesetEntryType::OpDelete )
    writeRowValues( entry.newValues );
}

void ChangesetWriter::writeCreateTableEntry( const ChangesetCreateTableEntry &entry )
{
  writeByte( static_cast<char>( ChangesetEntryType::OpCreateTable ) );
  writeNullTerminatedString( entry.tableName );
  writeVarint( static_cast<int>( entry.columns.size() ) );
  for ( const TableColumnInfo &column : entry.columns )
  {
    writeColumnInfo( column );
  }
}

void ChangesetWriter::writeDropTableEntry( const ChangesetDropTableEntry &entry )
{
  writeByte( static_cast<char>( ChangesetEntryType::OpDropTable ) );
  writeNullTerminatedString( entry.tableName );
  writeVarint( static_cast<int>( entry.columns.size() ) );
  for ( const TableColumnInfo &column : entry.columns )
  {
    writeColumnInfo( column );
  }
}

void ChangesetWriter::writeAddColumnEntry( const ChangesetAddColumnEntry &entry )
{
  writeByte( static_cast<char>( ChangesetEntryType::OpAddColumn ) );
  writeNullTerminatedString( entry.tableName );
  writeColumnInfo( entry.column );
}

void ChangesetWriter::writeDropColumnEntry( const ChangesetDropColumnEntry &entry )
{
  writeByte( static_cast<char>( ChangesetEntryType::OpDropColumn ) );
  writeNullTerminatedString( entry.tableName );
  writeColumnInfo( entry.column );
}
