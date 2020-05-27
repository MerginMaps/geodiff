/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "geodiffchangeset.h"

#include "varint_helpers.h"

#include <sqlite3.h>
#include <assert.h>
#include <endian.h>
#include <memory.h>

#include <sstream>

// The changeset format is borrowed from the format used by sqlite3 session extension.
// Here's the description (verbatim from sqlite3 sources):

/*
** CHANGESET FORMAT:
**
** A changeset is a collection of DELETE, UPDATE and INSERT operations on
** one or more tables. Operations on a single table are grouped together,
** but may occur in any order (i.e. deletes, updates and inserts are all
** mixed together).
**
** Each group of changes begins with a table header:
**
**   1 byte: Constant 0x54 (capital 'T')
**   Varint: Number of columns in the table.
**   nCol bytes: 0x01 for PK columns, 0x00 otherwise.
**   N bytes: Unqualified table name (encoded using UTF-8). Nul-terminated.
**
** Followed by one or more changes to the table.
**
**   1 byte: Either SQLITE_INSERT (0x12), UPDATE (0x17) or DELETE (0x09).
**   1 byte: The "indirect-change" flag.
**   old.* record: (delete and update only)
**   new.* record: (insert and update only)
**
** The "old.*" and "new.*" records, if present, are N field records in the
** format described above under "RECORD FORMAT", where N is the number of
** columns in the table. The i'th field of each record is associated with
** the i'th column of the table, counting from left to right in the order
** in which columns were declared in the CREATE TABLE statement.
**
** The new.* record that is part of each INSERT change contains the values
** that make up the new row. Similarly, the old.* record that is part of each
** DELETE change contains the values that made up the row that was deleted
** from the database. In the changeset format, the records that are part
** of INSERT or DELETE changes never contain any undefined (type byte 0x00)
** fields.
**
** Within the old.* record associated with an UPDATE change, all fields
** associated with table columns that are not PRIMARY KEY columns and are
** not modified by the UPDATE change are set to "undefined". Other fields
** are set to the values that made up the row before the UPDATE that the
** change records took place. Within the new.* record, fields associated
** with table columns modified by the UPDATE change contain the new
** values. Fields associated with table columns that are not modified
** are set to "undefined".

** Unlike the SQLite database record format, each field is self-contained -
** there is no separation of header and data. Each field begins with a
** single byte describing its type, as follows:
**
**       0x00: Undefined value.
**       0x01: Integer value.
**       0x02: Real value.
**       0x03: Text value.
**       0x04: Blob value.
**       0x05: SQL NULL value.
**
** Note that the above match the definitions of SQLITE_INTEGER, SQLITE_TEXT
** and so on in sqlite3.h. For undefined and NULL values, the field consists
** only of the single type byte. For other types of values, the type byte
** is followed by:
**
**   Text values:
**     A varint containing the number of bytes in the value (encoded using
**     UTF-8). Followed by a buffer containing the UTF-8 representation
**     of the text value. There is no nul terminator.
**
**   Blob values:
**     A varint containing the number of bytes in the value, followed by
**     a buffer containing the value itself.
**
**   Integer values:
**     An 8-byte big-endian integer value.
**
**   Real values:
**     An 8-byte big-endian IEEE 754-2008 real value.
**
** Varint values are encoded in the same way as varints in the SQLite
** record format.
*/


GeoDiffChangesetReader::GeoDiffChangesetReader()
{

}

GeoDiffChangesetReader::~GeoDiffChangesetReader()
{

}

bool GeoDiffChangesetReader::open( const std::string &filename )
{
  try
  {
    buffer.read( filename );
  }
  catch ( GeoDiffException )
  {
    return false;
  }

  return true;
}

bool GeoDiffChangesetReader::nextEntry( ChangesetEntry &entry )
{
  while ( 1 )
  {
    if ( offset >= buffer.size() )
      break;   // EOF

    int type = readByte();
    if ( type == 'T' )
    {
      readTableRecord();
      // and now continue reading, we want an entry
    }
    else if ( type == SQLITE_INSERT || type == SQLITE_UPDATE || type == SQLITE_DELETE )
    {
      int indirect = readByte();
      if ( type != SQLITE_INSERT )
        readRowValues( entry.oldValues );
      if ( type != SQLITE_DELETE )
        readRowValues( entry.newValues );

      entry.op = static_cast<ChangesetEntry::OperationType>( type );
      entry.table = &currentTable;
      return true;  // we're done!
    }
  }
  return false;
}

char GeoDiffChangesetReader::readByte()
{
  if ( offset >= buffer.size() )
    throwReaderError( "readByte: at the end of buffer" );
  const char *ptr = buffer.c_buf() + offset;
  ++offset;
  return *ptr;
}

int GeoDiffChangesetReader::readVarint()
{
  u32 value;
  const unsigned char *ptr = ( const unsigned char * )buffer.c_buf() + offset;
  //int nBytes = sqlite3GetVarint32(ptr, &value);
  int nBytes = getVarint32( ptr, value );
  offset += nBytes;
  return value;
}

