/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiffutils.hpp"
#include "geodiffrebase.hpp"
#include "geodifflogger.hpp"
#include "geodiffcontext.hpp"

#include "driver.h"
#include "changesetreader.h"
#include "changesetutils.h"
#include "changesetwriter.h"

#include "sqliteutils.h"

#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <cstring>
#include <string.h>

#include <sqlite3.h>

#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>

#include "json.hpp"

// use scripts/update_version.py to update the version here and in other places at once
const char *GEODIFF_version()
{
  return "2.0.1";
}

int GEODIFF_driverCount( GEODIFF_ContextH /*contextHandle*/ )
{
  const std::vector<std::string> drivers = Driver::drivers();
  return ( int ) drivers.size();
}

int GEODIFF_driverNameFromIndex( GEODIFF_ContextH contextHandle, int index, char *driverName )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  const std::vector<std::string> drivers = Driver::drivers();

  if ( ( size_t ) index >= drivers.size() )
  {
    context->logger().error( "Index out of range in GEODIFF_driverNameFromIndex" );
    return GEODIFF_ERROR;
  }

  const std::string name = drivers[index];
  const char *cname = name.c_str();
  const size_t len = name.size() + 1;
  assert( len < 256 );
  memcpy( driverName, cname, len );
  return GEODIFF_SUCCESS;
}

bool GEODIFF_driverIsRegistered( GEODIFF_ContextH contextHandle, const char *driverName )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !driverName )
  {
    context->logger().error( "NULL arguments to GEODIFF_driverIsRegistered" );
    return GEODIFF_ERROR;
  }

  return Driver::driverIsRegistered( std::string( driverName ) );
}

GEODIFF_ContextH GEODIFF_createContext()
{
  Context *context = new Context();

  sqlite3_initialize();

  return ( GEODIFF_ContextH ) context;
}

int GEODIFF_CX_setLoggerCallback( GEODIFF_ContextH contextHandle, GEODIFF_LoggerCallback loggerCallback )
{

  Context *context = static_cast<Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }
  context->logger().setCallback( loggerCallback );
  return GEODIFF_SUCCESS;
}

int GEODIFF_CX_setMaximumLoggerLevel( GEODIFF_ContextH contextHandle,
                                      GEODIFF_LoggerLevel maxLogLevel )
{
  Context *context = static_cast<Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }
  context->logger().setMaxLogLevel( maxLogLevel );
  return GEODIFF_SUCCESS;
}

int GEODIFF_CX_setTablesToSkip( GEODIFF_ContextH contextHandle, int tablesCount, const char **tablesToSkip )
{
  Context *context = static_cast<Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( tablesCount > 0 && !tablesToSkip )
  {
    context->logger().error( "NULL arguments to GEODIFF_CX_setTablesToSkip" );
    return GEODIFF_ERROR;
  }

  std::vector<std::string> tables;
  for ( int i = 0; i < tablesCount; ++i )
  {
    std::string tableName = tablesToSkip[i];
    tables.push_back( tableName );
  }

  context->setTablesToSkip( tables );
  return GEODIFF_SUCCESS;
}

void GEODIFF_CX_destroy( GEODIFF_ContextH contextHandle )
{
  Context *context = static_cast<Context *>( contextHandle );
  if ( context )
  {
    delete context;
    context = nullptr;
  }
}

int GEODIFF_createChangeset( GEODIFF_ContextH contextHandle, const char *base, const char *modified, const char *changeset )
{
  return GEODIFF_createChangesetEx( contextHandle, "sqlite", nullptr, base, modified, changeset );
}

int GEODIFF_applyChangeset( GEODIFF_ContextH contextHandle, const char *base, const char *changeset )
{
  return GEODIFF_applyChangesetEx( contextHandle, "sqlite", nullptr, base, changeset );
}


int GEODIFF_createChangesetEx( GEODIFF_ContextH contextHandle, const char *driverName, const char *driverExtraInfo,
                               const char *base, const char *modified,
                               const char *changeset )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !driverName || !base || !modified || !changeset )
  {
    context->logger().error( "NULL arguments to GEODIFF_createChangesetEx" );
    return GEODIFF_ERROR;
  }

  try
  {
    std::map<std::string, std::string> conn;
    conn["base"] = std::string( base );
    conn["modified"] = std::string( modified );
    if ( driverExtraInfo )
      conn["conninfo"] = std::string( driverExtraInfo );
    std::unique_ptr<Driver> driver( Driver::createDriver( context, std::string( driverName ) ) );
    if ( !driver )
      throw GeoDiffException( "Unable to use driver: " + std::string( driverName ) );
    driver->open( conn );

    ChangesetWriter writer;
    writer.open( changeset );
    driver->createChangeset( writer );
  }
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


