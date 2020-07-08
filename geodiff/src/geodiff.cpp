/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffutils.hpp"
#include "geodiffrebase.hpp"
#include "geodifflogger.hpp"

#include "driver.h"
#include "changesetreader.h"
#include "changesetutils.h"
#include "changesetwriter.h"

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
  return "0.8.2";
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
  return GEODIFF_createChangesetEx( "sqlite", nullptr, base, modified, changeset );
}

int GEODIFF_applyChangeset( const char *base, const char *changeset )
{
  return GEODIFF_applyChangesetEx( "sqlite", nullptr, base, changeset );
}


int GEODIFF_createChangesetEx( const char *driverName, const char *driverExtraInfo,
                               const char *base, const char *modified,
                               const char *changeset )
{
  if ( !driverName || !base || !modified || !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_createChangesetEx" );
    return GEODIFF_ERROR;
  }

  try
  {
    std::map<std::string, std::string> conn;
    conn["base"] = std::string( base );
    conn["modified"] = std::string( modified );
    if ( driverExtraInfo )
      conn["conninfo"] = std::string( driverExtraInfo );
    std::unique_ptr<Driver> driver( Driver::createDriver( std::string( driverName ) ) );
    if ( !driver )
      throw GeoDiffException( "Unable to use driver: " + std::string( driverName ) );
    driver->open( conn );

    ChangesetWriter writer;
    if ( !writer.open( changeset ) )
      throw GeoDiffException( "Unable to open changeset file for writing: " + std::string( changeset ) );
    driver->createChangeset( writer );
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


int GEODIFF_applyChangesetEx( const char *driverName, const char *driverExtraInfo,
                              const char *base, const char *changeset )
{
  if ( !driverName || !base || !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_applyChangesetEx" );
    return GEODIFF_ERROR;
  }

  try
  {
    std::map<std::string, std::string> conn;
    conn["base"] = std::string( base );
    if ( driverExtraInfo )
      conn["conninfo"] = std::string( driverExtraInfo );
    std::unique_ptr<Driver> driver( Driver::createDriver( std::string( driverName ) ) );
    if ( !driver )
      throw GeoDiffException( "Unable to use driver: " + std::string( driverName ) );
    driver->open( conn );

    ChangesetReader reader;
    if ( !reader.open( changeset ) )
      throw GeoDiffException( "Unable to open changeset file for reading: " + std::string( changeset ) );
    if ( reader.isEmpty() )
    {
      Logger::instance().debug( "--- no changes ---" );
      return GEODIFF_SUCCESS;
    }

    driver->applyChangeset( reader );
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
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
      for ( size_t i = 0; i < triggerNames.size(); ++i )
        Logger::instance().debug( "Unexpected trigger: " + triggerNames[i] );
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
        std::string res = conflictsToJSON( conflicts );
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
  if ( !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_hasChanges" );
    return -1;
  }

  ChangesetReader reader;
  if ( !reader.open( changeset ) )
  {
    Logger::instance().error( "Could not open changeset: " + std::string( changeset ) );
    return -1;
  }

  return !reader.isEmpty();
}

int GEODIFF_changesCount( const char *changeset )
{
  if ( !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_changesCount" );
    return -1;
  }

  ChangesetReader reader;
  if ( !reader.open( changeset ) )
  {
    Logger::instance().error( "Could not open changeset: " + std::string( changeset ) );
    return -1;
  }

  int changesCount = 0;
  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
    ++changesCount;

  return changesCount;
}

static int listChangesJSON( const char *changeset, const char *jsonfile, bool onlySummary )
{
  if ( !jsonfile || !changeset )
  {
    Logger::instance().error( "NULL arguments to listChangesJSON" );
    return GEODIFF_ERROR;
  }

  ChangesetReader reader;
  if ( !reader.open( changeset ) )
  {
    Logger::instance().error( "Could not open changeset: " + std::string( changeset ) );
    return GEODIFF_ERROR;
  }

  std::string res;
  try
  {
    if ( onlySummary )
      res = changesetToJSONSummary( reader );
    else
      res = changesetToJSON( reader );
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
  flushString( jsonfile, res );
  return GEODIFF_SUCCESS;
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

  ChangesetReader reader;
  if ( !reader.open( changeset ) )
  {
    Logger::instance().error( "Could not open changeset: " + std::string( changeset ) );
    return GEODIFF_ERROR;
  }

  ChangesetWriter writer;
  if ( !writer.open( changeset_inv ) )
  {
    Logger::instance().error( "Could not open file for writing: " + std::string( changeset_inv ) );
    return GEODIFF_ERROR;
  }

  try
  {
    invertChangeset( reader, writer );
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}

int GEODIFF_rebase( const char *base,
                    const char *modified_their,
                    const char *modified,
                    const char *conflictfile )
{
  if ( !base || !modified || !modified || !conflictfile )
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


int GEODIFF_makeCopy( const char *driverSrcName, const char *driverSrcExtraInfo, const char *src,
                      const char *driverDstName, const char *driverDstExtraInfo, const char *dst )
{
  if ( !driverSrcName || !src || !driverDstName || !dst )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_makeCopy" );
    return GEODIFF_ERROR;
  }

  std::unique_ptr<Driver> driverSrc( Driver::createDriver( std::string( driverSrcName ) ) );
  if ( !driverSrc )
  {
    Logger::instance().error( "Cannot create driver " + std::string( driverSrcName ) );
    return GEODIFF_ERROR;
  }

  std::unique_ptr<Driver> driverDst( Driver::createDriver( std::string( driverDstName ) ) );
  if ( !driverDst )
  {
    Logger::instance().error( "Cannot create driver " + std::string( driverDstName ) );
    return GEODIFF_ERROR;
  }

  TmpFile tmpFileChangeset( tmpdir() + "geodiff_changeset" + std::to_string( rand() ) );

  try
  {
    // open source
    std::map<std::string, std::string> connSrc;
    connSrc["base"] = std::string( src );
    if ( driverSrcExtraInfo )
      connSrc["conninfo"] = std::string( driverSrcExtraInfo );
    driverSrc->open( connSrc );

    // get source tables
    std::vector<TableSchema> tables;
    std::vector<std::string> tableNames = driverSrc->listTables();
    for ( const std::string &tableName : tableNames )
    {
      TableSchema tbl = driverSrc->tableSchema( tableName );
      tableSchemaConvert( driverSrcName, driverDstName, tbl );
      tables.push_back( tbl );
    }

    // get source data
    {
      ChangesetWriter writer;
      writer.open( tmpFileChangeset.c_path() );
      driverSrc->dumpData( writer );
    }

    // create destination
    std::map<std::string, std::string> connDst;
    connDst["base"] = dst;
    if ( driverDstExtraInfo )
      connDst["conninfo"] = std::string( driverDstExtraInfo );
    driverDst->create( connDst, true );

    // create tables in destination
    driverDst->createTables( tables );

    // insert data to destination
    {
      ChangesetReader reader;
      reader.open( tmpFileChangeset.c_path() );
      driverDst->applyChangeset( reader );
    }
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }

  // TODO: add spatial index to tables with geometry columns?

  return GEODIFF_SUCCESS;
}
