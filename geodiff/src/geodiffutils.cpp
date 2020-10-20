/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffutils.hpp"
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
#include <gpkg.h>
#include <locale>
#include <codecvt>

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
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
Sqlite3Db::Sqlite3Db() = default;
Sqlite3Db::~Sqlite3Db()
{
  close();
}

void Sqlite3Db::open( const std::string &filename )
{
  close();
  int rc = sqlite3_open_v2( filename.c_str(), &mDb, SQLITE_OPEN_READWRITE, nullptr );
  if ( rc )
  {
    throw GeoDiffException( "Unable to open " + filename + " as sqlite3 database" );
  }
}

void Sqlite3Db::exec( const Buffer &buf )
{
  int rc = sqlite3_exec( get(), buf.c_buf(), NULL, 0, NULL );
  if ( rc )
  {
    throw GeoDiffException( "Unable to exec buffer on sqlite3 database" );
  }
}

sqlite3 *Sqlite3Db::get()
{
  return mDb;
}

void Sqlite3Db::close()
{
  if ( mDb )
  {
    sqlite3_close( mDb );
    mDb = nullptr;
  }
}


Sqlite3Session::Sqlite3Session() = default;

Sqlite3Session::~Sqlite3Session()
{
  close();
}

void Sqlite3Session::create( std::shared_ptr<Sqlite3Db> db, const std::string &name )
{
  close();
  if ( db && db->get() )
  {
    int rc = sqlite3session_create( db->get(), name.c_str(), &mSession );
    if ( rc )
    {
      throw GeoDiffException( "Unable to open session " + name );
    }
  }
}

sqlite3_session *Sqlite3Session::get() const
{
  return mSession;
}

void Sqlite3Session::close()
{
  if ( mSession )
  {
    sqlite3session_delete( mSession );
    mSession = nullptr;
  }
}

Sqlite3Stmt::Sqlite3Stmt() = default;

Sqlite3Stmt::~Sqlite3Stmt()
{
  close();
}

sqlite3_stmt *Sqlite3Stmt::db_vprepare( sqlite3 *db, const char *zFormat, va_list ap )
{
  char *zSql;
  int rc;
  sqlite3_stmt *pStmt;

  zSql = sqlite3_vmprintf( zFormat, ap );
  if ( zSql == nullptr )
  {
    throw GeoDiffException( "out of memory" );
  }

  rc = sqlite3_prepare_v2( db, zSql, -1, &pStmt, nullptr );
  if ( rc )
  {
    throw GeoDiffException( "SQL statement error" );
  }
  sqlite3_free( zSql );
  return pStmt;
}

void Sqlite3Stmt::prepare( std::shared_ptr<Sqlite3Db> db, const char *zFormat, ... )
{
  if ( db && db->get() )
  {
    va_list ap;
    va_start( ap, zFormat );
    mStmt = db_vprepare( db->get(), zFormat, ap );
    va_end( ap );
  }
}

sqlite3_stmt *Sqlite3Stmt::get()
{
  return mStmt;
}

void Sqlite3Stmt::close()
{
  if ( mStmt )
  {
    sqlite3_finalize( mStmt );
    mStmt = nullptr;
  }
}

Sqlite3ChangesetIter::Sqlite3ChangesetIter() = default;

Sqlite3ChangesetIter::~Sqlite3ChangesetIter()
{
  close();
}

void Sqlite3ChangesetIter::start( const Buffer &buf )
{
  int rc = sqlite3changeset_start(
             &mChangesetIter,
             buf.size(),
             buf.v_buf()
           );
  if ( rc != SQLITE_OK )
  {
    throw GeoDiffException( "sqlite3changeset_start error" );
  }
}

sqlite3_changeset_iter *Sqlite3ChangesetIter::get()
{
  return mChangesetIter;
}

void Sqlite3ChangesetIter::close()
{
  if ( mChangesetIter )
  {
    sqlite3changeset_finalize( mChangesetIter );

    mChangesetIter = nullptr;
  }
}