int GEODIFF_createChangesetDr( GEODIFF_ContextH contextHandle, const char *driverSrcName, const char *driverSrcExtraInfo, const char *src,
                               const char *driverDstName, const char *driverDstExtraInfo, const char *dst,
                               const char *changeset )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !driverSrcName || !driverSrcExtraInfo || !driverDstName || !driverDstExtraInfo || !src || !dst || !changeset )
  {
    context->logger().error( "NULL arguments to GEODIFF_createChangesetAcrossDrivers" );
    return GEODIFF_ERROR;
  }

  if ( strcmp( driverSrcName, driverDstName ) == 0 )
  {
    return GEODIFF_createChangesetEx( contextHandle, driverSrcName, driverSrcExtraInfo, src, dst, changeset );
  }

  // copy both sources to geopackage and create changeset
  TmpFile tmpSrcGpkg;
  TmpFile tmpDstGpkg;

  if ( strcmp( driverSrcName, Driver::SQLITEDRIVERNAME.c_str() ) != 0 )
  {
    tmpSrcGpkg.setPath( tmpdir( ) + "_gpkg-" + randomString( 6 ) );
    if ( GEODIFF_makeCopy( contextHandle, driverSrcName, driverSrcExtraInfo, src, Driver::SQLITEDRIVERNAME.c_str(), "", tmpSrcGpkg.c_path() ) != GEODIFF_SUCCESS )
    {
      context->logger().error( "Failed to create a copy of base source for driver " + std::string( driverSrcName ) );
      return GEODIFF_ERROR;
    }
  }

  if ( strcmp( driverDstName, Driver::SQLITEDRIVERNAME.c_str() ) != 0 )
  {
    tmpDstGpkg.setPath( tmpdir() + "_gpkg-" + randomString( 6 ) );
    if ( GEODIFF_makeCopy( contextHandle, driverDstName, driverDstExtraInfo, dst, Driver::SQLITEDRIVERNAME.c_str(), "", tmpDstGpkg.c_path() ) != GEODIFF_SUCCESS )
    {
      context->logger().error( "Failed to create a copy of modified source for driver " + std::string( driverDstName ) );
      return GEODIFF_ERROR;
    }
  }

  return GEODIFF_createChangesetEx(
           contextHandle,
           Driver::SQLITEDRIVERNAME.c_str(),
           "",
           tmpSrcGpkg.path().empty() ? src : tmpSrcGpkg.c_path(),
           tmpDstGpkg.path().empty() ? dst : tmpDstGpkg.c_path(),
           changeset );
}


int GEODIFF_applyChangesetEx(
  GEODIFF_ContextH contextHandle,
  const char *driverName,
  const char *driverExtraInfo,
  const char *base,
  const char *changeset )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !driverName || !base || !changeset )
  {
    context->logger().error( "NULL arguments to GEODIFF_applyChangesetEx" );
    return GEODIFF_ERROR;
  }

  try
  {
    std::map<std::string, std::string> conn;
    conn["base"] = std::string( base );
    if ( driverExtraInfo )
      conn["conninfo"] = std::string( driverExtraInfo );
    std::unique_ptr<Driver> driver( Driver::createDriver( context,  std::string( driverName ) ) );
    if ( !driver )
      throw GeoDiffException( "Unable to use driver: " + std::string( driverName ) );
    driver->open( conn );

    ChangesetReader reader;
    if ( !reader.open( changeset ) )
      throw GeoDiffException( "Unable to open changeset file for reading: " + std::string( changeset ) );
    if ( reader.isEmpty() )
    {
      context->logger().debug( "--- no changes ---" );
      return GEODIFF_SUCCESS;
    }

    driver->applyChangeset( reader );
  }
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


