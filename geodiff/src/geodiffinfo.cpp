/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include <stdio.h>
#include <string.h>
#include <string>

void help()
{
  printf( "GEODIFF %s", GEODIFF_version() );
  printf( "geodiffinfo <mode> [args...]\n" );
  printf( "[version info] geodiffinfo version\n" );
  printf( "[create changeset] geodiffinfo createChangeset base modified changeset\n" );
  printf( "[create rebased changeset] geodiffinfo createRebasedChangeset base modified changeset_their changeset\n" );
  printf( "[apply changeset] geodiffinfo applyChangeset base patched changeset\n" );
  printf( "[list changes (raw stdout) in changeset] geodiffinfo listChanges changeset\n" );
  printf( "[list changes (JSON stdout) in changeset] geodiffinfo listChangesJSON base changeset json\n" );
}

int err( const std::string msg )
{
  help();
  printf( "ERROR: %s", msg.c_str() );
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
  if ( argc < 1 + 5 )
  {
    return err( "invalid number of arguments to createRebasedChangeset" );
  }

  int ret = GEODIFF_createRebasedChangeset( argv[2], argv[3], argv[4], argv[5] );
  return ret;
}

int applyChangeset( int argc, char *argv[] )
{
  if ( argc < 1 + 3 )
  {
    return err( "invalid number of arguments to applyChangeset" );
  }

  int ret = GEODIFF_applyChangeset( argv[2], argv[3], argv[4] );
  return ret;
}

int listChanges( int argc, char *argv[] )
{
  if ( argc < 1 + 1 )
  {
    return err( "invalid number of arguments to listChanges" );
  }

  int ret = GEODIFF_listChanges( argv[2] );
  return ret;
}

int listChangesJSON( int argc, char *argv[] )
{
  if ( argc < 1 + 3 )
  {
    return err( "invalid number of arguments to listChangesJSON" );
  }

  int ret = GEODIFF_listChangesJSON( argv[2], argv[3], argv[4] );
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
    else if ( mode == "listChangesJSON" )
    {
      return listChangesJSON( argc, argv );
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
