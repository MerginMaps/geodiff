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
#include <assert.h>
#include <sqlite3.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <string>
#include <vector>
#include <iostream>

void strInit( Str *p )
{
  p->z = 0;
  p->nAlloc = 0;
  p->nUsed = 0;
}


void strFree( Str *p )
{
  sqlite3_free( p->z );
  strInit( p );
}

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

std::string sqlite_value_2str( sqlite3_value *ppValue )
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
  return val;
}

int changesetIter2Str( sqlite3_changeset_iter *pp )
{
  if ( !pp )
    return 0;

  int rc;
  const char *pzTab;
  int pnCol;
  int pOp;
  int pbIndirect;
  rc = sqlite3changeset_op(
         pp,  /* Iterator object */
         &pzTab,             /* OUT: Pointer to table name */
         &pnCol,                     /* OUT: Number of columns in table */
         &pOp,                       /* OUT: SQLITE_INSERT, DELETE or UPDATE */
         &pbIndirect                 /* OUT: True for an 'indirect' change */
       );
  std::string s = pOpToStr( pOp );
  std::cout << " " << pzTab << " " << s << "    columns " << pnCol << "    indirect " << pbIndirect << std::endl;

  sqlite3_value *ppValueOld;
  sqlite3_value *ppValueNew;
  for ( int i = 0; i < pnCol; ++i )
  {
    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_INSERT )
    {
      rc = sqlite3changeset_new( pp, i, &ppValueNew );
      assert( rc == SQLITE_OK );
    }
    else
      ppValueNew = 0;

    if ( pOp == SQLITE_UPDATE || pOp == SQLITE_DELETE )
    {
      rc = sqlite3changeset_old( pp, i, &ppValueOld );
      assert( rc == SQLITE_OK );
    }
    else
      ppValueOld = 0;

    std::cout << "  " << i << ": " << sqlite_value_2str( ppValueOld ) << "->" << sqlite_value_2str( ppValueNew ) << std::endl;
  }
  std::cout << std::endl;


  return pnCol;
}

static void runtimeError( const char *zFormat, ... )
{
  // TODO do not use!
  va_list ap;
  // fprintf(stderr, "%s: ", zArgv0);
  va_start( ap, zFormat );
  vfprintf( stderr, zFormat, ap );
  va_end( ap );
  fprintf( stderr, "\n" );
  exit( 1 );
}

void errorLogCallback( void *pArg, int iErrCode, const char *zMsg )
{
  fprintf( stderr, "(%d)%s\n", iErrCode, zMsg );
}

void strPrintf( Str *p, const char *zFormat, ... )
{
  int nNew;
  for ( ;; )
  {
    if ( p->z )
    {
      va_list ap;
      va_start( ap, zFormat );
      sqlite3_vsnprintf( p->nAlloc - p->nUsed, p->z + p->nUsed, zFormat, ap );
      va_end( ap );
      nNew = ( int )strlen( p->z + p->nUsed );
    }
    else
    {
      nNew = p->nAlloc;
    }
    if ( p->nUsed + nNew < p->nAlloc - 1 )
    {
      p->nUsed += nNew;
      break;
    }
    p->nAlloc = p->nAlloc * 2 + 1000;
    p->z = ( char * ) sqlite3_realloc( p->z, p->nAlloc );
    if ( p->z == 0 ) runtimeError( "out of memory" );
  }
}

static sqlite3_stmt *db_vprepare( sqlite3 *db, const char *zFormat, va_list ap )
{
  char *zSql;
  int rc;
  sqlite3_stmt *pStmt;

  zSql = sqlite3_vmprintf( zFormat, ap );
  if ( zSql == 0 ) runtimeError( "out of memory" );
  rc = sqlite3_prepare_v2( db, zSql, -1, &pStmt, 0 );
  if ( rc )
  {
    runtimeError( "SQL statement error: %s\n\"%s\"", sqlite3_errmsg( db ),
                  zSql );
  }
  sqlite3_free( zSql );
  return pStmt;
}

sqlite3_stmt *db_prepare( sqlite3 *db, const char *zFormat, ... )
{
  va_list ap;
  sqlite3_stmt *pStmt;
  va_start( ap, zFormat );
  pStmt = db_vprepare( db, zFormat, ap );
  va_end( ap );
  return pStmt;
}

const char *all_tables_sql()
{
  return
    "SELECT name FROM main.sqlite_master\n"
    " WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%'\n"
    " UNION\n"
    "SELECT name FROM aux.sqlite_master\n"
    " WHERE type='table' AND sql NOT LIKE 'CREATE VIRTUAL%%'\n"
    " ORDER BY name";
}


void cp( const std::string &to, const std::string &from )
{
  boost::filesystem::copy_file( from, to,
                                boost::filesystem::copy_option::overwrite_if_exists );
}

long slurp( char const *path, char **buf )
{
  FILE  *fp;
  size_t fsz;
  long   off_end;
  int    rc;

  /* Open the file */
  fp = fopen( path, "r" );
  if ( NULL == fp )
  {
    return -1L;
  }

  /* Seek to the end of the file */
  rc = fseek( fp, 0L, SEEK_END );
  if ( 0 != rc )
  {
    return -1L;
  }

  /* Byte offset to the end of the file (size) */
  if ( 0 > ( off_end = ftell( fp ) ) )
  {
    return -1L;
  }
  fsz = ( size_t )off_end;

  /* Allocate a buffer to hold the whole file */
  *buf = ( char * ) malloc( fsz );
  if ( NULL == *buf )
  {
    return -1L;
  }

  /* Rewind file pointer to start of file */
  rewind( fp );

  /* Slurp file into buffer */
  if ( fsz != fread( *buf, 1, fsz, fp ) )
  {
    free( *buf );
    return -1L;
  }

  /* Close the file */
  if ( EOF == fclose( fp ) )
  {
    free( *buf );
    return -1L;
  }

  /* Return the file size */
  return ( long )fsz;
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
