/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>

#include <fstream>
#include <vector>
#include "geodiffutils.hpp"
#include "driver.h"

static bool fileToStdout( const std::string &filepath )
{
#ifdef WIN32
  std::ifstream f( stringToWString( filepath ) );
#else
  std::ifstream f( filepath );
#endif
  if ( !f.is_open() )
    return false;

  std::cout << f.rdbuf();
  return true;
}

static bool parseRequiredArgument( std::string &value, const std::vector<std::string> &args, size_t &i, const std::string &argName, const std::string &cmdName )
{
  if ( i < args.size() )
  {
    value = args[i++];
    return true;
  }
  else
  {
    std::cout << "Error: missing " << argName << " for '" << cmdName << "' command." << std::endl;
    return false;
  }
}

static bool checkNoExtraArguments( const std::vector<std::string> &args, size_t &i, const std::string &cmdName )
{
  if ( i < args.size() )
  {
    std::cout << "Error: unexpected extra arguments for '" << cmdName << "' command." << std::endl;
    return false;
  }
  return true;
}

static bool isOption( const std::string &str )
{
  return str.size() > 0 && str[0] == '-';
}

static bool parseDriverOption( const std::vector<std::string> &args, size_t &i, const std::string &cmdName, std::string &driverName, std::string &driverOptions )
{
  for ( ; i < args.size(); ++i )
  {
    if ( !isOption( args[i] ) )
      break;  // no more options

    if ( args[i] == "--driver" )
    {
      if ( i + 2 >= args.size() )
      {
        std::cout << "Error: missing arguments for driver option" << std::endl;
        return false;
      }
      driverName = args[i + 1];
      driverOptions = args[i + 2];
      i += 2;
      continue;
    }
    else
    {
      std::cout << "Error: unknown option '" << args[i] << "' for '" << cmdName << "' command." << std::endl;
      return false;
    }
  }
  return true;
}


static int handleCmdDiff( const std::vector<std::string> &args )
{
  //   geodiff diff [OPTIONS...] DB_1 DB_2 [CH_OUTPUT]

  bool writeJson = false;
  bool writeSummary = false;
  bool printOutput = true;
  std::string db1, db2, chOutput;
  std::string driver1Name = "sqlite", driver2Name = "sqlite", driver1Options, driver2Options;
  size_t i = 1;

  // parse options
  for ( ; i < args.size(); ++i )
  {
    if ( !isOption( args[i] ) )
      break;  // no more options

    if ( args[i] == "--json" )
    {
      writeJson = true;
      continue;
    }
    else if ( args[i] == "--summary" )
    {
      writeSummary = true;
      continue;
    }
    else if ( args[i] == "--driver" || args[i] == "--driver-1" || args[i] == "--driver-2" )
    {
      if ( i + 2 >= args.size() )
      {
        std::cout << "Error: missing arguments for driver option" << std::endl;
        return 1;
      }
      if ( args[i] == "--driver" || args[i] == "--driver-1" )
      {
        driver1Name = args[i + 1];
        driver1Options = args[i + 2];
      }
      if ( args[i] == "--driver" || args[i] == "--driver-2" )
      {
        driver2Name = args[i + 1];
        driver2Options = args[i + 2];
      }
      i += 2;
      continue;
    }
    else
    {
      std::cout << "Error: unknown option '" << args[i] << "' for 'diff' command." << std::endl;
      return 1;
    }
  }

  // check validity of options
  if ( writeJson && writeSummary )
  {
    std::cout << "Error: only one of the options can be passed: --json or --summary" << std::endl;
    return 1;
  }

  // parse required arguments
  if ( !parseRequiredArgument( db1, args, i, "DB_1", "diff" ) )
    return 1;
  if ( !parseRequiredArgument( db2, args, i, "DB_2", "diff" ) )
    return 1;

  if ( i < args.size() )
  {
    // optional output argument
    printOutput = false;
    chOutput = args[i++];

    if ( !checkNoExtraArguments( args, i, "diff" ) )
      return 1;
  }

  std::string changeset;
  TmpFile tmpChangeset;
  if ( printOutput || writeJson || writeSummary )
  {
    changeset = randomTmpFilename();
    tmpChangeset.setPath( changeset );
  }
  else
    changeset = chOutput;

  if ( driver1Name == driver2Name && driver1Options == driver2Options )
  {
    int ret = GEODIFF_createChangesetEx( driver1Name.data(), driver1Options.data(),
                                         db1.data(), db2.data(), changeset.data() );
    if ( ret != GEODIFF_SUCCESS )
    {
      std::cout << "Error: diff failed!" << std::endl;
      return 1;
    }
  }
  else
  {
    int ret = GEODIFF_createChangesetDr( driver1Name.data(), driver1Options.data(), db1.data(),
                                         driver2Name.data(), driver2Options.data(), db2.data(),
                                         changeset.data() );
    if ( ret != GEODIFF_SUCCESS )
    {
      std::cout << "Error: diff failed!" << std::endl;
      return 1;
    }
  }

  if ( writeJson || writeSummary )
  {
    std::string json;
    TmpFile tmpJson;
    if ( printOutput )
    {
      json = randomTmpFilename();
      tmpJson.setPath( json );
    }
    else
      json = chOutput;

    if ( writeJson )
    {
      if ( GEODIFF_listChanges( changeset.data(), json.data() ) != GEODIFF_SUCCESS )
      {
        std::cout << "Error: failed to convert changeset to JSON!" << std::endl;
        return 1;
      }
    }
    else  // writeSummary
    {
      if ( GEODIFF_listChangesSummary( changeset.data(), json.data() ) != GEODIFF_SUCCESS )
      {
        std::cout << "Error: failed to convert changeset to summary!" << std::endl;
        return 1;
      }
    }

    if ( printOutput )
    {
      if ( !fileToStdout( json ) )
      {
        std::cout << "Error: unable to read content of file " << json << std::endl;
        return 1;
      }
    }
  }
  else if ( printOutput )
  {
    if ( !fileToStdout( changeset ) )
    {
      std::cout << "Error: unable to read content of file " << changeset << std::endl;
      return 1;
    }
  }

  return 0;
}


