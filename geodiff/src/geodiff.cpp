/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffutils.hpp"
#include "geodiffrebase.hpp"
#include "geodiffexporter.hpp"
#include "geodifflogger.hpp"

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
  return "0.8.6";
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

void GEODIFF_setLoggerCallback( GEODIFF_LoggerCallback loggerCallback )
{
  Logger::instance().setCallback( loggerCallback );
}

void GEODIFF_setMaximumLoggerLevel( GEODIFF_LoggerLevel maxLogLevel )
{
  Logger::instance().setMaxLogLevel( maxLogLevel );
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
    sqlBuf.printf( "ATTACH '%q' AS aux", base );
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

int GEODIFF_createRebasedChangeset( const char *base,
                                    const char *modified,
                                    const char *changeset_their,
                                    const char *changeset,
                                    const char *conflictfile
                                  )
{
  if ( !conflictfile )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_createRebasedChangeset" );
    return GEODIFF_ERROR;
  }
  fileremove( conflictfile );

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

    ForeignKeys fks = foreignKeys( db, "main" );
    if ( !fks.empty() )
    {
      Logger::instance().error( "Unable to perform rebase for database with foreign keys" );
      return GEODIFF_ERROR;
    }

    TmpFile changeset_BASE_MODIFIED( std::string( changeset ) + "_BASE_MODIFIED" );
    int rc = GEODIFF_createChangeset( base, modified, changeset_BASE_MODIFIED.c_path() );
    if ( rc != GEODIFF_SUCCESS )
      return rc;

    std::vector<ConflictFeature> conflicts;
    rc = rebase( changeset_their, changeset, changeset_BASE_MODIFIED.path(), conflicts );
    if ( rc == GEODIFF_SUCCESS )
    {
      // output conflicts
      if ( conflicts.empty() )
      {
        Logger::instance().debug( "No conflicts present" );
      }
      else
      {
        GeoDiffExporter exporter;
        std::string res = exporter.toJSON( conflicts );
        flushString( conflictfile, res );
      }
    }
    return rc;
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
  if ( !jsonfile || !changeset )
  {
    Logger::instance().error( "NULL arguments to listChangesJSON" );
    return GEODIFF_ERROR;
  }

  try
  {
    Buffer buf;
    buf.read( changeset );
    if ( buf.isEmpty() )
    {
      return 0;
    }

    GeoDiffExporter exporter;
    std::string res = onlySummary ? exporter.toJSONSummary( buf ) : exporter.toJSON( buf );
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

int GEODIFF_invertChangeset( const char *changeset, const char *changeset_inv )
{
  if ( !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_invertChangeset" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( changeset ) )
  {
    Logger::instance().error( "Missing input files in GEODIFF_invertChangeset" );
    return GEODIFF_ERROR;
  }

  try
  {
    Buffer buf;
    buf.read( changeset );
    if ( buf.isEmpty() )
    {
      return GEODIFF_SUCCESS;
    }

    int pnOut = 0;
    void *ppOut = nullptr;

    int rc = sqlite3changeset_invert(
               buf.size(), buf.v_buf(),       /* Input changeset */
               &pnOut, &ppOut        /* OUT: Inverse of input */
             );

    if ( rc )
    {
      Logger::instance().error( "Unable to perform sqlite3changeset_invert" );
      return GEODIFF_ERROR;
    }

    Buffer out;
    out.read( pnOut, ppOut );
    out.write( changeset_inv );

    return GEODIFF_SUCCESS;
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
}

int GEODIFF_rebase( const char *base,
                    const char *modified_their,
                    const char *modified,
                    const char *conflictfile )
{
  if ( !base || !modified_their || !modified || !conflictfile )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_rebase" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( base ) || !fileexists( modified_their ) || !fileexists( modified ) )
  {
    Logger::instance().error( "Missing input files in GEODIFF_rebase" );
    return GEODIFF_ERROR;
  }

  try
  {
    std::string root = std::string( modified );

    TmpFile base2theirs( root + "_base2theirs.bin" );
    if ( GEODIFF_createChangeset( base, modified_their, base2theirs.c_path() ) != GEODIFF_SUCCESS )
    {
      Logger::instance().error( "Unable to perform GEODIFF_createChangeset base2theirs" );
      return GEODIFF_ERROR;
    }

    // situation 1: base2theirs is null, so we do not need rebase. modified is already fine
    if ( !GEODIFF_hasChanges( base2theirs.c_path() ) )
    {
      return GEODIFF_SUCCESS;
    }

    TmpFile base2modified( root + "_base2modified.bin" );
    if ( GEODIFF_createChangeset( base, modified, base2modified.c_path() ) != GEODIFF_SUCCESS )
    {
      Logger::instance().error( "Unable to perform GEODIFF_createChangeset base2modified" );
      return GEODIFF_ERROR;
    }

    // situation 2: we do not have changes (modified == base), so result is modified_theirs
    if ( !GEODIFF_hasChanges( base2modified.c_path() ) )
    {
      if ( GEODIFF_applyChangeset( modified, base2theirs.c_path() ) != GEODIFF_SUCCESS )
      {
        Logger::instance().error( "Unable to perform GEODIFF_applyChangeset base2theirs" );
        return GEODIFF_ERROR;
      }

      return GEODIFF_SUCCESS;
    }

    // situation 3: we have changes both in ours and theirs

    // 3A) Create all changesets
    TmpFile theirs2final( root + "_theirs2final.bin" );
    if ( GEODIFF_createRebasedChangeset( base, modified, base2theirs.c_path(), theirs2final.c_path(), conflictfile ) != GEODIFF_SUCCESS )
    {
      Logger::instance().error( "Unable to perform GEODIFF_createChangeset theirs2final" );
      return GEODIFF_ERROR;
    }

    TmpFile modified2base( root + "_modified2base.bin" );
    if ( GEODIFF_invertChangeset( base2modified.c_path(), modified2base.c_path() ) != GEODIFF_SUCCESS )
    {
      Logger::instance().error( "Unable to perform GEODIFF_invertChangeset modified2base" );
      return GEODIFF_ERROR;
    }

    // 3B) concat to single changeset
    TmpFile modified2final( root + "_modified2final.bin" );
    bool error = concatChangesets( modified2base.path(), base2theirs.path(), theirs2final.path(), modified2final.path() );
    if ( error )
    {
      Logger::instance().error( "Unable to perform concatChangesets" );
      return GEODIFF_ERROR;
    }

    // 3C) apply at once
    if ( GEODIFF_applyChangeset( modified, modified2final.c_path() ) != GEODIFF_SUCCESS )
    {
      Logger::instance().error( "Unable to perform GEODIFF_applyChangeset modified2final" );
      return GEODIFF_ERROR;
    }

    return GEODIFF_SUCCESS;
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
}