void Sqlite3ChangesetIter::oldValue( int i, sqlite3_value **val )
{
  int rc = sqlite3changeset_old( mChangesetIter, i, val );
  if ( rc != SQLITE_OK )
  {
    throw GeoDiffException( "sqlite3changeset_old error" );
  }
}

void Sqlite3ChangesetIter::newValue( int i, sqlite3_value **val )
{
  int rc = sqlite3changeset_new( mChangesetIter, i, val );
  if ( rc != SQLITE_OK )
  {
    throw GeoDiffException( "sqlite3changeset_new error" );
  }
}

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

void Buffer::read( const Sqlite3Session &session )
{
  free();
  if ( !session.get() )
  {
    throw GeoDiffException( "Invalid session" );
  }
  int rc = sqlite3session_changeset( session.get(), &mAlloc, ( void ** ) &mZ );
  mUsed = mAlloc;
  if ( rc )
  {
    throw GeoDiffException( "Unable to read sqlite3 session to internal buffer" );
  }
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
// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////



Sqlite3Value::Sqlite3Value() = default;

Sqlite3Value::Sqlite3Value( const sqlite3_value *val )
{
  if ( val )
  {
    mVal = sqlite3_value_dup( val );
  }
}

Sqlite3Value::~Sqlite3Value()
{
  if ( mVal )
  {
    sqlite3_value_free( mVal );
  }
}

bool Sqlite3Value::isValid() const
{
  return mVal != nullptr;
}

sqlite3_value *Sqlite3Value::value() const
{
  return mVal;
}

std::string Sqlite3Value::toString( sqlite3_value *ppValue )
{
  if ( !ppValue )
    return "nil";
  std::string val = "n/a";
  int type = sqlite3_value_type( ppValue );
  if ( type == SQLITE_INTEGER )
    val = std::to_string( sqlite3_value_int( ppValue ) );
  else if ( type == SQLITE_TEXT )
    val = std::string( reinterpret_cast<const char *>( sqlite3_value_text( ppValue ) ) );
  else if ( type == SQLITE_FLOAT )
    val = std::to_string( sqlite3_value_double( ppValue ) );
  else if ( type == SQLITE_BLOB )
    val = "blob " + std::to_string( sqlite3_value_bytes( ppValue ) ) + " bytes";
  return val;
}

// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////////////////

std::string pOpToStr( int pOp )
{
  switch ( pOp )
  {
    case SQLITE_CREATE_INDEX: return "SQLITE_CREATE_INDEX";
    case SQLITE_CREATE_TABLE: return "SQLITE_CREATE_TABLE";
    case SQLITE_CREATE_TEMP_INDEX: return "SQLITE_CREATE_TEMP_INDEX";
    case SQLITE_CREATE_TEMP_TABLE: return "SQLITE_CREATE_TEMP_TABLE";
    case SQLITE_CREATE_TEMP_TRIGGER: return "SQLITE_CREATE_TEMP_TRIGGER";
    case SQLITE_CREATE_TEMP_VIEW: return "SQLITE_CREATE_TEMP_VIEW";
    case SQLITE_CREATE_TRIGGER: return "SQLITE_CREATE_TRIGGER";
    case SQLITE_CREATE_VIEW: return "SQLITE_CREATE_VIEW";
    case SQLITE_DELETE: return "SQLITE_DELETE";
    case SQLITE_DROP_INDEX: return "SQLITE_DROP_INDEX";
    case SQLITE_DROP_TABLE: return "SQLITE_DROP_TABLE";
    case SQLITE_DROP_TEMP_INDEX : return "SQLITE_DROP_TEMP_INDEX";
    case SQLITE_DROP_TEMP_TABLE: return "SQLITE_DROP_TEMP_TABLE";
    case SQLITE_DROP_TEMP_TRIGGER: return "SQLITE_DROP_TEMP_TRIGGER";
    case SQLITE_DROP_TEMP_VIEW: return "SQLITE_DROP_TEMP_VIEW";
    case SQLITE_DROP_TRIGGER: return "SQLITE_DROP_TRIGGER";
    case SQLITE_DROP_VIEW: return "SQLITE_DROP_VIEW";
    case SQLITE_INSERT: return "SQLITE_INSERT";
    case SQLITE_PRAGMA: return "SQLITE_PRAGMA";
    case SQLITE_READ: return "SQLITE_READ";
    case SQLITE_SELECT: return "SQLITE_SELECT";
    case SQLITE_TRANSACTION: return "SQLITE_TRANSACTION";
    case SQLITE_UPDATE: return "SQLITE_UPDATE";
    case SQLITE_ATTACH: return "SQLITE_ATTACH";
    case SQLITE_DETACH: return "SQLITE_DETACH";
    case SQLITE_ALTER_TABLE: return "SQLITE_ALTER_TABLE";
    case SQLITE_REINDEX: return "SQLITE_REINDEX";
    case SQLITE_ANALYZE: return "SQLITE_ANALYZE";
    case SQLITE_CREATE_VTABLE: return "SQLITE_CREATE_VTABLE";
    case SQLITE_DROP_VTABLE: return "SQLITE_DROP_VTABLE";
    case SQLITE_FUNCTION: return "SQLITE_FUNCTION";
    case SQLITE_SAVEPOINT: return "SQLITE_SAVEPOINT";
    case SQLITE_COPY: return "SQLITE_COPY";
    case SQLITE_RECURSIVE: return "SQLITE_RECURSIVE";
  }
  return std::to_string( pOp );
}

std::string conflict2Str( int c )
{
  switch ( c )
  {
    case SQLITE_CHANGESET_DATA: return "SQLITE_CHANGESET_DATA";
    case SQLITE_CHANGESET_NOTFOUND: return "SQLITE_CHANGESET_NOTFOUND";
    case SQLITE_CHANGESET_CONFLICT: return "SQLITE_CHANGESET_CONFLICT";
    case SQLITE_CHANGESET_CONSTRAINT: return "SQLITE_CHANGESET_CONSTRAINT";
    case SQLITE_CHANGESET_FOREIGN_KEY: return "SQLITE_CHANGESET_FOREIGN_KEY";
  }
  return std::to_string( c );
}

BinaryStream::BinaryStream( const std::string &path, bool temporary )
  :
  mPath( path ),
  mIsTemporary( temporary ),
  mBuffer( nullptr )
{
}

BinaryStream::~BinaryStream()
{
  close();
  if ( mIsTemporary )
  {
    remove();
  }
}

void BinaryStream::open()
{
  close();
  remove();
  mBuffer = openFile( mPath, "wb" );
}

bool BinaryStream::isValid()
{
  return mBuffer != nullptr;
}

bool BinaryStream::appendTo( FILE *stream )
{
  close();
  FILE *buf = openFile( mPath, "rb" );
  if ( !buf )
    return true;

  char buffer[1]; //do not be lazy, use bigger buffer
  while ( fread( buffer, 1, 1, buf ) > 0 )
    fwrite( buffer, 1, 1, stream );

  fclose( buf );
  return false;
}

void BinaryStream::putsVarint( sqlite3_uint64 v )
{
  if ( !isValid() )
    return;

  int i, n;
  unsigned char p[12];
  if ( v & ( ( ( sqlite3_uint64 )0xff000000 ) << 32 ) )
  {
    p[8] = ( unsigned char )v;
    v >>= 8;
    for ( i = 7; i >= 0; i-- )
    {
      p[i] = ( unsigned char )( ( v & 0x7f ) | 0x80 );
      v >>= 7;
    }
    write( p, 8, 1 );
  }
  else
  {
    n = 9;
    do
    {
      p[n--] = ( unsigned char )( ( v & 0x7f ) | 0x80 );
      v >>= 7;
    }
    while ( v != 0 );
    p[9] &= 0x7f;
    write( p + n + 1, 9 - n, 1 );
  }
}

int BinaryStream::put( int v )
{
  if ( !isValid() )
    return -1;

  return putc( v, mBuffer );
}

size_t BinaryStream::write( const void *ptr, size_t size, size_t nitems )
{
  if ( !isValid() )
    return 0;

  return fwrite( ptr, size, nitems, mBuffer );
}

void BinaryStream::putChangesetIter( Sqlite3ChangesetIter &pp, int pnCol, int pOp )
{
  put( pOp );
  put( 0 );

  if ( pOp == SQLITE_INSERT )
  {
    sqlite3_value *value;
    for ( int i = 0; i < pnCol; i++ )
    {
      pp.newValue( i, &value );
      putValue( value );
    }
  }
  else if ( pOp == SQLITE_UPDATE )
  {
    sqlite3_value *value;

    for ( int i = 0; i < pnCol; i++ )
    {
      pp.oldValue( i, &value );
      putValue( value );
    }
    for ( int i = 0; i < pnCol; i++ )
    {
      pp.newValue( i, &value );
      putValue( value );
    }
  }
  else if ( pOp == SQLITE_DELETE )
  {
    sqlite3_value *value;
    for ( int i = 0; i < pnCol; i++ )
    {
      pp.oldValue( i, &value );
      putValue( value );
    }
  }
}

void BinaryStream::close()
{
  if ( mBuffer )
  {
    fclose( mBuffer );
    mBuffer = nullptr;
  }
}

void BinaryStream::remove()
{
  if ( fileexists( mPath ) )
  {
    fileremove( mPath );
  }
}

void BinaryStream::putValue( sqlite3_value *ppValue )
{
  if ( !isValid() )
    return;

  if ( !ppValue )
  {
    put( 0 );
  }
  else
  {
    int iDType = sqlite3_value_type( ppValue );
    sqlite3_int64 iX;
    double rX;
    sqlite3_uint64 uX;
    int j;

    put( iDType );
    switch ( iDType )
    {
      case SQLITE_INTEGER:
        iX = sqlite3_value_int64( ppValue );
        memcpy( &uX, &iX, 8 );
        for ( j = 56; j >= 0; j -= 8 ) put( ( uX >> j ) & 0xff );
        break;
      case SQLITE_FLOAT:
        rX = sqlite3_value_double( ppValue );
        memcpy( &uX, &rX, 8 );
        for ( j = 56; j >= 0; j -= 8 ) put( ( uX >> j ) & 0xff );
        break;
      case SQLITE_TEXT:
        iX = sqlite3_value_bytes( ppValue );
        putsVarint( ( sqlite3_uint64 )iX );
        write( sqlite3_value_text( ppValue ), 1, ( size_t )iX );
        break;
      case SQLITE_BLOB:
        iX = sqlite3_value_bytes( ppValue );
        putsVarint( ( sqlite3_uint64 )iX );
        write( sqlite3_value_blob( ppValue ), 1, ( size_t )iX );
        break;
      case SQLITE_NULL:
        break;
    }
  }
}

void BinaryStream::putValue( int ppValue )
{
  if ( !isValid() )
    return;

  sqlite3_uint64 uX;
  sqlite3_int64 iX = ppValue;

  memcpy( &uX, &iX, 8 );
  put( SQLITE_INTEGER );
  for ( int j = 56; j >= 0; j -= 8 ) put( ( uX >> j ) & 0xff );
}


void filecopy( const std::string &to, const std::string &from )
{
  fileremove( to );

#ifdef WIN32
  std::wstring _from = stringToWString( from );
  std::wstring _to = stringToWString( to );
  CopyFile( _from.c_str(), _to.c_str(), false);
#else
  std::ifstream  src( from, std::ios::binary );
  std::ofstream  dst( to,   std::ios::binary );

  dst << src.rdbuf();
#endif
}

void fileremove( const std::string &path )
{
  if ( fileexists( path ) )
  {
#ifdef WIN32
    _wremove( stringToWString( path ).c_str() );
#else
    remove( path.c_str() );
#endif
  }
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
    Logger::instance().error( "Unable to convert UTF-8 string to UTF-16: " + str );
    return std::wstring();
  }
}