static int handleCmdApply( const std::vector<std::string> &args )
{
  // geodiff apply [OPTIONS...] DB CH_INPUT

  size_t i = 1;
  std::string db, changeset;
  std::string driverName = "sqlite", driverOptions;

  // parse options
  if ( !parseDriverOption( args, i, "apply", driverName, driverOptions ) )
    return 1;

  // parse required arguments
  if ( !parseRequiredArgument( db, args, i, "DB", "apply" ) )
    return 1;
  if ( !parseRequiredArgument( changeset, args, i, "CH_INPUT", "apply" ) )
    return 1;
  if ( !checkNoExtraArguments( args, i, "apply" ) )
    return 1;

  int ret = GEODIFF_applyChangesetEx( driverName.data(), driverOptions.data(), db.data(), changeset.data() );
  if ( ret != GEODIFF_SUCCESS )
  {
    std::cout << "Error: apply changeset failed!" << std::endl;
    return 1;
  }

  return 0;
}

static int handleCmdRebaseDiff( const std::vector<std::string> &args )
{
  // geodiff rebase-diff [OPTIONS...] DB_BASE CH_BASE_OUR CH_BASE_THEIR CH_REBASED CONFLICT

  size_t i = 1;
  std::string dbBase, chBaseOur, chBaseTheir, chRebased, conflict;
  std::string driverName = "sqlite", driverOptions;

  // parse options
  if ( !parseDriverOption( args, i, "rebase-diff", driverName, driverOptions ) )
    return 1;

  if ( !parseRequiredArgument( dbBase, args, i, "DB_BASE", "rebase-diff" ) )
    return 1;
  if ( !parseRequiredArgument( chBaseOur, args, i, "CH_BASE_OUR", "rebase-diff" ) )
    return 1;
  if ( !parseRequiredArgument( chBaseTheir, args, i, "CH_BASE_THEIR", "rebase-diff" ) )
    return 1;
  if ( !parseRequiredArgument( chRebased, args, i, "CH_REBASED", "rebase-diff" ) )
    return 1;
  if ( !parseRequiredArgument( conflict, args, i, "CONFLICT", "rebase-diff" ) )
    return 1;
  if ( !checkNoExtraArguments( args, i, "rebase-diff" ) )
    return 1;

  int ret = GEODIFF_createRebasedChangesetEx(
              driverName.data(), driverOptions.data(),
              dbBase.data(), chBaseOur.data(),
              chBaseTheir.data(), chRebased.data(), conflict.data() );
  if ( ret != GEODIFF_SUCCESS )
  {
    std::cout << "Error: rebase-diff failed!" << std::endl;
    return 1;
  }

  return 0;
}

