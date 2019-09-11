/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include <sqlite3.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>

#include "geodiffsqlitedriver.hpp"
#include "sqlite3.h"
#include "geodiffutils.hpp"
#include "geodiff.h"
#include "geodiffsqliteutils.hpp"
#include "geodiffsqliterebase.hpp"

static int nconflicts = 0;

static int conflict_callback( void *ctx, int conflict, sqlite3_changeset_iter *iterator )
{
  nconflicts++;
  std::string s = conflict2Str( conflict );
  std::string vals = Sqlite3ChangesetIter::toString( iterator );
  Logger::instance().warn( "CONFLICT: " + s  + ": " + vals );
  return SQLITE_CHANGESET_REPLACE;
}

void _errorLogCallback( void *pArg, int iErrCode, const char *zMsg )
{
  std::string msg = "SQLITE3: (" + std::to_string( iErrCode ) + ")" + zMsg;
  Logger::instance().error( msg );
}


SqliteDriver::SqliteDriver(): Driver( "SQLITE" ) {}

void SqliteDriver::init()
{
  sqlite3_config( SQLITE_CONFIG_LOG, _errorLogCallback );
  sqlite3_initialize();
}

SqliteDriver::SqliteDriver( const std::string &name ): Driver( name ) {}

SqliteDriver::~SqliteDriver()
{

}

int SqliteDriver::createChangeset( const std::string &base, const std::string &modified, const std::string &changeset )
{
  std::shared_ptr<Sqlite3Db> db = std::make_shared<Sqlite3Db>();
  db->open( modified );

  Buffer sqlBuf;
  sqlBuf.printf( "ATTACH '%s' AS aux", base.c_str() );
  db->exec( sqlBuf );

  Sqlite3Session session;
  session.create( db, "main" );

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

int SqliteDriver::applyChangeset( const std::string &base, const std::string &patched, const std::string &changeset )
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
    Logger::instance().debug( "--- no changes ---" );
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

int SqliteDriver::listChanges( const std::string &changeset, int &nchanges )
{
  nchanges = 0;
  Buffer buf;
  buf.read( changeset );
  if ( buf.isEmpty() )
  {
    Logger::instance().info( "--- no changes ---" );
    return nchanges;
  }

  Sqlite3ChangesetIter pp;
  pp.start( buf );
  while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
  {
    std::string msg = Sqlite3ChangesetIter::toString( pp.get() );
    Logger::instance().info( msg );
    nchanges = nchanges + 1 ;
  }
  return GEODIFF_SUCCESS;
}

int SqliteDriver::createRebasedChangeset( const std::string &base, const std::string &modified, const std::string &changeset_their, const std::string &changeset )
{
  std::string changeset_BASE_MODIFIED = std::string( changeset ) + "_BASE_MODIFIED";
  int rc = createChangeset( base, modified, changeset_BASE_MODIFIED.c_str() );
  if ( rc != GEODIFF_SUCCESS )
    return rc;

  return rebase( changeset_their, changeset, changeset_BASE_MODIFIED );
}
