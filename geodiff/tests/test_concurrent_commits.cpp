/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiff.h"
#include "geodiff_testutils.hpp"
#include "gtest/gtest.h"

bool _test(
  const std::string &baseX,
  const std::string &testname,
  const std::string &A,
  const std::string &B,
  const std::string &expected_AB,
  int expected_changes_A,
  int expected_changes_AB,
  int expected_changes_XB,
  bool ignore_timestamp_change = false
)
{
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), baseX );
  std::string modifiedA = pathjoin( testdir(), testname, A );
  std::string modifiedB = pathjoin( testdir(), testname, B );
  std::string changesetbaseA = pathjoin( tmpdir(), testname, "changeset_base_to_A.bin" );
  std::string changesetAB = pathjoin( tmpdir(), testname, "changeset_A_to_B.bin" );
  std::string changesetBbase = pathjoin( tmpdir(), testname, "changeset_B_to_base.bin" );
  std::string patchedAB = pathjoin( tmpdir(), testname, "patched_AB.gpkg" ) ;
  std::string expected_patchedAB = pathjoin( testdir(), testname, expected_AB );

  // create changeset base to A
  if ( GEODIFF_createChangeset( base.c_str(), modifiedA.c_str(), changesetbaseA.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createChangeset A" << std::endl;
    return false;
  }

  int nchanges = GEODIFF_listChanges( changesetbaseA.c_str() );
  if ( nchanges != expected_changes_A )
  {
    std::cout << "err GEODIFF_listChanges A: " << nchanges << std::endl;
    return false;
  }

  // create changeset A to B
  if ( GEODIFF_createRebasedChangeset( base.c_str(), modifiedB.c_str(), changesetbaseA.c_str(), changesetAB.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createRebasedChangeset AB" << std::endl;
    return false;
  }

  nchanges = GEODIFF_listChanges( changesetAB.c_str() );
  if ( nchanges != expected_changes_AB )
  {
    std::cout << "err GEODIFF_listChanges AB: " << nchanges << " expected: " << expected_changes_AB << std::endl;
    return false;
  }

  // apply changeset to A to get AB
  if ( GEODIFF_applyChangeset( modifiedA.c_str(), patchedAB.c_str(), changesetAB.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_applyChangeset A -> AB" << std::endl;
    return false;
  }

  // check that then new data has both edits
  if ( GEODIFF_createChangeset( base.c_str(), patchedAB.c_str(), changesetBbase.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createChangeset Bbase" << std::endl;
    return false;
  }

  nchanges = GEODIFF_listChanges( changesetBbase.c_str() );
  if ( nchanges != expected_changes_XB )
  {
    std::cout << "err GEODIFF_listChanges Bbase: " << nchanges << std::endl;
    return false;
  }

  // check that it equals expected result
  std::cout << "final file: " << patchedAB << std::endl;
  return equals( patchedAB, expected_patchedAB, ignore_timestamp_change ) ;
}

TEST( ConcurrentCommitsSqlite3Test, test_2_inserts )
{
  std::cout << "geopackage 2 concurent INSERTS (base) -> (A) and (base) -> (B)" << std::endl;
  std::cout << "both (A) and (B) are (base) with 1 extra new feature" << std::endl;
  std::cout << "expected result: both new features are inserted" << std::endl;

  bool ret = _test(
               "base.gpkg",
               "2_inserts",
               "inserted_1_A.gpkg",
               "inserted_1_B.gpkg",
               "merged_1_A_1_B.gpkg",
               2,
               2,
               3
             );
  ASSERT_TRUE( ret );
}


TEST( ConcurrentCommitsSqlite3Test, test_2_edits )
{
  std::cout << "geopackage 2 concurent UPDATES (base) -> (A) and (base) -> (B)" << std::endl;
  std::cout << "both (A) and (B) are (base) edit int attribute of the feature 2" << std::endl;
  std::cout << "(A) also edits geometry of the feature 2" << std::endl;
  std::cout << "expected result: feature 2 has geometry from (B) and attribute from (A)" << std::endl;

  bool ret = _test(
               "base.gpkg",
               "2_updates",
               "updated_A.gpkg",
               "updated_B.gpkg",
               "merged_1_A_1_B.gpkg",
               2,
               2,
               2
             );
  ASSERT_TRUE( ret );
}

TEST( ConcurrentCommitsSqlite3Test, test_2_deletes )
{
  std::cout << "geopackage concurent DELETE (base) -> (A) and DELETE (base) -> (B)" << std::endl;
  std::cout << "both (A) and (B) deleted the feature 2" << std::endl;
  std::cout << "expected result: feature 2 is deleted (the rebased changeset is empty)" << std::endl;

  bool ret = _test(
               "base.gpkg",
               "2_deletes",
               "deleted_A.gpkg",
               "deleted_B.gpkg",
               "merged_A_B.gpkg",
               2,
               1,
               2
             );
  ASSERT_TRUE( ret );
}

TEST( ConcurrentCommitsSqlite3Test, test_delete_update )
{
  std::cout << "geopackage concurent DELETE (base) -> (A) and UPDATE (base) -> (B)" << std::endl;
  std::cout << "(A) deleted the feature 2 and (B) edits the geom of feature 2" << std::endl;
  std::cout << "expected result: feature 2 is deleted" << std::endl;

  bool ret = _test(
               "base.gpkg",
               "delete_update",
               "deleted_A.gpkg",
               "updated_B.gpkg",
               "deleted_A.gpkg",
               2,
               1,
               2,
               true
             );
  ASSERT_TRUE( ret );
}

TEST( ConcurrentCommitsSqlite3Test, test_update_delete )
{
  std::cout << "geopackage concurent UPDATE (base) -> (A) and DELETE (base) -> (B)" << std::endl;
  std::cout << "(A) edits the geom of feature 2 and (B) deleted the feature 2" << std::endl;
  std::cout << "expected result: feature 2 is deleted" << std::endl;

  bool ret = _test(
               "base.gpkg",
               "update_delete",
               "updated_A.gpkg",
               "deleted_B.gpkg",
               "deleted_B.gpkg",
               2,
               2,
               2
             );
  ASSERT_TRUE( ret );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