static int handleCmdRebaseDb( const std::vector<std::string> &args )
{
  // geodiff rebase-db [OPTIONS...] DB_BASE DB_OUR CH_BASE_THEIR CONFLICT

  size_t i = 1;
  std::string dbBase, dbOur, chBaseTheir, conflict;
  std::string driverName = "sqlite", driverOptions;

  // parse options
  if ( !parseDriverOption( args, i, "rebase-db", driverName, driverOptions ) )
    return 1;

  if ( !parseRequiredArgument( dbBase, args, i, "DB_BASE", "rebase-db" ) )
    return 1;
  if ( !parseRequiredArgument( dbOur, args, i, "DB_OUR", "rebase-db" ) )
    return 1;
  if ( !parseRequiredArgument( chBaseTheir, args, i, "CH_BASE_THEIR", "rebase-db" ) )
    return 1;
  if ( !parseRequiredArgument( conflict, args, i, "CONFLICT", "rebase-db" ) )
    return 1;
  if ( !checkNoExtraArguments( args, i, "rebase-db" ) )
    return 1;

  int ret = GEODIFF_rebaseEx( driverName.data(), driverOptions.data(), dbBase.data(), dbOur.data(),
                              chBaseTheir.data(), conflict.data() );
  if ( ret != GEODIFF_SUCCESS )
  {
    std::cout << "Error: rebase-db failed!" << std::endl;
    return 1;
  }

  return 0;
}

static int handleCmdInvert( const std::vector<std::string> &args )
{
  // geodiff invert CH_INPUT CH_OUTPUT

  size_t i = 1;
  std::string chInput, chOutput;

  if ( !parseRequiredArgument( chInput, args, i, "CH_INPUT", "invert" ) )
    return 1;
  if ( !parseRequiredArgument( chOutput, args, i, "CH_OUTPUT", "invert" ) )
    return 1;
  if ( !checkNoExtraArguments( args, i, "invert" ) )
    return 1;

  int ret = GEODIFF_invertChangeset( chInput.data(), chOutput.data() );
  if ( ret != GEODIFF_SUCCESS )
  {
    std::cout << "Error: invert changeset failed!" << std::endl;
    return 1;
  }

  return 0;
}

static int handleCmdConcat( const std::vector<std::string> &args )
{
  // geodiff concat CH_INPUT_1 CH_INPUT_2 [...] CH_OUTPUT

  if ( args.size() < 4 )
  {
    std::cout << "Error: 'concat' command needs at least two input changesets and one output changeset." << std::endl;
    return 1;
  }

  std::string chOutput = args[args.size() - 1];

  std::vector<const char *> changesets;
  for ( size_t i = 1; i < args.size() - 1; ++i )
  {
    changesets.push_back( args[i].data() );
  }

  int ret = GEODIFF_concatChanges( changesets.size(), changesets.data(), chOutput.data() );
  if ( ret != GEODIFF_SUCCESS )
  {
    std::cout << "Error: concat changesets failed!" << std::endl;
    return 1;
  }

  return 0;
}

static int handleCmdAsJson( const std::vector<std::string> &args )
{
  // geodiff as-json CH_INPUT [CH_OUTPUT]

  size_t i = 1;
  bool printOutput = true;
  std::string chInput, chOutput;

  // parse required arguments
  if ( !parseRequiredArgument( chInput, args, i, "CH_INPUT", "as-json" ) )
    return 1;

  if ( i < args.size() )
  {
    // optional output argument
    printOutput = false;
    chOutput = args[i++];

    if ( !checkNoExtraArguments( args, i, "as-json" ) )
      return 1;
  }

  std::string changeset;
  TmpFile tmpChangeset;
  if ( printOutput )
  {
    changeset = randomTmpFilename();
    tmpChangeset.setPath( changeset );
  }
  else
    changeset = chOutput;

  int ret = GEODIFF_listChanges( chInput.data(), changeset.data() );
  if ( ret != GEODIFF_SUCCESS )
  {
    std::cout << "Error: export changeset to JSON failed!" << std::endl;
    return 1;
  }

  if ( printOutput )
  {
    if ( !fileToStdout( changeset ) )
    {
      std::cout << "Error: unable to read content of file " << changeset << std::endl;
      return 1;
    }
  }

  return 0;
}

