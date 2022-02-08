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

#include "sqliteutils.h"

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

// use scripts/update_version.py to update the version here and in other places at once
const char *GEODIFF_version()
{
  return "1.0.6";
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


int GEODIFF_createChangesetDr( const char *driverSrcName, const char *driverSrcExtraInfo, const char *src,
                               const char *driverDstName, const char *driverDstExtraInfo, const char *dst,
                               const char *changeset )
{
  if ( !driverSrcName || !driverSrcExtraInfo || !driverDstName || !driverDstExtraInfo || !src || !dst || !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_createChangesetAcrossDrivers" );
    return GEODIFF_ERROR;
  }

  if ( strcmp( driverSrcName, driverDstName ) == 0 )
  {
    return GEODIFF_createChangesetEx( driverSrcName, driverSrcExtraInfo, src, dst, changeset );
  }

  // copy both sources to geopackage and create changeset
  TmpFile tmpSrcGpkg;
  TmpFile tmpDstGpkg;

  if ( strcmp( driverSrcName, Driver::SQLITEDRIVERNAME.c_str() ) != 0 )
  {
    tmpSrcGpkg.setPath( tmpdir() + "_gpkg-" + randomString( 6 ) );
    if ( GEODIFF_makeCopy( driverSrcName, driverSrcExtraInfo, src, Driver::SQLITEDRIVERNAME.c_str(), "", tmpSrcGpkg.c_path() ) != GEODIFF_SUCCESS )
    {
      Logger::instance().error( "Failed to create a copy of base source for driver " + std::string( driverSrcName ) );
      return GEODIFF_ERROR;
    }
  }

  if ( strcmp( driverDstName, Driver::SQLITEDRIVERNAME.c_str() ) != 0 )
  {
    tmpDstGpkg.setPath( tmpdir() + "_gpkg-" + randomString( 6 ) );
    if ( GEODIFF_makeCopy( driverDstName, driverDstExtraInfo, dst, Driver::SQLITEDRIVERNAME.c_str(), "", tmpDstGpkg.c_path() ) != GEODIFF_SUCCESS )
    {
      Logger::instance().error( "Failed to create a copy of modified source for driver " + std::string( driverDstName ) );
      return GEODIFF_ERROR;
    }
  }

  return GEODIFF_createChangesetEx(
           Driver::SQLITEDRIVERNAME.c_str(),
           "",
           tmpSrcGpkg.path().empty() ? src : tmpSrcGpkg.c_path(),
           tmpDstGpkg.path().empty() ? dst : tmpDstGpkg.c_path(),
           changeset );
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
    // first verify if we are able to do rebase on this database schema at all
    {
      std::map<std::string, std::string> conn;
      conn["base"] = std::string( modified );
      std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
      if ( !driver )
        throw GeoDiffException( "Unable to use driver: sqlite" );
      driver->open( conn );

      driver->checkCompatibleForRebase();  // will throw GeoDiffException in case of problems
    }

    TmpFile changeset_BASE_MODIFIED( std::string( changeset ) + "_BASE_MODIFIED" );
    int rc = GEODIFF_createChangeset( base, modified, changeset_BASE_MODIFIED.c_path() );
    if ( rc != GEODIFF_SUCCESS )
      return rc;

    return GEODIFF_createRebasedChangesetEx( "sqlite", "", base, changeset_BASE_MODIFIED.c_path(), changeset_their, changeset, conflictfile );
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }
}

