/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>

void help()
{
  printf( "GEODIFF %s", GEODIFF_version() );
  printf( "geodiffinfo <mode> [args...]\n\n" );
  printf( "you can control verbosity of the log by env variable\n" );
  printf( "GEODIFF_LOGGER_LEVEL 0(Nothing)-4(Debug)\n" );
  printf( "by default you get only errors printed to stdout\n\n" );
  printf( "[version info] geodiffinfo version\n" );
  printf( "[create changeset] geodiffinfo createChangeset base modified changeset\n" );
  printf( "[create rebased changeset] geodiffinfo createRebasedChangeset base modified changeset_their changeset conflict\n" );
  printf( "[apply changeset] geodiffinfo applyChangeset base changeset\n" );
  printf( "[list changes (JSON) in changeset] geodiffinfo listChanges changeset json\n" );
  printf( "[list summary of changes (JSON) in changeset] geodiffinfo listChangesSummary changeset json\n" );
}

int err( const std::string msg )
{
  help();
  printf( "ERROR: %s\n", msg.c_str() );
  return 1;
}

int createChangeset( int argc, char *argv[] )
{
  if ( argc < 1 + 4 )
  {
    return err( "invalid number of arguments to createChangeset" );
  }

  int ret = GEODIFF_createChangeset( argv[2], argv[3], argv[4] );
  return ret;
}

int createRebasedChangeset( int argc, char *argv[] )
{
  if ( argc < 1 + 6 )
  {
    return err( "invalid number of arguments to createRebasedChangeset" );
  }

  int ret = GEODIFF_createRebasedChangeset( argv[2], argv[3], argv[4], argv[5], argv[6] );
  return ret;
}

int applyChangeset( int argc, char *argv[] )
{
  if ( argc < 1 + 2 )
  {
    return err( "invalid number of arguments to applyChangeset" );
  }

  int ret = GEODIFF_applyChangeset( argv[2], argv[3] );
  return ret;
}

int listChanges( int argc, char *argv[] )
{
  if ( argc < 1 + 2 )
  {
    return err( "invalid number of arguments to listChanges" );
  }

  int ret = GEODIFF_listChanges( argv[2], argv[3] );
  return ret;
}

int listChangesSummary( int argc, char *argv[] )
{
  if ( argc < 1 + 2 )
  {
    return err( "invalid number of arguments to listChangesSummary" );
  }

  int ret = GEODIFF_listChangesSummary( argv[2], argv[3] );
  return ret;
}

int main( int argc, char *argv[] )
{
  GEODIFF_init();
  if ( argc > 1 )
  {
    std::string mode( argv[1] );
    if ( mode == "version" )
    {
      printf( "%s", GEODIFF_version() );
      return 0;
    }
    else if ( mode == "createChangeset" )
    {
      return createChangeset( argc, argv );
    }
    else if ( mode == "createRebasedChangeset" )
    {
      return createRebasedChangeset( argc, argv );
    }
    else if ( mode == "applyChangeset" )
    {
      return applyChangeset( argc, argv );
    }
    else if ( mode == "listChanges" )
    {
      return listChanges( argc, argv );
    }
    else if ( mode == "listChangesSummary" )
    {
      return listChangesSummary( argc, argv );
    }
    else
    {
      return err( "invalid mode" );
    }
  }
  else
  {
    return err( "missing mode" );
  }
}