int GEODIFF_createRebasedChangeset(
  GEODIFF_ContextH contextHandle, const char *base,
  const char *modified,
  const char *changeset_their,
  const char *changeset,
  const char *conflictfile )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !conflictfile )
  {
    context->logger().error( "NULL arguments to GEODIFF_createRebasedChangeset" );
    return GEODIFF_ERROR;
  }
  fileremove( conflictfile );

  try
  {
    // first verify if we are able to do rebase on this database schema at all
    {
      std::map<std::string, std::string> conn;
      conn["base"] = std::string( modified );
      std::unique_ptr<Driver> driver( Driver::createDriver( context, "sqlite" ) );
      if ( !driver )
        throw GeoDiffException( "Unable to use driver: sqlite" );
      driver->open( conn );

      driver->checkCompatibleForRebase();  // will throw GeoDiffException in case of problems
    }

    TmpFile changeset_BASE_MODIFIED( std::string( changeset ) + "_BASE_MODIFIED" );
    int rc = GEODIFF_createChangeset( contextHandle, base, modified, changeset_BASE_MODIFIED.c_path() );
    if ( rc != GEODIFF_SUCCESS )
      return rc;

    return GEODIFF_createRebasedChangesetEx( contextHandle, "sqlite", "", base, changeset_BASE_MODIFIED.c_path(), changeset_their, changeset, conflictfile );
  }
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }
}

int GEODIFF_createRebasedChangesetEx(
  GEODIFF_ContextH contextHandle,
  const char *driverName,
  const char * /* driverExtraInfo */,
  const char *base,
  const char *base2modified,
  const char *base2their,
  const char *rebased,
  const char *conflictfile )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !driverName || !base || !base2modified || !base2their || !rebased || !conflictfile )
  {
    context->logger().error( "NULL arguments to GEODIFF_createRebasedChangesetEx" );
    return GEODIFF_ERROR;
  }

  // TODO: use driverName + driverExtraInfo + base when creating rebased
  // changeset (e.g. to check whether a newly created ID is actually free)

  // TODO: call checkCompatibleForRebase()

  try
  {
    std::vector<ConflictFeature> conflicts;
    rebase( context, base2their, rebased, base2modified, conflicts );

    // output conflicts
    if ( conflicts.empty() )
    {
      context->logger().debug( "No conflicts present" );
    }
    else
    {
      nlohmann::json res = conflictsToJSON( conflicts );
      flushString( conflictfile, res.dump( 2 ) );
    }

    return GEODIFF_SUCCESS;
  }
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }
}


int GEODIFF_hasChanges(
  GEODIFF_ContextH contextHandle,
  const char *changeset )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !changeset )
  {
    context->logger().error( "NULL arguments to GEODIFF_hasChanges" );
    return -1;
  }

  ChangesetReader reader;
  if ( !reader.open( changeset ) )
  {
    context->logger().error( "Could not open changeset: " + std::string( changeset ) );
    return -1;
  }

  return !reader.isEmpty();
}

int GEODIFF_changesCount(
  GEODIFF_ContextH contextHandle,
  const char *changeset )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !changeset )
  {
    context->logger().error( "NULL arguments to GEODIFF_changesCount" );
    return -1;
  }

  ChangesetReader reader;
  if ( !reader.open( changeset ) )
  {
    context->logger().error( "Could not open changeset: " + std::string( changeset ) );
    return -1;
  }

  int changesCount = 0;
  ChangesetEntry entry;
  while ( reader.nextEntry( entry ) )
    ++changesCount;

  return changesCount;
}

static int listChangesJSON( const Context *context, const char *changeset, const char *jsonfile, bool onlySummary )
{
  if ( !changeset )
  {
    context->logger().error( "Not provided changeset file to listChangeset" );
    return GEODIFF_ERROR;
  }

  ChangesetReader reader;
  if ( !reader.open( changeset ) )
  {
    context->logger().error( "Could not open changeset: " + std::string( changeset ) );
    return GEODIFF_ERROR;
  }

  nlohmann::json res;
  try
  {
    if ( onlySummary )
      res = changesetToJSONSummary( reader );
    else
      res = changesetToJSON( reader );
  }
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }

  if ( !jsonfile )
  {
    // print to terminal
    std::cout << res.dump( 2 ) << std::endl;
  }
  else
  {
    flushString( jsonfile, res.dump( 2 ) );
  }

  return GEODIFF_SUCCESS;
}

int GEODIFF_listChanges(
  GEODIFF_ContextH contextHandle,
  const char *changeset,
  const char *jsonfile )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }
  return listChangesJSON( context, changeset, jsonfile, false );
}

int GEODIFF_listChangesSummary( GEODIFF_ContextH contextHandle, const char *changeset, const char *jsonfile )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  return listChangesJSON( context, changeset, jsonfile, true );
}