static int handleCmdAsSummary( const std::vector<std::string> &args )
{
  // geodiff as-summary CH_INPUT [SUMMARY]

  size_t i = 1;
  bool printOutput = true;
  std::string chInput, chOutput;

  // parse required arguments
  if ( !parseRequiredArgument( chInput, args, i, "CH_INPUT", "as-summary" ) )
    return 1;

  if ( i < args.size() )
  {
    // optional output argument
    printOutput = false;
    chOutput = args[i++];

    if ( !checkNoExtraArguments( args, i, "as-summary" ) )
      return 1;
  }

  std::string summary;
  TmpFile tmpSummary;
  if ( printOutput )
  {
    summary = randomTmpFilename();
    tmpSummary.setPath( summary );
  }
  else
    summary = chOutput;

  int ret = GEODIFF_listChangesSummary( chInput.data(), summary.data() );
  if ( ret != GEODIFF_SUCCESS )
  {
    std::cout << "Error: export changeset to summary failed!" << std::endl;
    return 1;
  }

  if ( printOutput )
  {
    if ( !fileToStdout( summary ) )
    {
      std::cout << "Error: unable to read content of file " << summary << std::endl;
      return 1;
    }
  }

  return 0;
}

static int handleCmdCopy( const std::vector<std::string> &args )
{
  // geodiff copy [OPTIONS...] DB_SOURCE DB_DESTINATION

  size_t i = 1;
  std::string chInput, chOutput;
  std::string driver1Name = "sqlite", driver1Options, driver2Name = "sqlite", driver2Options;

  // parse options
  for ( ; i < args.size(); ++i )
  {
    if ( !isOption( args[i] ) )
      break;  // no more options

    if ( args[i] == "--driver" || args[i] == "--driver-1" || args[i] == "--driver-2" )
    {
      if ( i + 2 >= args.size() )
      {
        std::cout << "Error: missing arguments for driver option" << std::endl;
        return 1;
      }
      if ( args[i] == "--driver" || args[i] == "--driver-1" )
      {
        driver1Name = args[i + 1];
        driver1Options = args[i + 2];
      }
      if ( args[i] == "--driver" || args[i] == "--driver-2" )
      {
        driver2Name = args[i + 1];
        driver2Options = args[i + 2];
      }
      i += 2;
      continue;
    }
    else
    {
      std::cout << "Error: unknown option '" << args[i] << "' for 'copy' command." << std::endl;
      return 1;
    }
  }

  if ( !parseRequiredArgument( chInput, args, i, "DB_SOURCE", "copy" ) )
    return 1;
  if ( !parseRequiredArgument( chOutput, args, i, "DB_DESTINATION", "copy" ) )
    return 1;
  if ( !checkNoExtraArguments( args, i, "copy" ) )
    return 1;

  if ( driver1Name == "sqlite" && driver2Name == "sqlite" )
  {
    int ret = GEODIFF_makeCopySqlite( chInput.data(), chOutput.data() );
    if ( ret != GEODIFF_SUCCESS )
    {
      std::cout << "Error: copy failed!" << std::endl;
      return 1;
    }
  }
  else
  {
    int ret = GEODIFF_makeCopy( driver1Name.data(), driver1Options.data(), chInput.data(),
                                driver2Name.data(), driver2Options.data(), chOutput.data() );
    if ( ret != GEODIFF_SUCCESS )
    {
      std::cout << "Error: copy failed!" << std::endl;
      return 1;
    }
  }

  return 0;
}

