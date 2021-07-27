/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffutils.hpp"
#include "changeset.h"
#include "geodifflogger.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <sqlite3.h>
#include <exception>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <gpkg.h>
#include <locale>
#include <codecvt>
#include <limits>

#ifdef _WIN32
#define UNICODE
#include <windows.h>
#include <tchar.h>
#include <Shlwapi.h>
#else
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#endif

GeoDiffException::GeoDiffException( const std::string &msg )
  : std::exception()
  , mMsg( msg )
{
}

const char *GeoDiffException::what() const throw()
{
  return mMsg.c_str();
}

// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////

// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////

Buffer::Buffer() = default;

Buffer::~Buffer()
{
  free();
}

bool Buffer::isEmpty() const
{
  return mAlloc == 0;
}

void Buffer::free()
{
  if ( mZ )
  {
    sqlite3_free( mZ );
    mZ = nullptr;
    mAlloc = 0;
    mUsed = 0;
  }
}

void Buffer::read( const std::string &filename )
{
  // https://stackoverflow.com/questions/3747086/reading-the-whole-text-file-into-a-char-array-in-c

  // clean the buffer
  free();

  /* Open the file */
  FILE *fp = openFile( filename, "rb" );
  if ( nullptr == fp )
  {
    throw GeoDiffException( "Unable to open " + filename );
  }

  /* Seek to the end of the file */
  int rc = fseek( fp, 0L, SEEK_END );
  if ( 0 != rc )
  {
    fclose( fp );
    throw GeoDiffException( "Unable to seek the end of " + filename );
  }

  long off_end;
  /* Byte offset to the end of the file (size) */
  if ( 0 > ( off_end = ftell( fp ) ) )
  {
    fclose( fp );
    throw GeoDiffException( "Unable to read file size of " + filename );
  }
  mAlloc = ( size_t )off_end;
  mUsed = mAlloc;

  if ( mAlloc == 0 )
  {
    // empty file
    fclose( fp );
    return;
  }

  /* Allocate a buffer to hold the whole file */
  mZ = ( char * ) sqlite3_malloc( mAlloc );
  if ( mZ == nullptr )
  {
    fclose( fp );
    throw GeoDiffException( "Out of memory to read " + filename + " to internal buffer" );
  }

  /* Rewind file pointer to start of file */
  rewind( fp );

  /* Slurp file into buffer */
  if ( mAlloc != fread( mZ, 1, mAlloc, fp ) )
  {
    fclose( fp );
    throw GeoDiffException( "Unable to read " + filename + " to internal buffer" );
  }

  /* Close the file */
  if ( EOF == fclose( fp ) )
  {
    throw GeoDiffException( "Unable to close " + filename );
  }
}

void Buffer::read( int size, void *stream )
{
  mAlloc = size;
  mUsed = size;
  mZ = ( char * ) stream;
}

void Buffer::printf( const char *zFormat, ... )
{
  int nNew;
  for ( ;; )
  {
    if ( mZ )
    {
      va_list ap;
      va_start( ap, zFormat );
      sqlite3_vsnprintf( mAlloc - mUsed, mZ + mUsed, zFormat, ap );
      va_end( ap );
      nNew = ( int )strlen( mZ + mUsed );
    }
    else
    {
      nNew = mAlloc;
    }
    if ( mUsed + nNew < mAlloc - 1 )
    {
      mUsed += nNew;
      break;
    }
    mAlloc = mAlloc * 2 + 1000;
    mZ = ( char * ) sqlite3_realloc( mZ, mAlloc );
    if ( mZ == nullptr )
    {
      throw GeoDiffException( "out of memory in Buffer::printf" );
    }
  }
}

void Buffer::write( const std::string &filename )
{
  FILE *f = openFile( filename, "wb" );
  if ( !f )
  {
    throw GeoDiffException( "Unable to open " + filename + " for writing" );
  }
  fwrite( mZ, mAlloc, 1, f );
  fclose( f );
}

const char *Buffer::c_buf() const
{
  return mZ;
}

void *Buffer::v_buf() const
{
  return mZ;
}

int Buffer::size() const
{
  return mAlloc;
}

// ////////////////////////////////////////////////////////////////////////


std::string to_string_with_max_precision( double a_value )
{
  std::ostringstream out;
  // The limits digits10 ( = 15 for double) and max_digits10 ( = 17 for double) are a bit confusing.
  // originally had numeric_limits::digits10 + 1 here, but that was not enough for some numbers.
  // Also, we used std::fixed to avoid scientific notation, but that's loosing precision for small
  // numbers like 0.000123... where we waste a couple of digits for leading zeros.
  out.precision( std::numeric_limits<double>::max_digits10 );
  out << a_value;
  return out.str();
}

//

