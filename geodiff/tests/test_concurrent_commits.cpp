/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiff_testutils.hpp"
#include "gtest/gtest.h"

TEST( ConcurrentCommitsSqlite3Test, test_2_inserts )
{
  std::cout << "geopackage 2 concurent INSERTS (base) -> (A) and (base) -> (B)" << std::endl;
  std::cout << "both (A) and (B) are (base) with 1 extra new feature" << std::endl;

  std::string testname = "2_inserts";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "base.gpkg" );
  std::string modifiedA = pathjoin( testdir(), testname, "inserted_1_A.gpkg" );
  std::string modifiedB = pathjoin( testdir(), testname, "inserted_1_B.gpkg" );
  std::string changesetbaseA = pathjoin( tmpdir(), testname, "changeset_base_to_A.bin" );
  std::string changesetAB = pathjoin( tmpdir(), testname, "changeset_A_to_B.bin" );
  std::string changesetBbase = pathjoin( tmpdir(), testname, "changeset_B_to_base.bin" );
  std::string patchedAB = pathjoin( tmpdir(), testname, "patched_AB.gpkg" ) ;
  std::string expected_patchedAB = pathjoin( testdir(), testname, "merged_1_A_1_B.gpkg" );

  // create changeset base to A
  ASSERT_EQ( GEODIFF_createChangeset( base.c_str(), modifiedA.c_str(), changesetbaseA.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_listChanges( changesetbaseA.c_str() ), 2 * 1 + 3 ); // 3 updates in total and 2 inserts for each feature

  // create changeset A to B
  ASSERT_EQ( GEODIFF_createRebasedChangeset( base.c_str(), modifiedB.c_str(), changesetbaseA.c_str(), changesetAB.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_listChanges( changesetAB.c_str() ), 2 * 1 + 3 ); // 3 updates in total and 2 inserts for each feature

  // apply changeset to A to get AB
  ASSERT_EQ( GEODIFF_applyChangeset( modifiedA.c_str(), patchedAB.c_str(), changesetAB.c_str() ), GEODIFF_SUCCESS );

  // check that then new data has both features
  ASSERT_EQ( GEODIFF_createChangeset( base.c_str(), patchedAB.c_str(), changesetBbase.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_listChanges( changesetBbase.c_str() ), 2 * 2 + 3 ); // 3 updates in total and 2 inserts for each feature

  // check that it equals expected result
  ASSERT_TRUE( equals( patchedAB, expected_patchedAB ) );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
