/*
 GEODIFF - MIT License
 Copyright (C) 2022 Alexander Bruy
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"
#include "geodiffutils.hpp"

TEST( SkipTablesSqlite3Test, test_skip_create )
{
  std::string testname( "test_skip_create" );
  std::string base = pathjoin( testdir(), "skip_tables", "base.gpkg" );
  std::string modified_all = pathjoin( testdir(), "skip_tables", "modified_all.gpkg" );
  std::string modified_points = pathjoin( testdir(), "skip_tables", "modified_points.gpkg" );
  std::string changeset_points = pathjoin( tmpdir(), testname, "changeset_points.bin" );
  std::string patched_points = pathjoin( tmpdir(), testname, "patched_points.gpkg" );

  makedir( pathjoin( tmpdir(), testname ) );

  // ignore lines table when creating changeset
  std::string tablesToSkip( "lines" );
  Context *ctx = static_cast<Context *>( testContext() );
  ctx->setTablesToSkip( tablesToSkip );

  int res = GEODIFF_createChangeset( testContext(), base.c_str(), modified_all.c_str(), changeset_points.c_str() );
  EXPECT_EQ( res, GEODIFF_SUCCESS );

  int nchanges = GEODIFF_changesCount( testContext(), changeset_points.c_str() );
  EXPECT_EQ( nchanges, 4 );

  // reconstuct changes in the points layer
  filecopy( patched_points, base );
  res = GEODIFF_applyChangeset( testContext(), patched_points.c_str(), changeset_points.c_str() );
  EXPECT_EQ( res, GEODIFF_SUCCESS );

  // check that now it is same file with modified
  EXPECT_TRUE( equals( patched_points, modified_points, false ) );

  // reset skip list
  tablesToSkip = "";
  ctx->setTablesToSkip( tablesToSkip );
}

TEST( SkipTablesSqlite3Test, test_skip_apply )
{
  std::string testname( "test_skip_apply" );
  std::string base = pathjoin( testdir(), "skip_tables", "base.gpkg" );
  std::string modified_all = pathjoin( testdir(), "skip_tables", "modified_all.gpkg" );
  std::string modified_points = pathjoin( testdir(), "skip_tables", "modified_points.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.bin" );
  std::string patched_points = pathjoin( tmpdir(), testname, "patched_points.gpkg" );

  makedir( pathjoin( tmpdir(), testname ) );

  int res = GEODIFF_createChangeset( testContext(), base.c_str(), modified_all.c_str(), changeset.c_str() );
  EXPECT_EQ( res, GEODIFF_SUCCESS );

  int nchanges = GEODIFF_changesCount( testContext(), changeset.c_str() );
  EXPECT_EQ( nchanges, 6 );

  // ignore lines table when applying changeset
  std::string tablesToSkip( "lines" );
  Context *ctx = static_cast<Context *>( testContext() );
  ctx->setTablesToSkip( tablesToSkip );

  // reconstuct changes in the points layer
  filecopy( patched_points, base );
  res = GEODIFF_applyChangeset( testContext(), patched_points.c_str(), changeset.c_str() );
  EXPECT_EQ( res, GEODIFF_SUCCESS );

  // check that now it is same file with modified
  EXPECT_TRUE( equals( patched_points, modified_points, false ) );

  // reset skip list
  tablesToSkip = "";
  ctx->setTablesToSkip( tablesToSkip );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
