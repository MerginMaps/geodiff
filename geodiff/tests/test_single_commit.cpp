/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"

TEST( SingleCommitSqlite3Test, test_sqlite_no_gis )
{
  // sqlite 2 updated 1 added 1 deleted
  std::string base = pathjoin( testdir(), "base.sqlite" );
  std::string modified = pathjoin( testdir(), "modified_base.sqlite" );
  std::string changeset = pathjoin( tmpdir(), "changeset_base_sqlite.bin" );
  std::string changeset2 = pathjoin( tmpdir(), "changeset_after_apply_base_sqlite.bin" );
  std::string patched = pathjoin( tmpdir(), "patched_base_sqlite.gpkg" );

  ASSERT_EQ( GEODIFF_createChangeset( base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_listChanges( changeset.c_str() ), 4 );
  ASSERT_EQ( GEODIFF_applyChangeset( base.c_str(), patched.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );

  // check that now it is same file
  ASSERT_EQ( GEODIFF_createChangeset( patched.c_str(), modified.c_str(), changeset2.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_listChanges( changeset2.c_str() ), 0 );
}

TEST( SingleCommitSqlite3Test, geopackage )
{
  // geopackage 1 updated geometry
  std::string base = pathjoin( testdir(), "base.gpkg" );
  std::string modified = pathjoin( testdir(), "modified_1_geom.gpkg" );
  std::string changeset = pathjoin( tmpdir(), "changeset_base_gpkg.bin" );
  std::string changeset2 = pathjoin( tmpdir(), "changeset_after_apply_base_gpkg.bin" );
  std::string patched = pathjoin( tmpdir(), "patched_base_gpkg.gpkg" );

  ASSERT_EQ( GEODIFF_createChangeset( base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_listChanges( changeset.c_str() ), 3 );

  ASSERT_EQ( GEODIFF_applyChangeset( base.c_str(), patched.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );

  // check that now it is same file
  ASSERT_EQ( GEODIFF_createChangeset( patched.c_str(), modified.c_str(), changeset2.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_listChanges( changeset2.c_str() ), 0 );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