int GEODIFF_invertChangeset( GEODIFF_ContextH contextHandle, const char *changeset, const char *changeset_inv )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !changeset )
  {
    context->logger().error( "NULL arguments to GEODIFF_invertChangeset" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( changeset ) )
  {
    context->logger().error( "Missing input files in GEODIFF_invertChangeset: " + std::string( changeset ) );
    return GEODIFF_ERROR;
  }

  try
  {

    ChangesetReader reader;
    if ( !reader.open( changeset ) )
    {
      context->logger().error( "Could not open changeset: " + std::string( changeset ) );
      return GEODIFF_ERROR;
    }

    ChangesetWriter writer;
    writer.open( changeset_inv );

    invertChangeset( reader, writer );
  }
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


int GEODIFF_concatChanges(
  GEODIFF_ContextH contextHandle,
  int inputChangesetsCount,
  const char **inputChangesets,
  const char *outputChangeset )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( inputChangesetsCount < 2 )
  {
    context->logger().error( "Need at least two input changesets in GEODIFF_concatChanges" );
    return GEODIFF_ERROR;
  }

  if ( !inputChangesets || !outputChangeset )
  {
    context->logger().error( "NULL arguments to GEODIFF_concatChanges" );
    return GEODIFF_ERROR;
  }

  std::vector<std::string> inputFiles;
  for ( int i = 0; i < inputChangesetsCount; ++i )
  {
    std::string filename = inputChangesets[i];
    if ( !fileexists( filename ) )
    {
      context->logger().error( "Input file in GEODIFF_concatChanges does not exist: " + filename );
      return GEODIFF_ERROR;
    }
    inputFiles.push_back( filename );
  }

  try
  {
    concatChangesets( context, inputFiles, outputChangeset );
  }
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


int GEODIFF_rebase(
  GEODIFF_ContextH contextHandle,
  const char *base,
  const char *modified_their,
  const char *modified,
  const char *conflictfile )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !base || !modified_their || !modified || !conflictfile )
  {
    context->logger().error( "NULL arguments to GEODIFF_rebase" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( base ) )
  {
    context->logger().error( std::string( "Missing 'base' file in GEODIFF_rebase: " ) + base );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( modified_their ) )
  {
    context->logger().error( std::string( "Missing 'modified_their' file in GEODIFF_rebase: " ) + modified_their );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( modified ) )
  {
    context->logger().error( std::string( "Missing 'modified' file in GEODIFF_rebase: " ) + modified );
    return GEODIFF_ERROR;
  }

  std::string root = std::string( modified );

  TmpFile base2theirs( root + "_base2theirs.bin" );
  if ( GEODIFF_createChangeset( contextHandle, base, modified_their, base2theirs.c_path() ) != GEODIFF_SUCCESS )
  {
    context->logger().error( "Unable to perform GEODIFF_createChangeset base2theirs" );
    return GEODIFF_ERROR;
  }

  return GEODIFF_rebaseEx( contextHandle, "sqlite", "", base, modified, base2theirs.c_path(), conflictfile );
}


