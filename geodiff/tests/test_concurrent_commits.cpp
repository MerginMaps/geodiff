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
  int expected_conflicts,
  bool ignore_timestamp_change = false
)
{
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), baseX );
  std::string modifiedA = pathjoin( testdir(), testname, A );
  std::string modifiedB = pathjoin( testdir(), testname, B );
  std::string changesetbaseA = pathjoin( tmpdir(), testname, "changeset_base_to_A.bin" );
  std::string changesetAB = pathjoin( tmpdir(), testname, "changeset_A_to_B.bin" );
  std::string conflictAB = pathjoin( tmpdir(), testname, "conflict_A_to_B.json" );
  std::string changesetBbase = pathjoin( tmpdir(), testname, "changeset_B_to_base.bin" );
  std::string patchedAB = pathjoin( tmpdir(), testname, "patched_AB.gpkg" ) ;
  std::string patchedAB_2 = pathjoin( tmpdir(), testname, "patched_AB_2.gpkg" ) ;
  std::string conflict2 = pathjoin( tmpdir(), testname, "conflict2.json" );
  std::string expected_patchedAB = pathjoin( testdir(), testname, expected_AB );
  std::string json = pathjoin( tmpdir(), testname, testname + ".json" );
  std::string json_summary = pathjoin( tmpdir(), testname, testname + "_summary.json" );


  // create changeset base to A
  if ( GEODIFF_createChangeset( base.c_str(), modifiedA.c_str(), changesetbaseA.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createChangeset A" << std::endl;
    return false;
  }

  int nchanges = GEODIFF_changesCount( changesetbaseA.c_str() );
  if ( nchanges != expected_changes_A )
  {
    std::cout << "err GEODIFF_listChanges A: " << nchanges << std::endl;
    return false;
  }

  // create changeset A to B
  if ( GEODIFF_createRebasedChangeset( base.c_str(), modifiedB.c_str(), changesetbaseA.c_str(), changesetAB.c_str(), conflictAB.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createRebasedChangeset AB" << std::endl;
    return false;
  }

  nchanges = GEODIFF_changesCount( changesetAB.c_str() );
  if ( nchanges != expected_changes_AB )
  {
    std::cout << "err GEODIFF_listChanges AB: " << nchanges << " expected: " << expected_changes_AB << " in " << changesetAB << std::endl;
    return false;
  }

  int nConflicts = countConflicts( conflictAB );
  if ( nConflicts != 0 )
  {
    printFileToStdout( "Conflicts", conflictAB );
  }

  if ( nConflicts != expected_conflicts )
  {
    std::cout << "err GEODIFF_listChanges AB conflict: " << nConflicts << " expected: " << expected_conflicts << " in " << changesetAB << std::endl;
    return false;
  }

  // apply changeset to A to get AB
  filecopy( patchedAB, modifiedA );
  if ( GEODIFF_applyChangeset( patchedAB.c_str(), changesetAB.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_applyChangeset A -> AB" << std::endl;
    return false;
  }

  // check that then new data has both edits
  if ( GEODIFF_createChangeset( base.c_str(), patchedAB.c_str(), changesetBbase.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createChangeset B -> base" << std::endl;
    return false;
  }

  nchanges = GEODIFF_changesCount( changesetBbase.c_str() );
  if ( nchanges != expected_changes_XB )
  {
    std::cout << "err GEODIFF_listChanges B->base: " << nchanges << " expected: " << expected_changes_XB << " in " << changesetBbase << std::endl;
    return false;
  }

  // print JSON
  printJSON( changesetAB, json, json_summary );

  // check that it equals expected result
  std::cout << "final file: " << patchedAB << std::endl;
  if ( !equals( patchedAB, expected_patchedAB, ignore_timestamp_change ) )
  {
    std::cout << "err equals A" << std::endl;
    return false;
  }

  // now check that we get same result in case of direct rebase
  filecopy( patchedAB_2, modifiedB );
  if ( GEODIFF_rebase( base.c_str(), modifiedA.c_str(), patchedAB_2.c_str(), conflict2.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_rebase A" << std::endl;
    return false;
  }

  nConflicts = countConflicts( conflict2 );
  if ( nConflicts != expected_conflicts )
  {
    std::cout << "err GEODIFF_rebase AB conflict: " << nConflicts << " expected: " << expected_conflicts << std::endl;
    return false;
  }


  return equals( patchedAB_2, expected_patchedAB, ignore_timestamp_change );
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
               3,
               0
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
               2,
               1
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
               2,
               0
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
               0,
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
               2,
               0
             );
  ASSERT_TRUE( ret );
}

TEST( ConcurrentCommitsSqlite3Test, test_update_extent )
{
  std::cout << "the base2theirs contains only a change to \"simple\" table, ";
  std::cout << "but base2modified contains an update to a gpkg_contents table (extent)" << std::endl;
  std::cout << "https://github.com/lutraconsulting/geodiff/issues/30" << std::endl;

  bool ret = _test(
               "base.gpkg",
               "updates_to_different_tables",
               "add_1.gpkg",
               "add_1_outside_extent.gpkg",
               "final_extent.gpkg",
               2,
               2,
               3,
               0
             );
  ASSERT_TRUE( ret );
}

TEST( ConcurrentCommitsSqlite3Test, test_update_2_different_tables )
{
  std::cout << "both concurrent changes different tables" << std::endl;
  std::cout << "https://github.com/lutraconsulting/geodiff/issues/29" << std::endl;

  bool ret = _test(
               "base3.gpkg",
               "updates_to_different_tables",
               "add_point.gpkg",
               "add_line.gpkg",
               "final_add_line.gpkg",
               2,
               2,
               4,
               0
             );
  ASSERT_TRUE( ret );

  ret = _test(
          "base3.gpkg",
          "updates_to_different_tables",
          "add_point.gpkg",
          "modify_line.gpkg",
          "final_modify_line.gpkg",
          2,
          3,
          5,
          0
        );
  ASSERT_TRUE( ret );

  ret = _test(
          "base3.gpkg",
          "updates_to_different_tables",
          "add_point.gpkg",
          "remove_line.gpkg",
          "final_delete_line.gpkg",
          2,
          2,
          4,
          0
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
