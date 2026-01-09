/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "changesetreader.h"

#include "changeset.h"
#include "geodiffutils.hpp"
#include "changesetgetvarint.h"
#include "portableendian.h"
#include "sqliteutils.h"
#include "tableschema.h"

#include <assert.h>
#include <memory.h>

#include <sstream>


ChangesetReader::ChangesetReader() = default;

ChangesetReader::~ChangesetReader() = default;


bool ChangesetReader::open( const std::string &filename )
{
  try
  {
    mBuffer.reset( new Buffer );
    mBuffer->read( filename );
  }
  catch ( const GeoDiffException & )
  {
    return false;
  }

  return true;
}

bool ChangesetReader::nextEntry( ChangesetEntry &entry )
{
  while ( 1 )
  {
    if ( mOffset >= mBuffer->size() )
      break;   // EOF

    ChangesetEntryType type = static_cast<ChangesetEntryType>( readByte() );
    if ( type == ChangesetEntryType::OpTableRecord )
    {
      readTableRecord();
      // and now continue reading, we want an entry
    }
    else if ( type == ChangesetEntryType::OpInsert || type == ChangesetEntryType::OpUpdate || type == ChangesetEntryType::OpDelete )
    {
      entry = readDataEntry( type );
      return true;  // we're done!
    }
    else if ( type == ChangesetEntryType::OpCreateTable )
    {
      entry = readCreateTableEntry();
      return true;
    }
    else if ( type == ChangesetEntryType::OpDropTable )
    {
      entry = readDropTableEntry();
      return true;
    }
    else if ( type == ChangesetEntryType::OpAddColumn )
    {
      entry = readAddColumnEntry();
      return true;
    }
    else if ( type == ChangesetEntryType::OpDropColumn )
    {
      entry = readDropColumnEntry();
      return true;
    }
    else
    {
      throwReaderError( "Unknown entry type " + std::to_string( static_cast<int>( type ) ) );
    }
  }
  return false;
}

bool ChangesetReader::isEmpty() const
{
  return mBuffer->size() == 0;
}

void ChangesetReader::rewind()
{
  mOffset = 0;
  mCurrentTable = ChangesetTable();
}

char ChangesetReader::readByte()
{
  if ( mOffset >= mBuffer->size() )
    throwReaderError( "readByte: at the end of buffer" );
  const char *ptr = mBuffer->c_buf() + mOffset;
  ++mOffset;
  return *ptr;
}

int ChangesetReader::readVarint()
{
  u32 value;
  const unsigned char *ptr = reinterpret_cast<const unsigned char *>( mBuffer->c_buf() ) + mOffset;
  int nBytes = getVarint32( ptr, value );
  mOffset += nBytes;
  return value;
}

std::string ChangesetReader::readNullTerminatedString()
{
  const char *ptr = mBuffer->c_buf() + mOffset;
  int count = 0;
  while ( mOffset + count < mBuffer->size() && ptr[count] )
    ++count;

  if ( mOffset + count >= mBuffer->size() )
    throwReaderError( "readNullTerminatedString: at the end of buffer" );

  mOffset += count + 1;
  return std::string( ptr, count );
}

void ChangesetReader::readRowValues( std::vector<Value> &values )
{
  // let's ensure we have the right size of array
  if ( values.size() != mCurrentTable.columnCount() )
  {
    values.resize( mCurrentTable.columnCount() );
  }

  for ( size_t i = 0; i < mCurrentTable.columnCount(); ++i )
  {
    int type = readByte();
    if ( type == Value::TypeInt ) // 0x01
    {
      // 64-bit int (big endian)
      int64_t v;
      uint64_t x;
      memcpy( &x, mBuffer->c_buf() + mOffset, 8 );
      mOffset += 8;
      x = be64toh( x ); // convert big endian to host
      memcpy( &v, &x, 8 );
      values[i].setInt( v );
    }
    else if ( type == Value::TypeDouble ) // 0x02
    {
      // 64-bit double (big endian)
      double v;
      uint64_t x;
      memcpy( &x, mBuffer->c_buf() + mOffset, 8 );
      mOffset += 8;
      x = be64toh( x ); // convert big endian to host
      memcpy( &v, &x, 8 );
      values[i].setDouble( v );
    }
    else if ( type == Value::TypeText || type == Value::TypeBlob ) // 0x03 or 0x04
    {
      int len = readVarint();
      if ( mOffset + len > mBuffer->size() )
        throwReaderError( "readRowValues: text/blob: at the end of buffer" );
      values[i].setString( type == Value::TypeText ? Value::TypeText : Value::TypeBlob, mBuffer->c_buf() + mOffset, len );
      mOffset += len;
    }
    else if ( type == Value::TypeNull ) // 0x05
    {
      values[i].setNull();
    }
    else if ( type == Value::TypeUndefined )  // undefined value  (different from NULL)
    {
      values[i].setUndefined();
    }
    else
    {
      throwReaderError( "readRowValues: unexpected entry type" );
    }
  }
}

