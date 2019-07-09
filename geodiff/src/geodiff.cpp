/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffutils.hpp"
#include "geodiffrebase.hpp"

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

#include <iostream>
#include <string>
#include <vector>

const char *GEODIFF_version()
{
  return "0.1.0";
}

void GEODIFF_init()
{
  sqlite3_config( SQLITE_CONFIG_LOG, errorLogCallback );
  sqlite3_initialize();
}

int GEODIFF_createChangeset( const char *base, const char *modified, const char *changeset )
{
  Str str;
  sqlite3 *db;
  sqlite3_session *session;
  int rc;
  int size;
  void *buf;
  FILE *f;
  sqlite3_stmt *pStmt;

  strInit( &str );

  printf( "%s\n%s\n%s\n", modified, base, changeset );

  rc = sqlite3_open( modified, &db );
  if ( rc )
  {
    printf( "err diff 1" );
    return GEODIFF_ERROR;
  }

  strPrintf( &str, "ATTACH '%s' AS aux", base );
  rc = sqlite3_exec( db, str.z, NULL, 0, NULL );
  if ( rc )
  {
    printf( "err diff 2" );
    return GEODIFF_ERROR;
  }

  rc = sqlite3session_create( db, "main", &session );
  if ( rc )
  {
    printf( "err diff 3" );
    return GEODIFF_ERROR;
  }

  pStmt = db_prepare( db, "%s", all_tables_sql() );
  while ( SQLITE_ROW == sqlite3_step( pStmt ) )
  {
    rc = sqlite3session_attach( session, ( const char * )sqlite3_column_text( pStmt, 0 ) );
    if ( rc )
    {
      printf( "err diff 4" );
      return GEODIFF_ERROR;
    }
    rc = sqlite3session_diff( session, "aux", ( const char * )sqlite3_column_text( pStmt, 0 ), NULL );
    if ( rc )
    {
      printf( "err diff 5" );
      return GEODIFF_ERROR;
    }
  }
  sqlite3_finalize( pStmt );

  /* Create a changeset */
  rc = sqlite3session_changeset( session, &size, &buf );
  if ( rc )
  {
    printf( "err diff 6" );
    return GEODIFF_ERROR;
  }

  f = fopen( changeset, "w" );
  if ( !f )
  {
    std::cout << "Unable to open file " << changeset << " for writing" << std::endl;
    return GEODIFF_ERROR;
  }
  printf( "size: %d\n", size );
  fwrite( buf, size, 1, f );
  fclose( f );
  sqlite3_free( buf );

  sqlite3session_delete( session );
  sqlite3_close( db );

  strFree( &str );

  return GEODIFF_SUCCESS;
}

static int nconflicts = 0;

static int conflict_callback( void *ctx, int conflict, sqlite3_changeset_iter *iterator )
{
  nconflicts++;
  std::string s = conflict2Str( conflict );
  printf( "CONFLICT! %s ", s.c_str() );
  changesetIter2Str( iterator );
  return SQLITE_CHANGESET_REPLACE;
}

int GEODIFF_applyChangeset( const char *base, const char *patched, const char *changeset )
{
  // static variable... how ugly!!!
  nconflicts = 0;

  sqlite3 *db;
  char *cbuf;
  size_t csize;
  int rc;
  sqlite3_stmt *pStmt;
  char *name;
  char *sql;

  // TODO consider using sqlite3changeset_apply_strm streamed versions
  cp( patched, base );

  csize = slurp( changeset, ( char ** ) &cbuf );
  if ( csize == 0 )
  {
    printf( "--- no changes ---" );
    return GEODIFF_SUCCESS;
  }

  if ( csize <= 0 )
  {
    printf( "err slurp" );
    return GEODIFF_ERROR;
  }

  rc = sqlite3_open( patched, &db );
  if ( rc )
  {
    printf( "err sqlite3_open" );
    return GEODIFF_ERROR;
  }

  // get all triggers sql commands
  std::vector<std::string> triggerNames;
  std::vector<std::string> triggerCmds;

  pStmt = db_prepare( db, "%s", "select name, sql from sqlite_master where type = 'trigger'" );
  while ( SQLITE_ROW == sqlite3_step( pStmt ) )
  {
    name = ( char * ) sqlite3_column_text( pStmt, 0 );
    // printf("%s", name);
    sql = ( char * ) sqlite3_column_text( pStmt, 1 );
    triggerNames.push_back( name );
    triggerCmds.push_back( sql );
  }
  sqlite3_finalize( pStmt );

  for ( std::string name : triggerNames )
  {
    pStmt = db_prepare( db, "drop trigger %s", name.c_str() );
    sqlite3_step( pStmt );
    sqlite3_finalize( pStmt );
  }

  // error: no such function: ST_IsEmpty otherwise
  // https://gis.stackexchange.com/q/294626/59405
  // rc = sqlite3_enable_load_extension(db, 1);
  // if(rc) RUNTIME_ERROR("sql error 2");

  // rc = sqlite3_load_extension(db, "mod_spatialite.so", NULL, NULL);
  // if(rc) RUNTIME_ERROR("sql error 3");

  // TODO use _v2 and data-> for rebaser!
  rc = sqlite3changeset_apply( db, csize, cbuf, NULL, conflict_callback, NULL );
  if ( rc )
  {
    printf( "err sqlite3changeset_apply" );
    return GEODIFF_ERROR;
  }

  // recreate triggers
  for ( std::string cmd : triggerCmds )
  {
    pStmt = db_prepare( db, "%s", cmd.c_str() );
    sqlite3_step( pStmt );
    sqlite3_finalize( pStmt );
  }

  sqlite3_close( db );

  if ( nconflicts > 0 )
  {
    printf( "NConflicts %d found ", nconflicts );
    return GEODIFF_CONFICTS;
  }
  return GEODIFF_SUCCESS;
}

int GEODIFF_listChanges( const char *changeset )
{
  void *buf; /* Patchset or changeset */
  int size;  /* And its size */
  int rc;
  sqlite3_changeset_iter *pp;
  int nchanges = 0;

  printf( "CHANGES:\n" );
  size = slurp( changeset, ( char ** ) &buf );
  if ( size == 0 )
  {
    printf( " -- no changes! --\n" );
    return nchanges;
  }

  if ( size <= 0 )
  {
    printf( "err list 1" );
    return -1;
  }

  rc = sqlite3changeset_start(
         &pp,
         size,
         buf
       );
  if ( rc != SQLITE_OK )
  {
    printf( "sqlite3changeset_start error %d\n", rc );
    return -1;
  }

  while ( SQLITE_ROW == sqlite3changeset_next( pp ) )
  {
    changesetIter2Str( pp );
    nchanges = nchanges + 1 ;
  }

  sqlite3changeset_finalize( pp );
  free( buf );
  return nchanges;
}


int GEODIFF_createRebasedChangeset( const char *base, const char *modified, const char *changeset_their, const char *changeset )
{
  std::string changeset_BASE_MODIFIED = std::string( changeset ) + "_BASE_MODIFIED";
  int rc = GEODIFF_createChangeset( base, modified, changeset_BASE_MODIFIED.c_str() );
  if ( rc != GEODIFF_SUCCESS )
    return rc;

  return rebase( changeset_their, changeset, changeset_BASE_MODIFIED );
}