static int handleCmdSchema( const std::vector<std::string> &args )
{
  // geodiff schema [OPTIONS...] DB [SCHEMA_JSON]

  size_t i = 1;
  bool printOutput = true;
  std::string db, schemaJson;
  std::string driverName = "sqlite", driverOptions;

  // parse options
  if ( !parseDriverOption( args, i, "schema", driverName, driverOptions ) )
    return 1;

  // parse required arguments
  if ( !parseRequiredArgument( db, args, i, "DB", "schema" ) )
    return 1;

  if ( i < args.size() )
  {
    // optional output argument
    printOutput = false;
    schemaJson = args[i++];

    if ( !checkNoExtraArguments( args, i, "schema" ) )
      return 1;
  }

  std::string json;
  TmpFile tmpJson;
  if ( printOutput )
  {
    json = randomTmpFilename();
    tmpJson.setPath( json );
  }
  else
    json = schemaJson;

  int ret = GEODIFF_schema( driverName.data(), driverOptions.data(), db.data(), json.data() );
  if ( ret != GEODIFF_SUCCESS )
  {
    std::cout << "Error: export changeset to summary failed!" << std::endl;
    return 1;
  }

  if ( printOutput )
  {
    if ( !fileToStdout( json ) )
    {
      std::cout << "Error: unable to read content of file " << json << std::endl;
      return 1;
    }
  }

  return 0;
}

static int handleCmdDump( const std::vector<std::string> &args )
{
  // geodiff dump [OPTIONS...] DB CH_OUTPUT

  size_t i = 1;
  std::string db, chOutput;
  std::string driverName = "sqlite", driverOptions;

  // parse options
  if ( !parseDriverOption( args, i, "dump", driverName, driverOptions ) )
    return 1;

  if ( !parseRequiredArgument( db, args, i, "DB", "dump" ) )
    return 1;
  if ( !parseRequiredArgument( chOutput, args, i, "CH_OUTPUT", "dump" ) )
    return 1;
  if ( !checkNoExtraArguments( args, i, "dump" ) )
    return 1;

  int ret = GEODIFF_dumpData( driverName.data(), driverOptions.data(), db.data(), chOutput.data() );
  if ( ret != GEODIFF_SUCCESS )
  {
    std::cout << "Error: dump database failed!" << std::endl;
    return 1;
  }

  return 0;
}

static int handleCmdDrivers( const std::vector<std::string> &args )
{
  // geodiff drivers

  size_t i = 1;
  if ( !checkNoExtraArguments( args, i, "drivers" ) )
    return 1;

  for ( const std::string &driverName : Driver::drivers() )
  {
    std::cout << driverName << std::endl;
  }

  return 0;
}

static int handleCmdVersion( const std::vector<std::string> &args )
{
  ( void )args;

  std::cout << GEODIFF_version() << std::endl;
  return 0;
}


