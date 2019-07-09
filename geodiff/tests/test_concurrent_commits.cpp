/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiff_testutils.hpp"
#include "gtest/gtest.h"

TEST( ConcurrentCommitsSqlite3Test, test_2_inserts )
{
  std::cout << "geopackage 2 concurent modifications (base) -> (A) and (base) -> (B)" << std::endl;

  std::string base = pathjoin( testdir(), "base.gpkg" );
  std::string modifiedA = pathjoin( testdir(), "inserted_1_A.gpkg" );
  std::string modifiedB = pathjoin( testdir(), "inserted_1_B.gpkg" );
  std::string changesetbaseA = pathjoin( tmpdir(), "changeset_base_to_A.bin" );
  std::string changesetAB = pathjoin( tmpdir(), "changeset_A_to_B.bin" );
  std::string changesetBbase = pathjoin( tmpdir(), "changeset_B_to_base.bin" );
  std::string patchedAB = pathjoin( tmpdir(), "patched_AB.gpkg" ) ;

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
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