int GEODIFF_rebaseEx(
  GEODIFF_ContextH contextHandle,
  const char *driverName,
  const char *driverExtraInfo,
  const char *base,
  const char *modified,
  const char *base2their,
  const char *conflictfile )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !base || !modified || !base2their || !conflictfile )
  {
    context->logger().error( "NULL arguments to GEODIFF_rebase" );
    return GEODIFF_ERROR;
  }

  try
  {
    std::string root = tmpdir( ) + "geodiff_" + randomString( 6 );

    // situation 1: base2theirs is null, so we do not need rebase. modified is already fine
    if ( !GEODIFF_hasChanges( contextHandle, base2their ) )
    {
      return GEODIFF_SUCCESS;
    }

    TmpFile base2modified( root + "_base2modified.bin" );
    if ( GEODIFF_createChangesetEx( contextHandle, driverName, driverExtraInfo, base, modified, base2modified.c_path() ) != GEODIFF_SUCCESS )
    {
      context->logger().error( "Unable to perform GEODIFF_createChangeset base2modified" );
      return GEODIFF_ERROR;
    }

    // situation 2: we do not have changes (modified == base), so result is modified_theirs
    if ( !GEODIFF_hasChanges( contextHandle, base2modified.c_path() ) )
    {
      if ( GEODIFF_applyChangesetEx( contextHandle, driverName, driverExtraInfo, modified, base2their ) != GEODIFF_SUCCESS )
      {
        context->logger().error( "Unable to perform GEODIFF_applyChangeset base2theirs" );
        return GEODIFF_ERROR;
      }

      return GEODIFF_SUCCESS;
    }

    // situation 3: we have changes both in ours and theirs

    // 3A) Create all changesets
    TmpFile theirs2final( root + "_theirs2final.bin" );
    if ( GEODIFF_createRebasedChangesetEx( contextHandle,
                                           driverName, driverExtraInfo, base,
                                           base2modified.c_path(), base2their,
                                           theirs2final.c_path(), conflictfile ) != GEODIFF_SUCCESS )
    {
      context->logger().error( "Unable to perform GEODIFF_createChangeset theirs2final" );
      return GEODIFF_ERROR;
    }

    TmpFile modified2base( root + "_modified2base.bin" );
    if ( GEODIFF_invertChangeset( contextHandle, base2modified.c_path(), modified2base.c_path() ) != GEODIFF_SUCCESS )
    {
      context->logger().error( "Unable to perform GEODIFF_invertChangeset modified2base" );
      return GEODIFF_ERROR;
    }

    // 3B) concat to single changeset
    TmpFile modified2final( root + "_modified2final.bin" );
    std::vector<std::string> concatInput;
    concatInput.push_back( modified2base.path() );
    concatInput.push_back( base2their );
    concatInput.push_back( theirs2final.path() );
    concatChangesets( context, concatInput, modified2final.path() );  // throws GeoDiffException exception on error

    // 3C) apply at once
    if ( GEODIFF_applyChangesetEx( contextHandle, driverName, driverExtraInfo, modified, modified2final.c_path() ) != GEODIFF_SUCCESS )
    {
      context->logger().error( "Unable to perform GEODIFF_applyChangeset modified2final" );
      return GEODIFF_ERROR;
    }

    return GEODIFF_SUCCESS;
  }
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }
}


int GEODIFF_makeCopy( GEODIFF_ContextH contextHandle,
                      const char *driverSrcName,
                      const char *driverSrcExtraInfo,
                      const char *src,
                      const char *driverDstName,
                      const char *driverDstExtraInfo,
                      const char *dst )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !driverSrcName || !driverSrcExtraInfo || !driverDstName || !driverDstExtraInfo || !src || !dst )
  {
    context->logger().error( "NULL arguments to GEODIFF_makeCopy" );
    return GEODIFF_ERROR;
  }

  std::string srcDriverName( driverSrcName );
  std::string dstDriverName( driverDstName );
  std::unique_ptr<Driver> driverSrc( Driver::createDriver( context, srcDriverName ) );
  if ( !driverSrc )
  {
    context->logger().error( "Cannot create driver " + srcDriverName );
    return GEODIFF_ERROR;
  }

  std::unique_ptr<Driver> driverDst( Driver::createDriver( context, dstDriverName ) );
  if ( !driverDst )
  {
    context->logger().error( "Cannot create driver " + dstDriverName );
    return GEODIFF_ERROR;
  }

  TmpFile tmpFileChangeset( tmpdir( ) + "geodiff_changeset" + std::to_string( rand() ) );

  try
  {
    // open source
    std::map<std::string, std::string> connSrc;
    connSrc["base"] = std::string( src );
    connSrc["conninfo"] = std::string( driverSrcExtraInfo );
    driverSrc->open( connSrc );

    // get source tables
    std::vector<TableSchema> tables;
    std::vector<std::string> tableNames = driverSrc->listTables();

    if ( srcDriverName != dstDriverName )
    {
      for ( const std::string &tableName : tableNames )
      {
        TableSchema tbl = driverSrc->tableSchema( tableName );
        tableSchemaConvert( driverDstName, tbl );
        tables.push_back( tbl );
      }
    }
    else
    {
      for ( const std::string &tableName : tableNames )
      {
        TableSchema tbl = driverSrc->tableSchema( tableName );
        tables.push_back( tbl );
      }
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
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }

  // TODO: add spatial index to tables with geometry columns?

  return GEODIFF_SUCCESS;
}