FILE *openFile( const std::string &path, const std::string &mode )
{
#ifdef WIN32
  // convert string path to wstring
  return _wfopen( stringToWString( path ).c_str(), stringToWString( mode ).c_str() );
#else
  return fopen( path.c_str(), mode.c_str() );
#endif
}

void filecopy( const std::string &to, const std::string &from )
{
  fileremove( to );

#ifdef WIN32
  std::wstring wFrom = stringToWString( from );
  std::wstring wTo = stringToWString( to );
  CopyFile( wFrom.c_str(), wTo.c_str(), false );
#else

  std::ifstream  src( from, std::ios::binary );
  std::ofstream  dst( to,   std::ios::binary );

  dst << src.rdbuf();
#endif
}

bool fileremove( const std::string &path )
{
  if ( fileexists( path ) )
  {
#ifdef WIN32
    int res = _wremove( stringToWString( path ).c_str() );
#else
    int res = remove( path.c_str() );
#endif
    return res == 0;
  }
  return true;  // nothing to delete, no problem...
}

bool fileexists( const std::string &path )
{
#ifdef WIN32
  std::wstring wPath = stringToWString( path );

  if ( wPath.empty() )
    return false;

  return PathFileExists( wPath.c_str() );
#else
  // https://stackoverflow.com/a/12774387/2838364
  struct stat buffer;
  return ( stat( path.c_str(), &buffer ) == 0 );
#endif
}

bool startsWith( const std::string &str, const std::string &substr )
{
  if ( str.size() < substr.size() )
    return false;

  return str.rfind( substr, 0 ) == 0;
}

std::string replace( const std::string &str, const std::string &substr, const std::string &replacestr )
{
  std::string res( str );

  size_t i = 0;
  while ( res.find( substr,  i ) != std::string::npos )
  {
    i = res.find( substr, i );
    res.replace( i, substr.size(), replacestr );
    i = i + replacestr.size();
  }
  return res;
}


bool isLayerTable( const std::string &tableName )
{
  /* typically geopackage from ogr would have these (table name is simple)
  gpkg_contents
  gpkg_extensions
  gpkg_geometry_columns
  gpkg_ogr_contents
  gpkg_spatial_ref_sys
  gpkg_tile_matrix
  gpkg_tile_matrix_set
  rtree_simple_geometry_node
  rtree_simple_geometry_parent
  rtree_simple_geometry_rowid
  simple (or any other name(s) of layers)
  sqlite_sequence
  */

  // table handled by triggers trigger_*_feature_count_*
  if ( startsWith( tableName, "gpkg_" ) )
    return false;
  // table handled by triggers rtree_*_geometry_*
  if ( startsWith( tableName, "rtree_" ) )
    return false;
  // internal table for AUTOINCREMENT
  if ( tableName == "sqlite_sequence" )
    return false;

  return true;
}


////

void get_primary_key( const ChangesetEntry &entry, int &fid, int &nColumn )
{
  const std::vector<bool> &tablePkeys = entry.table->primaryKeys;

  // lets assume for now it has only one PK and it is int...
  int pk_column_number = -1;
  for ( size_t i = 0; i < tablePkeys.size(); ++i )
  {
    if ( tablePkeys[i] )
    {
      if ( pk_column_number >= 0 )
      {
        // ups primary key composite!
        throw GeoDiffException( "internal error in _get_primary_key: support composite primary keys not implemented" );
      }
      pk_column_number = i;
    }
  }
  if ( pk_column_number == -1 )
  {
    throw GeoDiffException( "internal error in _get_primary_key: unable to find internal key" );
  }

  nColumn = pk_column_number;

  // now get the value
  Value pkeyValue;
  if ( entry.op == ChangesetEntry::OpInsert )
  {
    pkeyValue = entry.newValues[pk_column_number];
  }
  else if ( entry.op == ChangesetEntry::OpDelete || entry.op == ChangesetEntry::OpUpdate )
  {
    pkeyValue = entry.oldValues[pk_column_number];
  }
  if ( pkeyValue.type() == Value::TypeUndefined || pkeyValue.type() == Value::TypeNull )
    throw GeoDiffException( "internal error in _get_primary_key: unable to get value of primary key" );

  if ( pkeyValue.type() == Value::TypeInt )
  {
    int val = pkeyValue.getInt();
    fid = val;
    return;
  }
  else if ( pkeyValue.type() == Value::TypeText )
  {
    std::string str = pkeyValue.getString();
    const char *strData = str.data();
    int hash = 0;
    int len = str.size();
    for ( int i = 0; i < len; i++ )
    {
      hash = 33 * hash + ( unsigned char )strData[i];
    }
    fid = hash;
  }
  else
  {
    throw GeoDiffException( "internal error in _get_primary_key: unsuported type of primary key" );
  }
}


