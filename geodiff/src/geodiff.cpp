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
  if ( !base || !modified || !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_createChangeset" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( base ) || !fileexists( modified ) )
  {
    Logger::instance().error( "Missing input files in GEODIFF_createChangeset" );
    return GEODIFF_ERROR;
  }

  try
  {
    std::shared_ptr<Sqlite3Db> db = std::make_shared<Sqlite3Db>();
    db->open( modified );

    Buffer sqlBuf;
    sqlBuf.printf( "ATTACH '%s' AS aux", base );
    db->exec( sqlBuf );

    Sqlite3Session session;
    session.create( db, "main" );

    Sqlite3Stmt statament;
    statament.prepare( db, "%s", all_tables_sql() );
    while ( SQLITE_ROW == sqlite3_step( statament.get() ) )
    {
      int rc = sqlite3session_attach( session.get(), ( const char * )sqlite3_column_text( statament.get(), 0 ) );
      if ( rc )
      {
        Logger::instance().error( "Unable to attach session to database in GEODIFF_createChangeset" );
        return GEODIFF_ERROR;
      }
      rc = sqlite3session_diff( session.get(), "aux", ( const char * )sqlite3_column_text( statament.get(), 0 ), NULL );
      if ( rc )
      {
        Logger::instance().error( "Unable to diff tables in GEODIFF_createChangeset" );
        return GEODIFF_ERROR;
      }
    }

    Buffer changesetBuf;
    changesetBuf.read( session );
    changesetBuf.write( changeset );

    return GEODIFF_SUCCESS;
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
}

static int nconflicts = 0;

static int conflict_callback( void *ctx, int conflict, sqlite3_changeset_iter *iterator )
{
  nconflicts++;
  std::string s = conflict2Str( conflict );
  Logger::instance().warn( "CONFLICT: " + s );
  changesetIter2Str( iterator );
  return SQLITE_CHANGESET_REPLACE;
}

int GEODIFF_applyChangeset( const char *base, const char *patched, const char *changeset )
{
  if ( !base || !patched || !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_applyChangeset" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( base ) || !fileexists( changeset ) )
  {
    Logger::instance().error( "Missing input files in GEODIFF_applyChangeset" );
    return GEODIFF_ERROR;
  }

  try
  {
    // static variable... how ugly!!!
    nconflicts = 0;

    // make a copy
    filecopy( patched, base );

    // read changeset to buffer
    Buffer cbuf;
    cbuf.read( changeset );
    if ( cbuf.isEmpty() )
    {
      Logger::instance().info( "--- no changes ---" );
      return GEODIFF_SUCCESS;
    }

    std::shared_ptr<Sqlite3Db> db = std::make_shared<Sqlite3Db>();
    db->open( patched );

    // get all triggers sql commands
    std::vector<std::string> triggerNames;
    std::vector<std::string> triggerCmds;

    Sqlite3Stmt statament;
    statament.prepare( db, "%s", "select name, sql from sqlite_master where type = 'trigger'" );
    while ( SQLITE_ROW == sqlite3_step( statament.get() ) )
    {
      char *name = ( char * ) sqlite3_column_text( statament.get(), 0 );
      char *sql = ( char * ) sqlite3_column_text( statament.get(), 1 );
      triggerNames.push_back( name );
      triggerCmds.push_back( sql );
    }
    statament.close();

    for ( std::string name : triggerNames )
    {
      statament.prepare( db, "drop trigger %s", name.c_str() );
      sqlite3_step( statament.get() );
      statament.close();
    }

    // apply changeset
    int rc = sqlite3changeset_apply( db->get(), cbuf.size(), cbuf.v_buf(), NULL, conflict_callback, NULL );
    if ( rc )
    {
      Logger::instance().error( "Unable to perform sqlite3changeset_apply" );
      return GEODIFF_ERROR;
    }

    // recreate triggers
    for ( std::string cmd : triggerCmds )
    {
      statament.prepare( db, "%s", cmd.c_str() );
      sqlite3_step( statament.get() );
      statament.close();
    }

    if ( nconflicts > 0 )
    {
      Logger::instance().warn( "NConflicts " + std::to_string( nconflicts ) + " found " );
      return GEODIFF_CONFICTS;
    }
    return GEODIFF_SUCCESS;

  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
}

int GEODIFF_listChanges( const char *changeset )
{
  try
  {
    int nchanges = 0;
    Buffer buf;
    buf.read( changeset );
    if ( buf.isEmpty() )
    {
      Logger::instance().info( "--- no changes ---" );
      return GEODIFF_SUCCESS;
    }

    Sqlite3ChangesetIter pp;
    pp.start( buf );
    while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
    {
      changesetIter2Str( pp.get() );
      nchanges = nchanges + 1 ;
    }
    return nchanges;
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return -1;
  }
}

int GEODIFF_createRebasedChangeset( const char *base, const char *modified, const char *changeset_their, const char *changeset )
{
  try
  {
    std::string changeset_BASE_MODIFIED = std::string( changeset ) + "_BASE_MODIFIED";
    int rc = GEODIFF_createChangeset( base, modified, changeset_BASE_MODIFIED.c_str() );
    if ( rc != GEODIFF_SUCCESS )
      return rc;

    return rebase( changeset_their, changeset, changeset_BASE_MODIFIED );
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
}