static int handleCmdHelp( const std::vector<std::string> &args )
{
  ( void )args; // arguments unused

  std::cout << "GEODIFF " << GEODIFF_version() << ", a tool for handling diffs for geospatial data.\n\
\n\
Usage: geodiff <command> [args...]\n\
\n\
You can control verbosity using the environment variable GEODIFF_LOGGER_LEVEL:\n\
    0 = Nothing, 1 = Errors, 2 = Warnings, 3 = Info, 4 = Debug\n\
    (The default is 2 - showing only errors and warnings.)\n\
\n\
In the commands listed below, database files may be any GeoPackage files or other\n\
kinds of SQLite database files. This is using the default 'sqlite' driver. Even\n\
when 'sqlite' driver is specified in a command with --driver option, there are\n\
no extra driver options it needs (empty string \"\" can be passed).\n\
\n\
There may be other drivers available, for example 'postgres' driver. Its driver\n\
options expect the connection string as understood by its client library - either\n\
key/value pairs (e.g. \"host=localhost port=5432 dbname=mydb\") or connection URI\n\
(e.g. \"postgresql://localhost:5432/mydb\").\n\
\n\
Create and apply changesets (diffs):\n\
\n\
  geodiff diff [OPTIONS...] DB_1 DB_2 [CH_OUTPUT]\n\
\n\
    Creates a changeset (diff) between databases DB_BASE and DB_MODIFIED. If CH_OUTPUT\n\
    is specified, the result is written to that file, otherwise the output goes\n\
    to the standard output. By default, the changeset is written in the binary\n\
    format.\n\
\n\
    Options:\n\
      --json          Write changeset in JSON format instead of binary\n\
      --summary       Write only a summary for each table (in JSON)\n\
      --driver NAME DRIVER_OPTIONS\n\
                      Use driver NAME instead of the default 'sqlite' for both\n\
                      databases. Driver-specific options are provided in CONN_OPTIONS.\n\
      --driver-1 NAME DRIVER_OPTIONS\n\
                      Use driver NAME just for the first database. This allows creation\n\
                      of changesets across datasets in two different drivers.\n\
      --driver-2 NAME DRIVER_OPTIONS\n\
                      Use driver NAME just for the second database. This allows\n\
                      creation of changesets across datasets in two different drivers.\n\
\n\
  geodiff apply [OPTIONS...] DB CH_INPUT\n\
\n\
    Applies a changeset (diff) from file CH_INPUT to the database file DB.\n\
    The changeset must be in the binary format (JSON format is not supported).\n\
\n\
    Options:\n\
      --driver NAME DRIVER_OPTIONS\n\
                      Use driver NAME instead of the default 'sqlite' for the\n\
                      database. Driver-specific options are provided in CONN_OPTIONS.\n\
\n\
Rebasing:\n\
\n\
  geodiff rebase-diff [OPTIONS...] DB_BASE CH_BASE_OUR CH_BASE_THEIR CH_REBASED CONFLICT\n\
\n\
    Creates a rebased changeset. Using DB_BASE as the common base for \"our\" local\n\
    changes (CH_BASE_OUR) and \"their\" changes (CH_BASE_THEIR), the command will take\n\
    \"our\" changes and rebase them on top of \"their\" changes, and write results\n\
    to CH_REBASED file (containing just \"our\" changes, but modified to apply cleanly\n\
    on top of \"their\" changes). As a result, taking DB_BASE, applying CH_BASE_THEIR\n\
    and then applying CH_REBASED will result in a database containing both \"our\" and\n\
    \"their\" changes. If there were any conflicts during the rebase, they will be\n\
    written to CONFLICT file (in JSON format).\n\
\n\
    Options:\n\
      --driver NAME DRIVER_OPTIONS\n\
                      Use driver NAME instead of the default 'sqlite' for both\n\
                      databases. Driver-specific options are provided in CONN_OPTIONS.\n\
\n\
  geodiff rebase-db [OPTIONS...] DB_BASE DB_OUR CH_BASE_THEIR CONFLICT\n\
\n\
    Rebases database DB_OUR, using DB_BASE as the common base and CH_BASE_THEIR as the other\n\
    source of changes. CH_BASE_THEIR is a changeset containing changes between DB_BASE and\n\
    some other database. This will cause DB_OUR to be updated in-place to contain changes\n\
    (DB_OUR - DB_BASE) rebased on top of CH_BASE_THEIR. After successful rebase, DB_OUR will\n\
    contain both \"our\" and \"their\" changes. If there were any conflicts during\n\
    the rebase, they will be written to CONFLICT file (in JSON format).\n\
\n\
    Options:\n\
      --driver NAME DRIVER_OPTIONS\n\
                      Use driver NAME instead of the default 'sqlite' for all three\n\
                      databases. Driver-specific options are provided in CONN_OPTIONS.\n\
\n\
Utilities:\n\
\n\
  geodiff invert CH_INPUT CH_OUTPUT\n\
\n\
    Inverts changeset in file CH_INPUT and writes inverted changeset to CH_OUTPUT.\n\
    Both input and output changesets are in the binary format.\n\
\n\
  geodiff concat CH_INPUT_1 CH_INPUT_2 [...] CH_OUTPUT\n\
\n\
    Concatenates two or more changeset files (CH_INPUT_1, CH_INPUT_2, ...) into a\n\
    single changeset. During concatenation, commands that act on the same rows get\n\
    merged together.\n\
\n\
  geodiff as-json CH_INPUT [CH_OUTPUT]\n\
\n\
    Converts the changeset in CH_INPUT file (in binary format) to JSON representation.\n\
    If CH_OUTPUT file is provided, it will be written to that file, otherwise it will\n\
    be written to the standard output.\n\
\n\
  geodiff as-summary CH_INPUT [SUMMARY]\n\
\n\
    Converts the changeset in CH_INPUT file (in binary format) to a summary JSON\n\
    which only contains overall counts of insert/update/delete commands for each table.\n\
\n\
  geodiff copy [OPTIONS...] DB_SOURCE DB_DESTINATION\n\
\n\
    Copies the source database DB_SOURCE to the destination database DB_DESTINATION.\n\
\n\
    Options:\n\
      --driver NAME DRIVER_OPTIONS\n\
                      Use driver NAME instead of the default 'sqlite' for both\n\
                      databases. Driver-specific options are provided in CONN_OPTIONS.\n\
      --driver-1 NAME DRIVER_OPTIONS\n\
                      Use driver NAME just for the first database. This allows creation\n\
                      of changesets across datasets in two different drivers.\n\
      --driver-2 NAME DRIVER_OPTIONS\n\
                      Use driver NAME just for the second database. This allows\n\
                      creation of changesets across datasets in two different drivers.\n\
\n\
  geodiff schema [OPTIONS...] DB [SCHEMA_JSON]\n\
\n\
    Writes database schema of DB as understood by geodiff as JSON. If SCHEMA_JSON file\n\
    is provided, the output will be written to the file, otherwise the standard output\n\
    will be used.\n\
\n\
    Options:\n\
      --driver NAME DRIVER_OPTIONS\n\
                      Use driver NAME instead of the default 'sqlite' for the\n\
                      database. Driver-specific options are provided in CONN_OPTIONS.\n\
\n\
  geodiff dump [OPTIONS...] DB CH_OUTPUT\n\
\n\
    Dumps content of database DB to a changeset as a series of \"insert\" commands.\n\
\n\
    Options:\n\
      --driver NAME DRIVER_OPTIONS\n\
                      Use driver NAME instead of the default 'sqlite' for the\n\
                      database. Driver-specific options are provided in CONN_OPTIONS.\n\
\n\
  geodiff drivers\n\
\n\
    Prints the list of all drivers supported in this version. The \"sqlite\" driver\n\
    is always available.\n\
\n\
  geodiff version\n\
\n\
    Prints version of geodiff.\n\
\n\
  geodiff help\n\
\n\
    Prints this help information.\n\
\n\
Copyright (C) 2019-2021 Lutra Consulting\n\
";
  return 0;
}