int GEODIFF_makeCopySqlite( GEODIFF_ContextH contextHandle, const char *src, const char *dst )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !src || !dst )
  {
    context->logger().error( "NULL arguments to GEODIFF_makeCopySqlite" );
    return GEODIFF_ERROR;
  }

  if ( !fileexists( src ) )
  {
    context->logger().error( "MakeCopySqlite: Source database does not exist: " + std::string( src ) );
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
      context->logger().warn( "MakeCopySqlite: Removed existing destination database: " + std::string( dst ) );
    }
    else
    {
      context->logger().error( "MakeCopySqlite: Failed to remove existing destination database: " + std::string( dst ) );
    }
  }

  Sqlite3Db dbFrom, dbTo;
  try
  {
    dbFrom.open( src );
  }
  catch ( const  GeoDiffException &e )
  {
    context->logger().error( "MakeCopySqlite: Unable to open source database: " + std::string( src ) + "\n" + e.what() );
    return GEODIFF_ERROR;
  }

  try
  {
    dbTo.create( dst );
  }
  catch ( const  GeoDiffException &e )
  {
    context->logger().error( "MakeCopySqlite: Unable to open destination database: " + std::string( dst ) + "\n" + e.what() );
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
    context->logger().error( "MakeCopySqlite: backup failed: " + errorMsg );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


int GEODIFF_dumpData( GEODIFF_ContextH contextHandle, const char *driverName, const char *driverExtraInfo, const char *src, const char *changeset )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !driverName || !src || !changeset )
  {
    context->logger().error( "NULL arguments to GEODIFF_dumpData" );
    return GEODIFF_ERROR;
  }

  std::unique_ptr<Driver> driver( Driver::createDriver( context,  std::string( driverName ) ) );
  if ( !driver )
  {
    context->logger().error( "Cannot create driver " + std::string( driverName ) );
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
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


int GEODIFF_schema( GEODIFF_ContextH contextHandle, const char *driverName, const char *driverExtraInfo, const char *src, const char *json )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return GEODIFF_ERROR;
  }

  if ( !driverName || !src || !json )
  {
    context->logger().error( "NULL arguments to GEODIFF_schema" );
    return GEODIFF_ERROR;
  }

  std::unique_ptr<Driver> driver( Driver::createDriver( context,  std::string( driverName ) ) );
  if ( !driver )
  {
    context->logger().error( "Cannot create driver " + std::string( driverName ) );
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
    auto tablesData = nlohmann::json::array();
    for ( const std::string &tableName : driver->listTables() )
    {
      const TableSchema tbl = driver->tableSchema( tableName );

      auto columnsJson = nlohmann::json::array();
      for ( const TableColumnInfo &column : tbl.columns )
      {
        nlohmann::json columnData;
        columnData[ "name" ] = column.name;
        columnData[ "type" ] = TableColumnType::baseTypeToString( column.type.baseType );
        columnData[ "type_db" ] = column.type.dbType;
        if ( column.isPrimaryKey )
          columnData[ "primary_key" ] = true;
        if ( column.isNotNull )
          columnData[ "not_null" ] = true;
        if ( column.isAutoIncrement )
          columnData[ "auto_increment" ] = true;
        if ( column.isGeometry )
        {
          nlohmann::json geometryData;
          geometryData[ "type" ] = column.geomType;
          geometryData[ "srs_id" ] = std::to_string( column.geomSrsId );
          if ( column.geomHasZ )
            geometryData[ "has_z" ] = true;
          if ( column.geomHasM )
            geometryData[ "has_m" ] = true;

          columnData[ "geometry" ] = geometryData;
        }

        columnsJson.push_back( columnData );
      }

      nlohmann::json tableJson;
      tableJson[ "table"] = tableName;
      tableJson[ "columns" ] = columnsJson;
      if ( tbl.crs.srsId != 0 )
      {
        nlohmann::json crsJson;
        crsJson[ "srs_id" ] = tbl.crs.srsId;
        crsJson[ "auth_name" ] = tbl.crs.authName;
        crsJson[ "auth_code" ] = tbl.crs.authCode;
        crsJson[ "wkt" ] = tbl.crs.wkt;

        tableJson[ "crs" ] = crsJson;
      }
      tablesData.push_back( tableJson );
    }

    nlohmann::json res;
    res[ "geodiff_schema" ] = tablesData;

    // write file content
    flushString( json, res.dump( 2 ) );
  }
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    return GEODIFF_ERROR;
  }

  return GEODIFF_SUCCESS;
}