void flushString( const std::string &filename, const std::string &str )
{
#ifdef WIN32
  std::wstring wFilename = stringToWString( filename );
  std::ofstream out( wFilename );
#else
  std::ofstream out( filename );
#endif
  out << str;
  out.close();
}

std::string getEnvVar( std::string const &key, const std::string &defaultVal )
{
  char *val = getenv( key.c_str() );
  return val == nullptr ? defaultVal : std::string( val );
}

std::string tmpdir()
{
#ifdef WIN32
  wchar_t arr[MAX_PATH];
  DWORD dwRetVal = GetTempPathW( MAX_PATH, arr );

  std::wstring tempDirPath( arr );
  if ( dwRetVal > MAX_PATH || ( dwRetVal == 0 ) )
  {
    return std::string( "C:/temp/" );
  }

  return wstringToString( tempDirPath );
#else
  return getEnvVar( "TMPDIR", "/tmp/" );
#endif
}

std::wstring stringToWString( const std::string &str )
{
  // we need to convert UTF-8 string to UTF-16 in order to use WindowsAPI
  // https://stackoverflow.com/questions/2573834/c-convert-string-or-char-to-wstring-or-wchar-t
  try
  {
    std::wstring_convert< std::codecvt_utf8_utf16< wchar_t > > converter;
    std::wstring wStr = converter.from_bytes( str );

    return wStr;
  }
  catch ( const std::range_error & )
  {
    Logger::instance().error( "Unable to convert UTF-8 to UTF-16." );
    return std::wstring();
  }
}

std::string wstringToString( const std::wstring &wStr )
{
  // we need to convert UTF-16 string to UTF-8 in order to use WindowsAPI
  // https://stackoverflow.com/questions/4804298/how-to-convert-wstring-into-string
  try
  {
    std::wstring_convert< std::codecvt_utf8_utf16< wchar_t > > converter;
    std::string str = converter.to_bytes( wStr );

    return str;
  }
  catch ( const std::range_error & )
  {
    Logger::instance().error( "Unable to convert UTF-16 to UTF-8." );
    return std::string();
  }
}

TmpFile::TmpFile() = default;

TmpFile::TmpFile( const std::string &path ):
  mPath( path )
{
}

TmpFile::~TmpFile()
{
  if ( fileexists( mPath ) )
  {
    fileremove( mPath );
  }
}

std::string TmpFile::path() const
{
  return mPath;
}

const char *TmpFile::c_path() const
{
  return mPath.c_str();
}

void TmpFile::setPath( const std::string &path )
{
  if ( mPath != path )
    mPath = path;
}

ConflictFeature::ConflictFeature( int pk,
                                  const std::string &tableName )
  : mPk( pk )
  , mTableName( tableName )
{
}

bool ConflictFeature::isValid() const
{
  return !mItems.empty();
}

void ConflictFeature::addItem( const ConflictItem &item )
{
  mItems.push_back( item );
}

std::string ConflictFeature::tableName() const
{
  return mTableName;
}

int ConflictFeature::pk() const
{
  return mPk;
}

std::vector<ConflictItem> ConflictFeature::items() const
{
  return mItems;
}

ConflictItem::ConflictItem( int column, const Value &base,
                            const Value &theirs, const Value &ours )
  : mColumn( column )
  , mBase( base )
  , mTheirs( theirs )
  , mOurs( ours )
{

}

Value ConflictItem::base() const
{
  return mBase;
}

Value ConflictItem::theirs() const
{
  return mTheirs;
}

Value ConflictItem::ours() const
{
  return mOurs;
}

int ConflictItem::column() const
{
  return mColumn;
}

int indexOf( const std::vector<std::string> &arr, const std::string &val )
{
  std::vector<std::string>::const_iterator result = std::find( arr.begin(), arr.end(), val );
  if ( result == arr.end() )
    return -1;
  else
    return std::distance( arr.begin(), result );
}

std::string concatNames( const std::vector<std::string> &names )
{
  std::string output;
  for ( const std::string &name : names )
  {
    if ( !output.empty() )
      output += ", ";
    output += name;
  }
  return output;
}

std::string lowercaseString( const std::string &str )
{
  std::string ret = str;
  std::transform( ret.begin(), ret.end(), ret.begin(),
  []( unsigned char c ) { return std::tolower( c ); } );
  return ret;
}


// from https://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c
std::string randomString( size_t length )
{
  auto randchar = []() -> char
  {
    const char charset[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = ( sizeof( charset ) - 1 );
    return charset[ rand() % max_index ];
  };
  std::string str( length, 0 );
  std::generate_n( str.begin(), length, randchar );
  return str;
}

std::string randomTmpFilename()
{
  return tmpdir() + "geodiff_" + randomString( 6 );
}