std::string GeoDiffChangesetReader::readNullTerminatedString()
{
  const char *ptr = buffer.c_buf() + offset;
  int count = 0;
  while ( offset + count < buffer.size() && ptr[count] )
    ++count;

  if ( offset + count >= buffer.size() )
    throwReaderError( "readNullTerminatedString: at the end of buffer" );

  offset += count + 1;
  return std::string( ptr, count );
}

void GeoDiffChangesetReader::readRowValues( std::vector<Value> &values )
{
  // let's ensure we have the right size of array
  if ( values.size() != currentTable.primaryKeys.size() )
  {
    values.resize( currentTable.primaryKeys.size() );
  }

  for ( size_t i = 0; i < currentTable.primaryKeys.size(); ++i )
  {
    int type = readByte();
    if ( type == SQLITE_INTEGER ) // 0x01
    {
      // 64-bit int (big endian)
      int64_t v;
      uint64_t x;
      memcpy( &x, buffer.c_buf() + offset, 8 );
      offset += 8;
      x = be64toh( x ); // convert big endian to host
      memcpy( &v, &x, 8 );
      values[i].setInt( v );
    }
    else if ( type == SQLITE_FLOAT ) // 0x02
    {
      // 64-bit double (big endian)
      double v;
      uint64_t x;
      memcpy( &x, buffer.c_buf() + offset, 8 );
      offset += 8;
      x = be64toh( x ); // convert big endian to host
      memcpy( &v, &x, 8 );
      values[i].setDouble( v );
    }
    else if ( type == SQLITE_TEXT || type == SQLITE_BLOB ) // 0x03 or 0x04
    {
      int len = readVarint();
      if ( offset + len >= buffer.size() )
        throwReaderError( "readRowValues: text/blob: at the end of buffer" );
      values[i].setString( type == SQLITE_TEXT ? Value::TypeText : Value::TypeBlob, buffer.c_buf() + offset, len );
      offset += len;
    }
    else if ( type == SQLITE_NULL ) // 0x05
    {
      values[i].setNull();
    }
    else if ( type == 0 )  // undefined value  (different from NULL)
    {
      values[i].setUndefined();
    }
    else
    {
      throwReaderError( "readRowValues: unexpected entry type" );
    }
  }
}

void GeoDiffChangesetReader::readTableRecord()
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

  currentTable.primaryKeys.clear();

  for ( int i = 0; i < nCol; ++i )
  {
    currentTable.primaryKeys.push_back( readByte() );
  }

  currentTable.name = readNullTerminatedString();
}


void GeoDiffChangesetReader::throwReaderError( const std::string &message )
{
  std::ostringstream stringStream;
  stringStream << "Reader error at offset " << offset << ":\n" << message;
  std::string str = stringStream.str();
  throw GeoDiffException( str );
}


//


bool GeoDiffChangesetWriter::open( const std::string &filename )
{
  myfile.open( filename, std::ios::out | std::ios::binary );
  if ( !myfile.is_open() )
    return false;

  return true;
}

void GeoDiffChangesetWriter::beginTable( const ChangesetTable &table )
{
  currentTable = table;

  writeByte( 'T' );
  writeVarint( table.primaryKeys.size() );
  for ( size_t i = 0; i < table.primaryKeys.size(); ++i )
    writeByte( table.primaryKeys[i] );
  writeNullTerminatedString( table.name );
}

void GeoDiffChangesetWriter::writeEntry( const ChangesetEntry &entry )
{
  if ( entry.op != SQLITE_INSERT && entry.op != SQLITE_UPDATE && entry.op != SQLITE_DELETE )
    throw GeoDiffException( "wrong op for changeset entry" );
  writeByte( entry.op );
  writeByte( 0 );  // "indirect" always false

  if ( entry.op != SQLITE_INSERT )
    writeRowValues( entry.oldValues );
  if ( entry.op != SQLITE_DELETE )
    writeRowValues( entry.newValues );
}

void GeoDiffChangesetWriter::writeByte( char c )
{
  myfile.write( &c, 1 );
}

void GeoDiffChangesetWriter::writeVarint( int n )
{
  unsigned char output[9];  // 1-9 bytes
  int numBytes = putVarint32( output, n );
  myfile.write( ( char * )output, numBytes );
}

void GeoDiffChangesetWriter::writeNullTerminatedString( const std::string &str )
{
  myfile.write( str.c_str(), str.size() + 1 );
}

void GeoDiffChangesetWriter::writeRowValues( const std::vector<Value> &values )
{
  if ( values.size() != currentTable.primaryKeys.size() )
    throw GeoDiffException( "wrong number of rows in the entry" );

  for ( size_t i = 0; i < currentTable.primaryKeys.size(); ++i )
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
      myfile.write( ( char * )&x, 8 );
    }
    else if ( type == Value::TypeDouble ) // 0x02
    {
      // 64-bit double (big endian)
      int64_t x;
      double v = values[i].getDouble();
      memcpy( &x, &v, 8 );
      x = htobe64( x ); // convert host to big endian
      myfile.write( ( char * )&x, 8 );
    }
    else if ( type == Value::TypeText || type == Value::TypeBlob ) // 0x03 or 0x04
    {
      const std::string &str = values[i].getString();
      writeVarint( str.size() );
      myfile.write( str.c_str(), str.size() );
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
