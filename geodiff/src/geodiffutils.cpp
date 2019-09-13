/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffutils.hpp"

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

#ifdef WIN32
#include <windows.h>
#include <tchar.h>
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

Logger::Logger()
{
  levelFromEnv();
}

Logger &Logger::instance()
{
  static Logger instance;
  return instance;
}

Logger::LoggerLevel Logger::level() const
{
  return mLevel;
}

void Logger::debug( const std::string &msg )
{
  log( LevelDebug, msg );
}

void Logger::warn( const std::string &msg )
{
  log( LevelWarnings, msg );
}

void Logger::error( const std::string &msg )
{
  log( LevelErrors, msg );
}

void Logger::error( const GeoDiffException &exp )
{
  log( LevelErrors, exp.what() );
}

void Logger::info( const std::string &msg )
{
  log( LevelInfos, msg );
}

void Logger::log( LoggerLevel level, const std::string &msg )
{
  if ( static_cast<int>( level ) > static_cast<int>( mLevel ) )
    return;

  std::string prefix;
  switch ( level )
  {
    case LevelErrors: prefix = "Error: "; break;
    case LevelWarnings: prefix = "Warn: "; break;
    case LevelDebug: prefix = "Debug: "; break;
    default: break;
  }
  std::cout << prefix << msg << std::endl ;
}

void Logger::levelFromEnv()
{
  char *val = getenv( "GEODIFF_LOGGER_LEVEL" );
  if ( val )
  {
    int level = atoi( val );
    if ( level >= LevelNothing && level <= LevelDebug )
    {
      mLevel = ( LoggerLevel )level;
    }
  }
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
  int rc = sqlite3_open( filename.c_str(), &mDb );
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

std::string Sqlite3ChangesetIter::toString( sqlite3_changeset_iter *pp )
{
  std::ostringstream ret;

  if ( !pp )
    return std::string();

  int rc;
  const char *pzTab;
  int pnCol;
  int pOp;
  int pbIndirect;
  rc = sqlite3changeset_op(
         pp,
         &pzTab,
         &pnCol,
         &pOp,
         &pbIndirect
       );
  std::string s = pOpToStr( pOp );
  ret << " " << pzTab << " " << s << "    columns " << pnCol << "    indirect " << pbIndirect << std::endl;

  sqlite3_value *ppValueOld;
  sqlite3_value *ppValueNew;
  for ( int i = 0; i < pnCol; ++i )
  {
    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_INSERT )
    {
      rc = sqlite3changeset_new( pp, i, &ppValueNew );
      if ( rc != SQLITE_OK )
        throw GeoDiffException( "sqlite3changeset_new" );
    }
    else
      ppValueNew = nullptr;

    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_DELETE )
    {
      rc = sqlite3changeset_old( pp, i, &ppValueOld );
      if ( rc != SQLITE_OK )
        throw GeoDiffException( "sqlite3changeset_old" );
    }
    else
      ppValueOld = nullptr;

    ret << "  " << i << ": " << Sqlite3Value::toString( ppValueOld ) << "->" << Sqlite3Value::toString( ppValueNew ) << std::endl;
  }
  ret << std::endl;
  return ret.str();

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
  FILE *fp = fopen( filename.c_str(), "rb" );
  if ( nullptr == fp )
  {
    throw GeoDiffException( "Unable to open " + filename );
  }

  /* Seek to the end of the file */
  int rc = fseek( fp, 0L, SEEK_END );
  if ( 0 != rc )
  {
    throw GeoDiffException( "Unable to seek the end of " + filename );
  }

  long off_end;
  /* Byte offset to the end of the file (size) */
  if ( 0 > ( off_end = ftell( fp ) ) )
  {
    throw GeoDiffException( "Unable to read file size of " + filename );
  }
  mAlloc = ( size_t )off_end;
  mUsed = mAlloc;

  if ( mAlloc == 0 )
  {
    // empty file
    return;
  }

  /* Allocate a buffer to hold the whole file */
  mZ = ( char * ) sqlite3_malloc( mAlloc );
  if ( mZ == nullptr )
  {
    throw GeoDiffException( "Out of memory to read " + filename + " to internal buffer" );
  }

  /* Rewind file pointer to start of file */
  rewind( fp );

  /* Slurp file into buffer */
  if ( mAlloc != fread( mZ, 1, mAlloc, fp ) )
  {
    throw GeoDiffException( "Unable to read " + filename + " to internal buffer" );
  }

  /* Close the file */
  if ( EOF == fclose( fp ) )
  {
    throw GeoDiffException( "Unable to close " + filename );
  }
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
  FILE *f = fopen( filename.c_str(), "wb" );
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

