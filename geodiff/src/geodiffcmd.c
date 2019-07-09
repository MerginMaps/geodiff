/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include <stdio.h>
#include <string.h>

int main( int argc, char *argv[] )
{
  int nchanges;

  init();
  if ( argc == 1 + 4 )
  {
    char *mode = argv[1];
    if ( strncmp( mode, "diff", 4 ) == 0 )
    {
      int ret = createChangeset( argv[2], argv[3], argv[4] );
      printf( "diff: %d", ret );
      listChanges( argv[4], &nchanges );
      return ret;
    }
    else if ( strncmp( mode, "apply", 5 ) == 0 )
    {
      int ret = applyChangeset( argv[2], argv[3], argv[4] );
      listChanges( argv[4], &nchanges );
      printf( "apply: %d", ret );
      return ret;
    }
    else
    {
      printf( "invalid mode, must be diff or apply" );
    }
  }
  else
  {
    printf( "n args: %d\n", argc );
    printf( "[create changeset] GEODIFF diff file1 file2 changeset.bin\n" );
    printf( "[create changeset] GEODIFF diff file1 file2 changeset.bin\n" );

    printf( "[apply changeset] GEODIFF apply file1 file2 changeset.bin\n" );
    //TODO rebase?
    return 1;
  }

  return 0;
}