int main( int argc, char *argv[] )
{
  GEODIFF_init();

  if ( !getenv( "GEODIFF_LOGGER_LEVEL" ) )
  {
    GEODIFF_setMaximumLoggerLevel( LevelWarning );
  }

  std::vector<std::string> args;
  for ( int i = 1; i < argc; ++i )
    args.push_back( argv[i] );

  if ( args.size() < 1 )
  {
    std::cout << "Error: missing command. See 'geodiff help' for a list of commands." << std::endl;
    return 1;
  }

  std::string command = args[0];
  if ( command == "diff" )
  {
    return handleCmdDiff( args );
  }
  else if ( command == "apply" )
  {
    return handleCmdApply( args );
  }
  else if ( command == "rebase-diff" )
  {
    return handleCmdRebaseDiff( args );
  }
  else if ( command == "rebase-db" )
  {
    return handleCmdRebaseDb( args );
  }
  else if ( command == "invert" )
  {
    return handleCmdInvert( args );
  }
  else if ( command == "concat" )
  {
    return handleCmdConcat( args );
  }
  else if ( command == "as-json" )
  {
    return handleCmdAsJson( args );
  }
  else if ( command == "as-summary" )
  {
    return handleCmdAsSummary( args );
  }
  else if ( command == "copy" )
  {
    return handleCmdCopy( args );
  }
  else if ( command == "schema" )
  {
    return handleCmdSchema( args );
  }
  else if ( command == "dump" )
  {
    return handleCmdDump( args );
  }
  else if ( command == "drivers" )
  {
    return handleCmdDrivers( args );
  }
  else if ( command == "version" )
  {
    return handleCmdVersion( args );
  }
  else if ( command == "help" )
  {
    return handleCmdHelp( args );
  }
  else
  {
    std::cout << "Error: unknown command '" << command << "'. See 'geodiff help' for a list of commands." << std::endl;
    return 1;
  }
}