void putsVarint( FILE *out, sqlite3_uint64 v )
{
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
    fwrite( p, 8, 1, out );
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
    fwrite( p + n + 1, 9 - n, 1, out );
  }
}

void putValue( FILE *out, sqlite3_value *ppValue )
{
  if ( !ppValue )
  {
    putc( 0, out );
  }
  else
  {
    int iDType = sqlite3_value_type( ppValue );
    sqlite3_int64 iX;
    double rX;
    sqlite3_uint64 uX;
    int j;

    putc( iDType, out );
    switch ( iDType )
    {
      case SQLITE_INTEGER:
        iX = sqlite3_value_int64( ppValue );
        memcpy( &uX, &iX, 8 );
        for ( j = 56; j >= 0; j -= 8 ) putc( ( uX >> j ) & 0xff, out );
        break;
      case SQLITE_FLOAT:
        rX = sqlite3_value_double( ppValue );
        memcpy( &uX, &rX, 8 );
        for ( j = 56; j >= 0; j -= 8 ) putc( ( uX >> j ) & 0xff, out );
        break;
      case SQLITE_TEXT:
        iX = sqlite3_value_bytes( ppValue );
        putsVarint( out, ( sqlite3_uint64 )iX );
        fwrite( sqlite3_value_text( ppValue ), 1, ( size_t )iX, out );
        break;
      case SQLITE_BLOB:
        iX = sqlite3_value_bytes( ppValue );
        putsVarint( out, ( sqlite3_uint64 )iX );
        fwrite( sqlite3_value_blob( ppValue ), 1, ( size_t )iX, out );
        break;
      case SQLITE_NULL:
        break;
    }
  }
}

void putValue( FILE *out, int ppValue )
{
  sqlite3_uint64 uX;
  sqlite3_int64 iX = ppValue;

  memcpy( &uX, &iX, 8 );
  putc( SQLITE_INTEGER, out );
  for ( int j = 56; j >= 0; j -= 8 ) putc( ( uX >> j ) & 0xff, out );
}


void filecopy( const std::string &to, const std::string &from )
{
  fileremove( to );

  std::ifstream  src( from, std::ios::binary );
  std::ofstream  dst( to,   std::ios::binary );

  dst << src.rdbuf();
}

void fileremove( const std::string &path )
{
  if ( fileexists( path ) )
  {
    remove( path.c_str() );
  }
}

bool fileexists( const std::string &path )
{
#ifdef WIN32
  WIN32_FIND_DATA FindFileData;
  HANDLE handle = FindFirstFile( path.c_str(), &FindFileData ) ;
  int found = handle != INVALID_HANDLE_VALUE;
  if ( found )
  {
    //FindClose(&handle); this will crash
    FindClose( handle );
  }
  return found;
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

void triggers( std::shared_ptr<Sqlite3Db> db, std::vector<std::string> &triggerNames, std::vector<std::string> &triggerCmds )
{
  triggerNames.clear();
  triggerCmds.clear();

  Sqlite3Stmt statament;
  statament.prepare( db, "%s", "select name, sql from sqlite_master where type = 'trigger'" );
  while ( SQLITE_ROW == sqlite3_step( statament.get() ) )
  {
    char *name = ( char * ) sqlite3_column_text( statament.get(), 0 );
    char *sql = ( char * ) sqlite3_column_text( statament.get(), 1 );

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
    if ( startsWith( triggerName, "gpkg_tile_matrix_" ) )
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

void tables( std::shared_ptr<Sqlite3Db> db, std::vector<std::string> &tableNames )
{
  tableNames.clear();
  std::string all_tables_sql = "SELECT name FROM main.sqlite_master\n"
                               " WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%'\n"
                               " UNION\n"
                               "SELECT name FROM aux.sqlite_master\n"
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
    simple
    sqlite_sequence
    */
    if ( startsWith( tableName, "gpkg_" ) )
      continue;
    if ( startsWith( tableName, "rtree_" ) )
      continue;
    if ( tableName == "sqlite_sequence" )
      continue;

    tableNames.push_back( tableName );
  }
}