FILE* openFile( const std::string &path, const std::string &mode )
{
#ifdef WIN32
  // convert string path to wstring
  return _wfopen( stringToWString( path ).c_str(), stringToWString( mode ).c_str() );
#else
  return fopen( path.c_str(), mode.c_str() );
#endif
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

void triggers( std::shared_ptr<Sqlite3Db> db, std::vector<std::string> &triggerNames, std::vector<std::string> &triggerCmds )
{
  triggerNames.clear();
  triggerCmds.clear();

  Sqlite3Stmt statament;
  statament.prepare( db, "%s", "select name, sql from sqlite_master where type = 'trigger'" );
  while ( SQLITE_ROW == sqlite3_step( statament.get() ) )
  {
    const char *name = ( char * ) sqlite3_column_text( statament.get(), 0 );
    const char *sql = ( char * ) sqlite3_column_text( statament.get(), 1 );

    if ( !name || !sql )
      continue;

    /* typically geopackage from ogr would have these (table name is simple)
        - gpkg_tile_matrix_zoom_level_insert
        - gpkg_tile_matrix_zoom_level_update
        - gpkg_tile_matrix_matrix_width_insert
        - gpkg_tile_matrix_matrix_width_update
        - gpkg_tile_matrix_matrix_height_insert
        - gpkg_tile_matrix_matrix_height_update
        - gpkg_tile_matrix_pixel_x_size_insert
        - gpkg_tile_matrix_pixel_x_size_update
        - gpkg_tile_matrix_pixel_y_size_insert
        - gpkg_tile_matrix_pixel_y_size_update
        - gpkg_metadata_md_scope_insert
        - gpkg_metadata_md_scope_update
        - gpkg_metadata_reference_reference_scope_insert
        - gpkg_metadata_reference_reference_scope_update
        - gpkg_metadata_reference_column_name_insert
        - gpkg_metadata_reference_column_name_update
        - gpkg_metadata_reference_row_id_value_insert
        - gpkg_metadata_reference_row_id_value_update
        - gpkg_metadata_reference_timestamp_insert
        - gpkg_metadata_reference_timestamp_update
        - rtree_simple_geometry_insert
        - rtree_simple_geometry_update1
        - rtree_simple_geometry_update2
        - rtree_simple_geometry_update3
        - rtree_simple_geometry_update4
        - rtree_simple_geometry_delete
        - trigger_insert_feature_count_simple
        - trigger_delete_feature_count_simple
     */
    const std::string triggerName( name );
    if ( startsWith( triggerName, "gpkg_" ) )
      continue;
    if ( startsWith( triggerName, "rtree_" ) )
      continue;
    if ( startsWith( triggerName, "trigger_insert_feature_count_" ) )
      continue;
    if ( startsWith( triggerName, "trigger_delete_feature_count_" ) )
      continue;
    triggerNames.push_back( name );
    triggerCmds.push_back( sql );
  }
  statament.close();
}

void tables( std::shared_ptr<Sqlite3Db> db,
             const std::string &dbName,
             std::vector<std::string> &tableNames )
{
  tableNames.clear();
  std::string all_tables_sql = "SELECT name FROM " + dbName + ".sqlite_master\n"
                               " WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%'\n"
                               " ORDER BY name";
  Sqlite3Stmt statament;
  statament.prepare( db, "%s", all_tables_sql.c_str() );
  while ( SQLITE_ROW == sqlite3_step( statament.get() ) )
  {
    const char *name = ( const char * )sqlite3_column_text( statament.get(), 0 );
    if ( !name )
      continue;

    std::string tableName( name );
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
    if ( startsWith( tableName, "gpkg_ogr_contents" ) )
      continue;
    // table handled by triggers rtree_*_geometry_*
    if ( startsWith( tableName, "rtree_" ) )
      continue;
    // internal table for AUTOINCREMENT
    if ( tableName == "sqlite_sequence" )
      continue;

    tableNames.push_back( tableName );
  }

  // result is ordered by name
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

ForeignKeys foreignKeys( std::shared_ptr<Sqlite3Db> db, const std::string &dbName )
{
  std::vector<std::string> fromTableNames;
  tables( db, dbName, fromTableNames );

  ForeignKeys ret;

  for ( const std::string &fromTableName : fromTableNames )
  {
    if ( isLayerTable( fromTableName ) )
    {
      Sqlite3Stmt pStmt;     /* SQL statement being run */
      pStmt.prepare( db, "SELECT * FROM %s.pragma_foreign_key_list(%Q)", dbName.c_str(), fromTableName.c_str() );
      while ( SQLITE_ROW == sqlite3_step( pStmt.get() ) )
      {
        const char *fk_to_table = ( const char * )sqlite3_column_text( pStmt.get(), 2 );
        const char *fk_from = ( const char * )sqlite3_column_text( pStmt.get(), 3 );
        const char *fk_to = ( const char * )sqlite3_column_text( pStmt.get(), 4 );

        if ( fk_to_table && fk_from && fk_to )
        {
          // TODO: this part is not speed-optimized and could be slower for databases with a lot of
          // columns and/or foreign keys. For each entry we grab column names again
          // and we search for index of value in plain std::vector array...
          std::vector<std::string> fromColumnNames = columnNames( db, dbName, fromTableName );
          int fk_from_id = indexOf( fromColumnNames, fk_from );
          if ( fk_from_id < 0 )
            continue;

          std::vector<std::string> toColumnNames = columnNames( db, dbName, fk_to_table );
          int fk_to_id = indexOf( toColumnNames, fk_to );
          if ( fk_to_id < 0 )
            continue;

          TableColumn from( fromTableName, fk_from_id );
          TableColumn to( fk_to_table, fk_to_id );
          ret.insert( std::pair<TableColumn, TableColumn>( from, to ) );
        }
      }
      pStmt.close();
    }
  }

  return ret;
}

/*
 * inspired by sqldiff.c function: columnNames()
 */
std::vector<std::string> columnNames(
  std::shared_ptr<Sqlite3Db> db,
  const std::string &zDb,                /* Database ("main" or "aux") to query */
  const std::string &tableName   /* Name of table to return details of */
)
{
  std::vector<std::string> az;           /* List of column names to be returned */
  int naz = 0;             /* Number of entries in az[] */
  Sqlite3Stmt pStmt;     /* SQL statement being run */
  std::string zPkIdxName;    /* Name of the PRIMARY KEY index */
  int truePk = 0;          /* PRAGMA table_info indentifies the PK to use */
  int nPK = 0;             /* Number of PRIMARY KEY columns */
  int i, j;                /* Loop counters */

  /* Figure out what the true primary key is for the table.
  **   *  For WITHOUT ROWID tables, the true primary key is the same as
  **      the schema PRIMARY KEY, which is guaranteed to be present.
  **   *  For rowid tables with an INTEGER PRIMARY KEY, the true primary
  **      key is the INTEGER PRIMARY KEY.
  **   *  For all other rowid tables, the rowid is the true primary key.
  */
  const char *zTab = tableName.c_str();
  pStmt.prepare( db, "PRAGMA %s.index_list=%Q", zDb.c_str(), zTab );
  while ( SQLITE_ROW == sqlite3_step( pStmt.get() ) )
  {
    if ( sqlite3_stricmp( ( const char * )sqlite3_column_text( pStmt.get(), 3 ), "pk" ) == 0 )
    {
      zPkIdxName = ( const char * ) sqlite3_column_text( pStmt.get(), 1 );
      break;
    }
  }
  pStmt.close();

  if ( !zPkIdxName.empty() )
  {
    int nKey = 0;
    int nCol = 0;
    truePk = 0;
    pStmt.prepare( db, "PRAGMA %s.index_xinfo=%Q", zDb.c_str(), zPkIdxName.c_str() );
    while ( SQLITE_ROW == sqlite3_step( pStmt.get() ) )
    {
      nCol++;
      if ( sqlite3_column_int( pStmt.get(), 5 ) ) { nKey++; continue; }
      if ( sqlite3_column_int( pStmt.get(), 1 ) >= 0 ) truePk = 1;
    }
    if ( nCol == nKey ) truePk = 1;
    if ( truePk )
    {
      nPK = nKey;
    }
    else
    {
      nPK = 1;
    }
    pStmt.close();
  }
  else
  {
    truePk = 1;
    nPK = 1;
  }
  pStmt.prepare( db, "PRAGMA %s.table_info=%Q", zDb.c_str(), zTab );

  naz = nPK;
  az.resize( naz );
  while ( SQLITE_ROW == sqlite3_step( pStmt.get() ) )
  {
    int iPKey;
    std::string name = ( char * )sqlite3_column_text( pStmt.get(), 1 );
    if ( truePk && ( iPKey = sqlite3_column_int( pStmt.get(), 5 ) ) > 0 )
    {
      az[iPKey - 1] = name;
    }
    else
    {
      az.push_back( name );
    }
  }
  pStmt.close();

  /* If this table has an implicit rowid for a PK, figure out how to refer
  ** to it. There are three options - "rowid", "_rowid_" and "oid". Any
  ** of these will work, unless the table has an explicit column of the
  ** same name.  */
  if ( az[0].empty() )
  {
    std::vector<std::string> azRowid = { "rowid", "_rowid_", "oid" };
    for ( i = 0; i < azRowid.size(); i++ )
    {
      for ( j = 1; j < naz; j++ )
      {
        if ( az[j] == azRowid[i] ) break;
      }
      if ( j >= naz )
      {
        az[0] = azRowid[i];
        break;
      }
    }
    if ( az[0].empty() )
    {
      az.clear();
    }
  }

  return az;
}

std::shared_ptr<Sqlite3Db> blankGeopackageDb()
{
  std::shared_ptr<Sqlite3Db> db = std::make_shared<Sqlite3Db>();
  db->open( ":memory:" );
  std::string cmd = "CREATE TABLE gpkg_contents (table_name TEXT NOT NULL PRIMARY KEY,data_type TEXT NOT NULL,identifier TEXT UNIQUE,description TEXT DEFAULT '',last_change DATETIME NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),min_x DOUBLE, min_y DOUBLE,max_x DOUBLE, max_y DOUBLE,srs_id INTEGER)";
  Sqlite3Stmt statament;
  statament.prepare( db, "%s", cmd.c_str() );
  sqlite3_step( statament.get() );
  statament.close();

  bool success = register_gpkg_extensions( db );
  if ( !success )
  {
    throw GeoDiffException( "Unable to enable sqlite3/gpkg extensions" );
  }
  return db;
}

bool has_same_table_schema( std::shared_ptr<Sqlite3Db> db, const std::string &tableName, std::string &errStr )
{
  // inspired by sqldiff.c function: static void summarize_one_table(const char *zTab, FILE *out);
  if ( sqlite3_table_column_metadata( db->get(), "main", tableName.c_str(), 0, 0, 0, 0, 0, 0 ) )
  {
    /* Table missing from source */
    errStr = tableName + " missing from first database";
    return false;
  }

  std::vector<std::string> az = columnNames( db, "main", tableName );
  std::vector<std::string> az2 = columnNames( db, "aux", tableName );
  if ( az.size() != az2.size() )
  {
    errStr = "Table " + tableName + " has different number of columns";
    return false;
  }


  for ( size_t n = 0; n < az.size(); ++n )
  {
    if ( az[n] != az2[n] )
    {
      errStr = "Table " + tableName + " has different name of columns: " + az[n] + " vs " + az2[n];
      return false;
    }

    /* no need to check type:

       from sqlite.h:
         SQLite uses dynamic run-time typing. So just because a column
         is declared to contain a particular type does not mean that the
         data stored in that column is of the declared type.  SQLite is
         strongly typed, but the typing is dynamic not static.  ^Type
         is associated with individual values, not with the containers
         used to hold those values.
    */
  }

  return true;
}

std::string convertGeometryToWKT( std::shared_ptr<Sqlite3Db> db, sqlite3_value *wkb )
{
  if ( !wkb )
    return std::string();

  int type = sqlite3_value_type( wkb );
  if ( type != SQLITE_BLOB )
    return std::string();

  Sqlite3Stmt pStmt;
  std::string ret;
  pStmt.prepare( db, "SELECT ST_AsText(?)" );

  int rc = sqlite3_bind_value( pStmt.get(), 1, wkb );
  if ( rc != SQLITE_OK )
  {
    throw GeoDiffException( "sqlite3_bind_blob error" );
  }

  const char *blob = nullptr;
  while ( SQLITE_ROW == sqlite3_step( pStmt.get() ) )
  {
    blob = ( const char * ) sqlite3_column_text( pStmt.get(), 0 );
    if ( blob )
    {
      ret = blob;
      break;
    }
  }
  pStmt.close();
  return ret;
}

void get_primary_key( Sqlite3ChangesetIter &pp, int pOp, int &fid, int &nColumn )
{
  if ( !pp.get() )
    throw GeoDiffException( "internal error in _get_primary_key" );

  unsigned char *pabPK;
  int pnCol;
  int rc = sqlite3changeset_pk( pp.get(),  &pabPK, &pnCol );
  if ( rc )
  {
    throw GeoDiffException( "internal error in _get_primary_key" );
  }

  // lets assume for now it has only one PK and it is int...
  int pk_column_number = -1;
  for ( int i = 0; i < pnCol; ++i )
  {
    if ( pabPK[i] == 0x01 )
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
  sqlite3_value *ppValue = nullptr;
  if ( pOp == SQLITE_INSERT )
  {
    pp.newValue( pk_column_number, &ppValue );
  }
  else if ( pOp == SQLITE_DELETE || pOp == SQLITE_UPDATE )
  {
    pp.oldValue( pk_column_number, &ppValue );
  }
  if ( !ppValue )
    throw GeoDiffException( "internal error in _get_primary_key: unable to get value of primary key" );

  int type = sqlite3_value_type( ppValue );
  if ( type == SQLITE_INTEGER )
  {
    int val = sqlite3_value_int( ppValue );
    fid = val;
    return;
  }
  else if ( type == SQLITE_TEXT )
  {
    const unsigned char *valT = sqlite3_value_text( ppValue );
    int hash = 0;
    int len = strlen( ( const char * ) valT );
    for ( int i = 0; i < len; i++ )
    {
      hash = 33 * hash + ( unsigned char )valT[i];
    }
    fid = hash;
  }
  else
  {
    throw GeoDiffException( "internal error in _get_primary_key: unsuported type of primary key" );
  }
}

bool register_gpkg_extensions( std::shared_ptr<Sqlite3Db> db )
{
  // register GPKG functions like ST_IsEmpty
  int rc = sqlite3_enable_load_extension( db->get(), 1 );
  if ( rc )
  {
    return false;
  }

  rc = sqlite3_gpkg_auto_init( db->get(), NULL, NULL );
  if ( rc )
  {
    return false;
  }

  return true;
}

bool isGeoPackage( std::shared_ptr<Sqlite3Db> db )
{
  std::vector<std::string> tableNames;
  tables( db,
          "main",
          tableNames );

  return std::find( tableNames.begin(), tableNames.end(), "gpkg_contents" ) != tableNames.end();
}

void flushString( const std::string &filename, const std::string &str )
{
  std::ofstream out( filename );
  out << str;
  out.close();
}

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

ConflictItem::ConflictItem( int column, std::shared_ptr<Sqlite3Value> base, std::shared_ptr<Sqlite3Value> theirs, std::shared_ptr<Sqlite3Value> ours )
  : mColumn( column )
  , mBase( base )
  , mTheirs( theirs )
  , mOurs( ours )
{

}

std::shared_ptr<Sqlite3Value> ConflictItem::base() const
{
  return mBase;
}

std::shared_ptr<Sqlite3Value> ConflictItem::theirs() const
{
  return mTheirs;
}

std::shared_ptr<Sqlite3Value> ConflictItem::ours() const
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