GEODIFF_ChangesetReaderH GEODIFF_readChangeset( GEODIFF_ContextH contextHandle, const char *changeset )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    return nullptr;
  }

  if ( !changeset )
  {
    context->logger().error( "NULL changeset argument to GEODIFF_readChangeset" );
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

GEODIFF_ChangesetEntryH GEODIFF_CR_nextEntry( GEODIFF_ContextH contextHandle, GEODIFF_ChangesetReaderH readerHandle, bool *ok )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context )
  {
    *ok = false;
    return nullptr;
  }

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
  catch ( const  GeoDiffException &exc )
  {
    context->logger().error( exc );
    *ok = false;
    return nullptr;
  }
  return entry.release();
}

void GEODIFF_CR_destroy( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ChangesetReaderH readerHandle )
{
  delete static_cast<ChangesetReader *>( readerHandle );
}

int GEODIFF_CE_operation( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ChangesetEntryH entryHandle )
{
  return static_cast<ChangesetEntry *>( entryHandle )->op;
}

GEODIFF_ChangesetTableH GEODIFF_CE_table( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ChangesetEntryH entryHandle )
{
  ChangesetTable *table = static_cast<ChangesetEntry *>( entryHandle )->table;
  return table;
}

int GEODIFF_CE_countValues( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ChangesetEntryH entryHandle )
{
  ChangesetEntry *entry = static_cast<ChangesetEntry *>( entryHandle );
  size_t ret = entry->op == ChangesetEntry::OpDelete ? entry->oldValues.size() : entry->newValues.size();
  return ( int ) ret;
}

GEODIFF_ValueH GEODIFF_CE_oldValue( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ChangesetEntryH entryHandle, int i )
{
  return new Value( static_cast<ChangesetEntry *>( entryHandle )->oldValues[i] );
}

GEODIFF_ValueH GEODIFF_CE_newValue( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ChangesetEntryH entryHandle, int i )
{
  return new Value( static_cast<ChangesetEntry *>( entryHandle )->newValues[i] );
}

void GEODIFF_CE_destroy( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ChangesetEntryH entryHandle )
{
  delete static_cast<ChangesetEntry *>( entryHandle );
}

int GEODIFF_V_type( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ValueH valueHandle )
{
  return static_cast<Value *>( valueHandle )->type();
}

int64_t GEODIFF_V_getInt( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ValueH valueHandle )
{
  return static_cast<Value *>( valueHandle )->getInt();
}

double GEODIFF_V_getDouble( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ValueH valueHandle )
{
  return static_cast<Value *>( valueHandle )->getDouble();
}

void GEODIFF_V_destroy( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ValueH valueHandle )
{
  delete static_cast<Value *>( valueHandle );
}

int GEODIFF_V_getDataSize( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ValueH valueHandle )
{
  size_t ret = static_cast<Value *>( valueHandle )->getString().size();
  return ( int ) ret;
}

void GEODIFF_V_getData( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ValueH valueHandle, char *data )
{
  const std::string &str = static_cast<Value *>( valueHandle )->getString();
  memcpy( data, str.data(), str.size() );
}

const char *GEODIFF_CT_name( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ChangesetTableH tableHandle )
{
  return static_cast<ChangesetTable *>( tableHandle )->name.data();
}

int GEODIFF_CT_columnCount( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ChangesetTableH tableHandle )
{
  size_t ret = static_cast<ChangesetTable *>( tableHandle )->columnCount();
  return ( int ) ret;
}

bool GEODIFF_CT_columnIsPkey( GEODIFF_ContextH /*contextHandle*/, GEODIFF_ChangesetTableH tableHandle, int i )
{
  return static_cast<ChangesetTable *>( tableHandle )->primaryKeys.at( i );
}

int GEODIFF_createWkbFromGpkgHeader( GEODIFF_ContextH contextHandle, const char *gpkgWkb, size_t gpkgLength, const char **wkb, size_t *wkbLength )
{
  const Context *context = static_cast<const Context *>( contextHandle );
  if ( !context || !gpkgWkb || !wkb || !wkbLength )
  {
    return GEODIFF_ERROR;
  }

  if ( gpkgLength == 0 )
  {
    return GEODIFF_ERROR;
  }

  std::string gpkgWkbStr( gpkgWkb, gpkgLength );
  int headerSize = parseGpkgbHeaderSize( gpkgWkbStr );

  size_t result_len = gpkgLength - headerSize;
  *wkb = gpkgWkb + headerSize;
  *wkbLength = result_len;

  return GEODIFF_SUCCESS;
}