void ChangesetReader::readTableRecord()
{
  /* A 'table' record consists of:
  **
  **   * A constant 'T' character,
  **   * Number of columns in said table (a varint),
  **   * An array of nCol bytes (sPK),
  **   * A nul-terminated table name.
  */

  int nCol = readVarint();
  if ( nCol < 0 || nCol > 65536 )
    throwReaderError( "readByte: unexpected number of columns" );

  mCurrentTable.primaryKeys.clear();

  for ( int i = 0; i < nCol; ++i )
  {
    mCurrentTable.primaryKeys.push_back( readByte() );
  }

  mCurrentTable.name = readNullTerminatedString();
}

ChangesetDataEntry ChangesetReader::readDataEntry( ChangesetEntryType type )
{
  ChangesetDataEntry entry;
  readByte();
  if ( type != ChangesetEntryType::OpInsert )
    readRowValues( entry.oldValues );
  else
    entry.oldValues.erase( entry.oldValues.begin(), entry.oldValues.end() );
  if ( type != ChangesetEntryType::OpDelete )
    readRowValues( entry.newValues );
  else
    entry.newValues.erase( entry.newValues.begin(), entry.newValues.end() );

  entry.op = static_cast<ChangesetDataEntry::OperationType>( type );
  entry.table = &mCurrentTable;
  return entry;
}

TableColumnInfo ChangesetReader::readColumnInfo()
{
  TableColumnInfo column;
  column.name = readNullTerminatedString();
  column.type.baseType = static_cast<TableColumnType::BaseType>( readByte() );
  column.type.dbType = column.type.baseTypeToString( column.type.baseType );
  char flags = readByte();
  column.isPrimaryKey = flags & 1;
  column.isNotNull = flags & ( 1 << 1 );
  column.isAutoIncrement = flags & ( 1 << 2 );
  column.isGeometry = flags & ( 1 << 3 );
  column.geomHasZ = flags & ( 1 << 4 );
  column.geomHasM = flags & ( 1 << 5 );
  column.geomType = readNullTerminatedString();
  column.geomSrsId = readVarint();
  return column;
}

ChangesetCreateTableEntry ChangesetReader::readCreateTableEntry()
{
  ChangesetCreateTableEntry entry;
  entry.tableName = readNullTerminatedString();
  int columnCount = readVarint();
  entry.columns.resize( columnCount );
  for ( size_t i = 0; i < entry.columns.size(); i++ )
  {
    entry.columns[i] = readColumnInfo();
  }
  return entry;
}

ChangesetDropTableEntry ChangesetReader::readDropTableEntry()
{
  ChangesetDropTableEntry entry;
  entry.tableName = readNullTerminatedString();
  return entry;
}

ChangesetAddColumnEntry ChangesetReader::readAddColumnEntry()
{
  ChangesetAddColumnEntry entry;
  entry.tableName = readNullTerminatedString();
  entry.column = readColumnInfo();
  return entry;
}

ChangesetDropColumnEntry ChangesetReader::readDropColumnEntry()
{
  ChangesetDropColumnEntry entry;
  entry.tableName = readNullTerminatedString();
  entry.columnName = readNullTerminatedString();
  return entry;
}

void ChangesetReader::throwReaderError( const std::string &message ) const
{
  std::ostringstream stringStream;
  stringStream << "Reader error at offset " << mOffset << ":\n" << message;
  std::string str = stringStream.str();
  throw GeoDiffException( str );
}
