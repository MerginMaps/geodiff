/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffutils.hpp"
#include "geodiffrebase.hpp"
#include "geodiffexporter.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include <sqlite3.h>

#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>

const char *GEODIFF_version()
{
  return "0.6.0";
}

void _errorLogCallback( void *pArg, int iErrCode, const char *zMsg )
{
  std::string msg = "SQLITE3: (" + std::to_string( iErrCode ) + ")" + zMsg;
  Logger::instance().error( msg );
}

static bool gInitialized = false;
void GEODIFF_init()
{
  if ( !gInitialized )
  {
    gInitialized = true;
    sqlite3_config( SQLITE_CONFIG_LOG, _errorLogCallback );
    sqlite3_initialize();
  }
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

    // names are sorted and gpkg tables are filtered out
    std::vector<std::string> mainTableNames;
    tables( db, "main", mainTableNames );

    // names are sorted and gpkg tables are filtered out
    std::vector<std::string> auxTableNames;
    tables( db, "aux", auxTableNames );

    if ( auxTableNames.size() != mainTableNames.size() )
    {
      Logger::instance().error( "Modified does contain different number of tables than base in GEODIFF_createChangeset" );
      return GEODIFF_UNSUPPORTED_CHANGE;
    }

    for ( size_t i = 0; i < mainTableNames.size(); ++i )
    {
      const std::string &table = mainTableNames.at( i );
      const std::string &auxTable = auxTableNames.at( i );
      if ( auxTable != table )
      {
        Logger::instance().error( "Modified renamed table " + table + " to " + auxTable + " in GEODIFF_createChangeset" );
        return GEODIFF_UNSUPPORTED_CHANGE;
      }

      std::string errMsg;
      bool hasSameSchema = has_same_table_schema( db, table, errMsg );
      if ( !hasSameSchema )
      {
        Logger::instance().error( errMsg + " in GEODIFF_createChangeset" );
        return GEODIFF_UNSUPPORTED_CHANGE;
      }

      int rc = sqlite3session_attach( session.get(), table.c_str() );
      if ( rc )
      {
        Logger::instance().error( "Unable to attach session to database in GEODIFF_createChangeset" );
        return GEODIFF_ERROR;
      }
      rc = sqlite3session_diff( session.get(), "aux", table.c_str(), NULL );
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

static int conflict_callback( void *ctx, int conflict, sqlite3_changeset_iter *iterator )
{
  int *nconflicts = static_cast<int *>( ctx );
  *nconflicts += 1;
  std::string s = conflict2Str( conflict );
  std::string vals = GeoDiffExporter::toString( iterator );
  Logger::instance().warn( "CONFLICT: " + s  + ": " + vals );
  return SQLITE_CHANGESET_REPLACE;
}

int GEODIFF_applyChangeset( const char *base, const char *changeset )
{
  if ( !base || !changeset )
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
    int nconflicts = 0;

    // read changeset to buffer
    Buffer cbuf;
    cbuf.read( changeset );
    if ( cbuf.isEmpty() )
    {
      Logger::instance().debug( "--- no changes ---" );
      return GEODIFF_SUCCESS;
    }

    std::shared_ptr<Sqlite3Db> db = std::make_shared<Sqlite3Db>();
    db->open( base );

    if ( isGeoPackage( db ) )
    {
      bool success = register_gpkg_extensions( db );
      if ( !success )
      {
        Logger::instance().error( "Unable to enable sqlite3/gpkg extensions" );
        return GEODIFF_ERROR;
      }
    }

    // get all triggers sql commands
    // that we do not recognize (gpkg triggers are filtered)
    std::vector<std::string> triggerNames;
    std::vector<std::string> triggerCmds;
    triggers( db, triggerNames, triggerCmds );

    Sqlite3Stmt statament;
    for ( std::string name : triggerNames )
    {
      statament.prepare( db, "drop trigger %s", name.c_str() );
      sqlite3_step( statament.get() );
      statament.close();
    }

    // apply changeset
    int rc = sqlite3changeset_apply( db->get(), cbuf.size(), cbuf.v_buf(), nullptr, conflict_callback, &nconflicts );
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

int GEODIFF_createRebasedChangeset( const char *base, const char *modified, const char *changeset_their, const char *changeset )
{
  try
  {
    // get all triggers sql commands
    // and make sure that there are only triggers we recognize
    // we deny rebase changesets with unrecognized triggers
    std::shared_ptr<Sqlite3Db> db = std::make_shared<Sqlite3Db>();
    db->open( modified );
    std::vector<std::string> triggerNames;
    std::vector<std::string> triggerCmds;
    triggers( db, triggerNames, triggerCmds );
    if ( !triggerNames.empty() )
    {
      Logger::instance().error( "Unable to perform rebase for database with unknown triggers" );
      return GEODIFF_ERROR;
    }

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

int GEODIFF_hasChanges( const char *changeset )
{
  try
  {
    Buffer buf;
    buf.read( changeset );
    if ( buf.isEmpty() )
    {
      return 0;
    }

    Sqlite3ChangesetIter pp;
    pp.start( buf );
    while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
    {
      // ok we have at least one change
      return 1;
    }
    return 0; // no changes
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return -1;
  }
}

int GEODIFF_changesCount( const char *changeset )
{
  try
  {
    int nchanges = 0;
    Buffer buf;
    buf.read( changeset );
    if ( buf.isEmpty() )
    {
      return nchanges;
    }

    Sqlite3ChangesetIter pp;
    pp.start( buf );
    while ( SQLITE_ROW == sqlite3changeset_next( pp.get() ) )
    {
      ++nchanges;
    }
    return nchanges;
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return -1;
  }
}

static int listChangesJSON( const char *changeset, const char *jsonfile, bool onlySummary )
{
  try
  {
    Buffer buf;
    buf.read( changeset );
    if ( buf.isEmpty() )
    {
      return 0;
    }

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

    std::string res = onlySummary ? GeoDiffExporter::toJSONSummary( buf ) : GeoDiffExporter::toJSON( db, buf );
    flushString( jsonfile, res );
    return GEODIFF_SUCCESS;
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
}

int GEODIFF_listChanges( const char *changeset, const char *jsonfile )
{
  return listChangesJSON( changeset, jsonfile, false );
}

int GEODIFF_listChangesSummary( const char *changeset, const char *jsonfile )
{
  return listChangesJSON( changeset, jsonfile, true );
}