int GEODIFF_createRebasedChangesetEx( const char *driverName,
                                      const char *driverExtraInfo,
                                      const char *base,
                                      const char *base2modified,
                                      const char *base2their,
                                      const char *rebased,
                                      const char *conflictfile )
{
  if ( !driverName || !base || !base2modified || !base2their || !rebased || !conflictfile )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_createRebasedChangesetEx" );
    return GEODIFF_ERROR;
  }

  // TODO: use driverName + driverExtraInfo + base when creating rebased
  // changeset (e.g. to check whether a newly created ID is actually free)

  // TODO: call checkCompatibleForRebase()

  try
  {
    std::vector<ConflictFeature> conflicts;
    int rc = rebase( base2their, rebased, base2modified, conflicts );
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
  if ( !changeset )
  {
    Logger::instance().error( "Not provided changeset file to listChangeset" );
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

  if ( !jsonfile )
  {
    // print to terminal
    std::cout << res << std::endl;
  }
  else
  {
    flushString( jsonfile, res );
  }

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
    Logger::instance().error( "Missing input files in GEODIFF_invertChangeset: " + std::string( changeset ) );
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


int GEODIFF_concatChanges( int inputChangesetsCount, const char **inputChangesets, const char *outputChangeset )
{
  if ( inputChangesetsCount < 2 )
  {
    Logger::instance().error( "Need at least two input changesets in GEODIFF_concatChanges" );
    return GEODIFF_ERROR;
  }

  if ( !inputChangesets || !outputChangeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_concatChanges" );
    return GEODIFF_ERROR;
  }

  std::vector<std::string> inputFiles;
  for ( int i = 0; i < inputChangesetsCount; ++i )
  {
    std::string filename = inputChangesets[i];
    if ( !fileexists( filename ) )
    {
      Logger::instance().error( "Input file in GEODIFF_concatChanges does not exist: " + filename );
      return GEODIFF_ERROR;
    }
    inputFiles.push_back( filename );
  }

  try
  {
    concatChangesets( inputFiles, outputChangeset );
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
  if ( !base || !modified_their || !modified || !conflictfile )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_rebase" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( base ) )
  {
    Logger::instance().error( std::string( "Missing 'base' file in GEODIFF_rebase: " ) + base );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( modified_their ) )
  {
    Logger::instance().error( std::string( "Missing 'modified_their' file in GEODIFF_rebase: " ) + modified_their );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( modified ) )
  {
    Logger::instance().error( std::string( "Missing 'modified' file in GEODIFF_rebase: " ) + modified );
    return GEODIFF_ERROR;
  }

  std::string root = std::string( modified );

  TmpFile base2theirs( root + "_base2theirs.bin" );
  if ( GEODIFF_createChangeset( base, modified_their, base2theirs.c_path() ) != GEODIFF_SUCCESS )
  {
    Logger::instance().error( "Unable to perform GEODIFF_createChangeset base2theirs" );
    return GEODIFF_ERROR;
  }

  return GEODIFF_rebaseEx( "sqlite", "", base, modified, base2theirs.c_path(), conflictfile );
}


int GEODIFF_rebaseEx( const char *driverName,
                      const char *driverExtraInfo,
                      const char *base,
                      const char *modified,
                      const char *base2their,
                      const char *conflictfile )
{
  if ( !base || !modified || !modified || !conflictfile )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_rebase" );
    return GEODIFF_ERROR;
  }

  try
  {
    std::string root = tmpdir() + "geodiff_" + randomString( 6 );

    // situation 1: base2theirs is null, so we do not need rebase. modified is already fine
    if ( !GEODIFF_hasChanges( base2their ) )
    {
      return GEODIFF_SUCCESS;
    }

    TmpFile base2modified( root + "_base2modified.bin" );
    if ( GEODIFF_createChangesetEx( driverName, driverExtraInfo, base, modified, base2modified.c_path() ) != GEODIFF_SUCCESS )
    {
      Logger::instance().error( "Unable to perform GEODIFF_createChangeset base2modified" );
      return GEODIFF_ERROR;
    }

    // situation 2: we do not have changes (modified == base), so result is modified_theirs
    if ( !GEODIFF_hasChanges( base2modified.c_path() ) )
    {
      if ( GEODIFF_applyChangesetEx( driverName, driverExtraInfo, modified, base2their ) != GEODIFF_SUCCESS )
      {
        Logger::instance().error( "Unable to perform GEODIFF_applyChangeset base2theirs" );
        return GEODIFF_ERROR;
      }

      return GEODIFF_SUCCESS;
    }

    // situation 3: we have changes both in ours and theirs

    // 3A) Create all changesets
    TmpFile theirs2final( root + "_theirs2final.bin" );
    if ( GEODIFF_createRebasedChangesetEx( driverName, driverExtraInfo, base,
                                           base2modified.c_path(), base2their,
                                           theirs2final.c_path(), conflictfile ) != GEODIFF_SUCCESS )
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
    std::vector<std::string> concatInput;
    concatInput.push_back( modified2base.path() );
    concatInput.push_back( base2their );
    concatInput.push_back( theirs2final.path() );
    concatChangesets( concatInput, modified2final.path() );  // throws GeoDiffException exception on error

    // 3C) apply at once
    if ( GEODIFF_applyChangesetEx( driverName, driverExtraInfo, modified, modified2final.c_path() ) != GEODIFF_SUCCESS )
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
  if ( !driverSrcName || !driverSrcExtraInfo || !driverDstName || !driverDstExtraInfo || !src || !dst )
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
      tableSchemaConvert( driverDstName, tbl );
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

int GEODIFF_makeCopySqlite( const char *src, const char *dst )
{
  if ( !src || !dst )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_makeCopySqlite" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( src ) )
  {
    Logger::instance().error( "MakeCopySqlite: Source database does not exist: " + std::string( src ) );
    return GEODIFF_ERROR;
  }

  // If the destination file already exists, let's replace it. This is for convenience: if the file exists
  // and it is SQLite database, the backup API would overwrite it, but if the file would not be a valid
  // SQLite database, it would fail to open. With this check+remove we make sure that any existing file
  // gets overwritten, regardless of its original content (making the API easier to use for the caller).
  if ( fileexists( dst ) )
  {
    if ( fileremove( dst ) )
    {
      Logger::instance().warn( "MakeCopySqlite: Removed existing destination database: " + std::string( dst ) );
    }
    else
    {
      Logger::instance().error( "MakeCopySqlite: Failed to remove existing destination database: " + std::string( dst ) );
    }
  }

  Sqlite3Db dbFrom, dbTo;
  try
  {
    dbFrom.open( src );
  }
  catch ( GeoDiffException e )
  {
    Logger::instance().error( "MakeCopySqlite: Unable to open source database: " + std::string( src ) + "\n" + e.what() );
    return GEODIFF_ERROR;
  }

  try
  {
    dbTo.create( dst );
  }
  catch ( GeoDiffException e )
  {
    Logger::instance().error( "MakeCopySqlite: Unable to open destination database: " + std::string( dst ) + "\n" + e.what() );
    return GEODIFF_ERROR;
  }

  // Set up the backup procedure to copy from the "main" database of
  // connection pFrom to the main database of connection pTo.
  // If something goes wrong, pBackup will be set to NULL and an error
  // code and message left in connection pTo.

  // If the backup object is successfully created, call backup_step()
  // to copy data from pFrom to pTo. Then call backup_finish()
  // to release resources associated with the pBackup object.  If an
  // error occurred, then an error code and message will be left in
  // connection pTo. If no error occurred, then the error code belonging
  // to pTo is set to SQLITE_OK.

  sqlite3_backup *pBackup = sqlite3_backup_init( dbTo.get(), "main", dbFrom.get(), "main" );
  if ( pBackup )
  {
    sqlite3_backup_step( pBackup, -1 );
    sqlite3_backup_finish( pBackup );
  }

  std::string errorMsg;
  if ( sqlite3_errcode( dbTo.get() ) )
    errorMsg = sqlite3_errmsg( dbTo.get() );

  if ( !errorMsg.empty() )
  {
    Logger::instance().error( "MakeCopySqlite: backup failed: " + errorMsg );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


int GEODIFF_dumpData( const char *driverName, const char *driverExtraInfo, const char *src, const char *changeset )
{
  if ( !driverName || !src || !changeset )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_dumpData" );
    return GEODIFF_ERROR;
  }

  std::unique_ptr<Driver> driver( Driver::createDriver( std::string( driverName ) ) );
  if ( !driver )
  {
    Logger::instance().error( "Cannot create driver " + std::string( driverName ) );
    return GEODIFF_ERROR;
  }

  try
  {
    // open source
    std::map<std::string, std::string> conn;
    conn["base"] = std::string( src );
    if ( driverExtraInfo )
      conn["conninfo"] = std::string( driverExtraInfo );
    driver->open( conn );

    // get source data
    ChangesetWriter writer;
    writer.open( changeset );
    driver->dumpData( writer );
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


int GEODIFF_schema( const char *driverName, const char *driverExtraInfo, const char *src, const char *json )
{
  if ( !driverName || !src || !json )
  {
    Logger::instance().error( "NULL arguments to GEODIFF_schema" );
    return GEODIFF_ERROR;
  }

  std::unique_ptr<Driver> driver( Driver::createDriver( std::string( driverName ) ) );
  if ( !driver )
  {
    Logger::instance().error( "Cannot create driver " + std::string( driverName ) );
    return GEODIFF_ERROR;
  }

  try
  {
    // open source
    std::map<std::string, std::string> conn;
    conn["base"] = std::string( src );
    if ( driverExtraInfo )
      conn["conninfo"] = std::string( driverExtraInfo );
    driver->open( conn );

    // prepare JSON
    std::vector<std::string> tablesData;
    for ( const std::string &tableName : driver->listTables() )
    {
      const TableSchema tbl = driver->tableSchema( tableName );

      std::vector<std::string> columnsJson;
      for ( const TableColumnInfo &column : tbl.columns )
      {
        std::vector<std::string> columnData;
        columnData.push_back( "\"name\": \"" + jsonQuoted( column.name ) + "\"" );
        columnData.push_back( "\"type\": \"" + TableColumnType::baseTypeToString( column.type.baseType ) + "\"" );
        columnData.push_back( "\"type_db\": \"" + jsonQuoted( column.type.dbType ) + "\"" );
        if ( column.isPrimaryKey )
          columnData.push_back( "\"primary_key\": true" );
        if ( column.isNotNull )
          columnData.push_back( "\"not_null\": true" );
        if ( column.isAutoIncrement )
          columnData.push_back( "\"auto_increment\": true" );
        if ( column.isGeometry )
        {
          std::vector<std::string> geometryData;
          geometryData.push_back( "\"type\": \"" + jsonQuoted( column.geomType ) + "\"" );
          geometryData.push_back( "\"srs_id\": \"" + std::to_string( column.geomSrsId ) + "\"" );
          if ( column.geomHasZ )
            geometryData.push_back( "\"has_z\": true" );
          if ( column.geomHasM )
            geometryData.push_back( "\"has_m\": true" );

          std::string geometryJson;
          geometryJson += "\"geometry\": {\n               ";
          geometryJson += join( geometryData.begin(), geometryData.end(), ",\n               " );
          geometryJson += "\n            }";
          columnData.push_back( geometryJson );
        }

        std::string columnJson;
        columnJson += "         {\n";
        columnJson += "            ";
        columnJson += join( columnData.begin(), columnData.end(), ",\n            " );
        columnJson += "\n         }";

        columnsJson.push_back( columnJson );
      }

      std::vector<std::string> tableData;
      tableData.push_back( "\"table\": \"" + jsonQuoted( tableName ) + "\"" );
      tableData.push_back( "\"columns\": [\n" + join( columnsJson.begin(), columnsJson.end(), ",\n" ) + "\n         ]" );
      if ( tbl.crs.srsId != 0 )
      {
        std::vector<std::string> crsData;
        crsData.push_back( "\"srs_id\": " + std::to_string( tbl.crs.srsId ) );
        crsData.push_back( "\"auth_name\": \"" + tbl.crs.authName + "\"" );
        crsData.push_back( "\"auth_code\": " + std::to_string( tbl.crs.authCode ) );
        crsData.push_back( "\"wkt\": \"" + jsonQuoted( tbl.crs.wkt ) + "\"" );

        std::string crsJson;
        crsJson += "\"crs\": {\n            ";
        crsJson += join( crsData.begin(), crsData.end(), ",\n            " );
        crsJson += "\n         }";
        tableData.push_back( crsJson );
      }
      std::string tableJson;
      tableJson += "      {\n         ";
      tableJson += join( tableData.begin(), tableData.end(), ",\n         " );
      tableJson += "\n      }";

      tablesData.push_back( tableJson );
    }
    std::string res = "{\n   \"geodiff_schema\": [\n";
    res += join( tablesData.begin(), tablesData.end(), ",\n   " );
    res += "\n   ]\n";
    res += "}\n";

    // write file content
    flushString( json, res );
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


GEODIFF_ChangesetReaderH GEODIFF_readChangeset( const char *changeset )
{
  if ( !changeset )
  {
    Logger::instance().error( "NULL changeset argument to GEODIFF_readChangeset" );
    return nullptr;
  }

  ChangesetReader *reader = new ChangesetReader;
  if ( !reader->open( changeset ) )
  {
    delete reader;
    return nullptr;
  }
  return reader;
}

GEODIFF_ChangesetEntryH GEODIFF_CR_nextEntry( GEODIFF_ChangesetReaderH readerHandle, bool *ok )
{
  *ok = true;
  ChangesetReader *reader = static_cast<ChangesetReader *>( readerHandle );
  std::unique_ptr<ChangesetEntry> entry( new ChangesetEntry );
  try
  {
    if ( !reader->nextEntry( *entry ) )
    {
      // we have reached the end of file
      return nullptr;
    }
  }
  catch ( GeoDiffException exc )
  {
    Logger::instance().error( exc );
    *ok = false;
    return nullptr;
  }
  return entry.release();
}

void GEODIFF_CR_destroy( GEODIFF_ChangesetReaderH readerHandle )
{
  delete static_cast<ChangesetReader *>( readerHandle );
}

int GEODIFF_CE_operation( GEODIFF_ChangesetEntryH entryHandle )
{
  return static_cast<ChangesetEntry *>( entryHandle )->op;
}

GEODIFF_ChangesetTableH GEODIFF_CE_table( GEODIFF_ChangesetEntryH entryHandle )
{
  ChangesetTable *table = static_cast<ChangesetEntry *>( entryHandle )->table;
  return table;
}

int GEODIFF_CE_countValues( GEODIFF_ChangesetEntryH entryHandle )
{
  ChangesetEntry *entry = static_cast<ChangesetEntry *>( entryHandle );
  return entry->op == ChangesetEntry::OpDelete ? entry->oldValues.size() : entry->newValues.size();
}

GEODIFF_ValueH GEODIFF_CE_oldValue( GEODIFF_ChangesetEntryH entryHandle, int i )
{
  return new Value( static_cast<ChangesetEntry *>( entryHandle )->oldValues[i] );
}

GEODIFF_ValueH GEODIFF_CE_newValue( GEODIFF_ChangesetEntryH entryHandle, int i )
{
  return new Value( static_cast<ChangesetEntry *>( entryHandle )->newValues[i] );
}

void GEODIFF_CE_destroy( GEODIFF_ChangesetEntryH entryHandle )
{
  delete static_cast<ChangesetEntry *>( entryHandle );
}

int GEODIFF_V_type( GEODIFF_ValueH valueHandle )
{
  return static_cast<Value *>( valueHandle )->type();
}

int64_t GEODIFF_V_getInt( GEODIFF_ValueH valueHandle )
{
  return static_cast<Value *>( valueHandle )->getInt();
}

double GEODIFF_V_getDouble( GEODIFF_ValueH valueHandle )
{
  return static_cast<Value *>( valueHandle )->getDouble();
}

void GEODIFF_V_destroy( GEODIFF_ValueH valueHandle )
{
  delete static_cast<Value *>( valueHandle );
}

int GEODIFF_V_getDataSize( GEODIFF_ValueH valueHandle )
{
  return static_cast<Value *>( valueHandle )->getString().size();
}

void GEODIFF_V_getData( GEODIFF_ValueH valueHandle, char *data )
{
  const std::string &str = static_cast<Value *>( valueHandle )->getString();
  memcpy( data, str.data(), str.size() );
}

const char *GEODIFF_CT_name( GEODIFF_ChangesetTableH tableHandle )
{
  return static_cast<ChangesetTable *>( tableHandle )->name.data();
}

int GEODIFF_CT_columnCount( GEODIFF_ChangesetTableH tableHandle )
{
  return static_cast<ChangesetTable *>( tableHandle )->columnCount();
}

bool GEODIFF_CT_columnIsPkey( GEODIFF_ChangesetTableH tableHandle, int i )
{
  return static_cast<ChangesetTable *>( tableHandle )->primaryKeys.at( i );
}
