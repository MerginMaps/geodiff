/*
 GEODIFF - MIT License
 Copyright (C) 2023 Peter Petrik
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"
#include "geodiffutils.hpp"

#include <string>

TEST( CAPITest, invalid_calls )
{
  GEODIFF_ContextH invalidContext = nullptr;
  GEODIFF_ContextH context = GEODIFF_createContext();

  char buffer[200];

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_driverNameFromIndex( invalidContext, 0, buffer ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_driverIsRegistered( invalidContext, "sqlite" ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_driverIsRegistered( context, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_CX_setLoggerCallback( invalidContext, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_CX_setMaximumLoggerLevel( invalidContext, GEODIFF_LoggerLevel::LevelWarning ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_CX_setTablesToSkip( invalidContext, 0, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_CX_setTablesToSkip( context, 1, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createChangesetEx( invalidContext, "sqlite", nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createChangesetEx( context, "sqlite", nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createChangesetEx( context, "invalid driver", " ", " ", " ", " " ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createChangesetDr( invalidContext, "sqlite", nullptr, nullptr, "sqlite", nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createChangesetDr( context, "sqlite", nullptr, nullptr, "sqlite", nullptr, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_applyChangesetEx( invalidContext, "sqlite", nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_applyChangesetEx( context, "sqlite", nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_applyChangesetEx( context, "invalid driver", " ", " ", " " ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createRebasedChangeset( invalidContext, nullptr, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createRebasedChangeset( context, nullptr, nullptr, nullptr, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createRebasedChangesetEx( invalidContext, "sqlite", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createRebasedChangesetEx( context, "sqlite", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );

  ASSERT_EQ( -1, GEODIFF_hasChanges( invalidContext, nullptr ) );
  ASSERT_EQ( -1, GEODIFF_hasChanges( context, nullptr ) );

  ASSERT_EQ( -1, GEODIFF_changesCount( invalidContext, nullptr ) );
  ASSERT_EQ( -1, GEODIFF_changesCount( context, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_listChanges( invalidContext, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_listChanges( context, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_listChangesSummary( invalidContext, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_listChangesSummary( context, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_invertChangeset( invalidContext, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_invertChangeset( context, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_concatChanges( invalidContext, 1, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_concatChanges( context, 1, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_concatChanges( context, 2, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase( invalidContext, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase( context, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase( context, "bad file", "bad file", "bad file", "bad file" ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase( context, pathjoin( testdir(), "base.gpkg" ).c_str(), "bad file", "bad file", "bad file" ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase( context, pathjoin( testdir(), "base.gpkg" ).c_str(), pathjoin( testdir(), "base.gpkg" ).c_str(), "bad file", "bad file" ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebaseEx( invalidContext, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebaseEx( context, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopy( invalidContext, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopy( context, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopy( context, "invalid driver",  " ",  " ",  " ",  " ",  " " ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopy( context, "sqlite",  " ",  " ", "invalid driver",  " ",  " " ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopySqlite( invalidContext, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopySqlite( context, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_dumpData( invalidContext, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_dumpData( context, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_dumpData( context, "invalid driver",  " ",  " ",  " " ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_schema( invalidContext, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_schema( context, nullptr, nullptr, nullptr, nullptr ) );

  ASSERT_EQ( nullptr, GEODIFF_readChangeset( invalidContext, nullptr ) );
  ASSERT_EQ( nullptr, GEODIFF_readChangeset( context, nullptr ) );

  ASSERT_EQ( nullptr, GEODIFF_CR_nextEntry( invalidContext, nullptr, nullptr ) );
  bool ok;
  ASSERT_EQ( nullptr, GEODIFF_CR_nextEntry( invalidContext, nullptr, &ok ) );
  ASSERT_EQ( nullptr, GEODIFF_CR_nextEntry( context, nullptr, &ok ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createWkbFromGpkgHeader( invalidContext, nullptr, size_t( 1 ), nullptr, nullptr ) );

  size_t size;
  const char *wkb;
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createWkbFromGpkgHeader( invalidContext, " ", size_t( 0 ), &wkb, &size ) );

  GEODIFF_CX_destroy( context );
}

TEST( CAPITest, test_copy )
{
  GEODIFF_ContextH context = GEODIFF_createContext();
  makedir( pathjoin( tmpdir(), "test_copy" ) );

  // database "db-base.gpkg"
  // - table AUDIT has no PK
  // - custom trigger that adds entry to AUDIT table on each update of "simple" table

  {
    std::string fileOutput = pathjoin( tmpdir(), "test_copy", "db-makeCopy.gpkg" );

    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopy(
                 context,
                 "sqlite",
                 "",
                 pathjoin( testdir(), "gpkg_custom_triggers", "db-base.gpkg" ).c_str(),
                 "sqlite",
                 "",
                 fileOutput.c_str() ) );


    // THIS DROPS ALL TRIGGERS!!
    ASSERT_FALSE( fileContentEquals( fileOutput, pathjoin( testdir(), "gpkg_custom_triggers", "db-base.gpkg" ) ) );
  }


  {
    std::string fileOutput = pathjoin( tmpdir(), "test_copy", "db-makeCopySqlite.gpkg" );

    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopySqlite(
                 context,
                 pathjoin( testdir(), "gpkg_custom_triggers", "db-base.gpkg" ).c_str(),
                 fileOutput.c_str() ) );
  }

  GEODIFF_CX_destroy( context );
}

TEST( CAPITest, test_rebases )
{
  GEODIFF_ContextH context = GEODIFF_createContext();
  makedir( pathjoin( tmpdir(), "test_rebases" ) );

  {
    std::string fileOutput = pathjoin( tmpdir(), "test_rebases", "text_pk_A.sqlite" );
    std::string fileConflict = pathjoin( tmpdir(), "test_rebases", "output_text_pk.log" );


    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopySqlite(
                 context,
                 pathjoin( testdir(), "sqlite_pks", "text_pk_A.sqlite" ).c_str(),
                 fileOutput.c_str() ) );


    ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase(
                 context,
                 pathjoin( testdir(), "sqlite_pks", "text_pk.sqlite" ).c_str(),
                 pathjoin( testdir(), "sqlite_pks", "text_pk_B.sqlite" ).c_str(),
                 fileOutput.c_str(),
                 fileConflict.c_str() )
             );
  }

  {
    std::string fileOutput = pathjoin( tmpdir(), "test_rebases", "output_compose_pk.sqlite" );
    std::string fileConflict = pathjoin( tmpdir(), "test_rebases", "output_compose_pk.log" );


    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopySqlite(
                 context,
                 pathjoin( testdir(), "sqlite_pks", "multi_primary_key_A.sqlite" ).c_str(),
                 fileOutput.c_str() ) );


    ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase(
                 context,
                 pathjoin( testdir(), "sqlite_pks", "multi_primary_key.sqlite" ).c_str(),
                 pathjoin( testdir(), "sqlite_pks", "multi_primary_key_B.sqlite" ).c_str(),
                 fileOutput.c_str(),
                 fileConflict.c_str() )
             );
  }

  {
    std::string fileOutput = pathjoin( tmpdir(), "test_rebases", "output_custom_triggers.gpkg" );
    std::string fileConflict = pathjoin( tmpdir(), "test_rebases", "output_custom_triggers.log" );


    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopySqlite(
                 context,
                 pathjoin( testdir(), "gpkg_custom_triggers", "db-modified_A.gpkg" ).c_str(),
                 fileOutput.c_str() ) );


    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_rebase(
                 context,
                 pathjoin( testdir(), "gpkg_custom_triggers", "db-base.gpkg" ).c_str(),
                 pathjoin( testdir(), "gpkg_custom_triggers", "db-modified_B.gpkg" ).c_str(),
                 fileOutput.c_str(),
                 fileConflict.c_str() )
             );
  }

  GEODIFF_CX_destroy( context );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
