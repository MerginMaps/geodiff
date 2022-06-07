/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include <fstream>
#include "geodiff.h"
#include "geodiff_testutils.hpp"
#include "gtest/gtest.h"

#include "json.hpp"
#include "geodiffutils.hpp"

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
  if ( GEODIFF_createChangeset( testContext(), base.c_str(), modifiedA.c_str(), changesetbaseA.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createChangeset A" << std::endl;
    return false;
  }

  int nchanges = GEODIFF_changesCount( testContext(), changesetbaseA.c_str() );
  if ( nchanges != expected_changes_A )
  {
    std::cout << "err GEODIFF_listChanges A: " << nchanges << std::endl;
    return false;
  }

  // create changeset A to B
  if ( GEODIFF_createRebasedChangeset( testContext(),  base.c_str(), modifiedB.c_str(), changesetbaseA.c_str(), changesetAB.c_str(), conflictAB.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createRebasedChangeset AB" << std::endl;
    return false;
  }

  // print JSON
  printJSON( changesetAB, json, json_summary );

  nchanges = GEODIFF_changesCount( testContext(), changesetAB.c_str() );
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
  if ( GEODIFF_applyChangeset( testContext(), patchedAB.c_str(), changesetAB.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_applyChangeset A -> AB" << std::endl;
    return false;
  }

  // check that then new data has both edits
  if ( GEODIFF_createChangeset( testContext(), base.c_str(), patchedAB.c_str(), changesetBbase.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createChangeset B -> base" << std::endl;
    return false;
  }

  nchanges = GEODIFF_changesCount( testContext(), changesetBbase.c_str() );
  if ( nchanges != expected_changes_XB )
  {
    std::cout << "err GEODIFF_listChanges B->base: " << nchanges << " expected: " << expected_changes_XB << " in " << changesetBbase << std::endl;
    return false;
  }

  // check that it equals expected result
  std::cout << "final file: " << patchedAB << std::endl;
  if ( !equals( patchedAB, expected_patchedAB, ignore_timestamp_change ) )
  {
    std::cout << "err equals A" << std::endl;
    return false;
  }

  // now check that we get same result in case of direct rebase
  filecopy( patchedAB_2, modifiedB );
  if ( GEODIFF_rebase( testContext(), base.c_str(), modifiedA.c_str(), patchedAB_2.c_str(), conflict2.c_str() ) != GEODIFF_SUCCESS )
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

bool _test_createRebasedChangesetEx(
  const std::string &testName,
  const std::string &testBaseDb,
  const std::string &diffOur,
  const std::string &diffTheir,
  const std::string &expectedDiffOurRebased,
  const std::string &expectedConflictFile
)
{
  makedir( pathjoin( tmpdir(), testName ) );

  std::string diffOurRebased = pathjoin( tmpdir(), testName, "rebased.diff" );
  std::string conflictFile = pathjoin( tmpdir(), testName, "conflicts.json" );
  int res = GEODIFF_createRebasedChangesetEx( testContext(), "sqlite", "", testBaseDb.c_str(), diffOur.c_str(), diffTheir.c_str(), diffOurRebased.c_str(), conflictFile.c_str() );
  if ( res != GEODIFF_SUCCESS )
  {
    std::cerr << "err GEODIFF_createRebasedChangesetEx" << std::endl;
    return false;
  }

  // check rebased diff equality
  if ( !compareDiffsByContent( diffOurRebased, expectedDiffOurRebased ) )
  {
    std::cerr << "err rebased diff is not equal to the expected diff" << std::endl;
    return false;
  }

  // check conflict equality
  if ( !expectedConflictFile.empty() )
  {
    std::ifstream cf( conflictFile );
    nlohmann::json j_conflict = nlohmann::json::parse( cf );

    std::ifstream ef( expectedConflictFile );
    nlohmann::json j_expected = nlohmann::json::parse( ef );

    if ( j_conflict != j_expected )
    {
      std::cerr << "err conflict file is not equal to the expected conflict file" << std::endl;
      return false;
    }
  }
  else
  {
    // it is expected that no conflict file would be created...
    if ( fileexists( conflictFile ) )
    {
      std::cerr << "err conflict file should not be created, but it got created" << std::endl;
      return false;
    }
  }

  return true;
}

bool _test_expect_not_implemented(
  const std::string &baseX,
  const std::string &testname,
  const std::string &A,
  const std::string &B,
  int expected_changes_A
)
{
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), baseX );
  std::string modifiedA = pathjoin( testdir(), testname, A );
  std::string modifiedB = pathjoin( testdir(), testname, B );
  std::string changesetbaseA = pathjoin( tmpdir(), testname, "changeset_base_to_A.bin" );
  std::string changesetAB = pathjoin( tmpdir(), testname, "changeset_A_to_B.bin" );
  std::string conflictAB = pathjoin( tmpdir(), testname, "conflict_A_to_B.json" );


  // create changeset base to A
  if ( GEODIFF_createChangeset( testContext(), base.c_str(), modifiedA.c_str(), changesetbaseA.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createChangeset A" << std::endl;
    return false;
  }

  int nchanges = GEODIFF_changesCount( testContext(), changesetbaseA.c_str() );
  if ( nchanges != expected_changes_A )
  {
    std::cout << "err GEODIFF_listChanges A: " << nchanges << std::endl;
    return false;
  }

  // create changeset A to B -- EXPECT ERROR!
  if ( GEODIFF_createRebasedChangeset( testContext(), base.c_str(), modifiedB.c_str(), changesetbaseA.c_str(), changesetAB.c_str(), conflictAB.c_str() ) == GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createRebasedChangeset AB" << std::endl;
    return false;
  }
  return true;
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
               1,
               1,
               2,
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
               1,
               1,
               1,
               1
             );
  ASSERT_TRUE( ret );
}

TEST( ConcurrentCommitsSqlite3Test, test_2_edits_2 )
{
  std::cout << "geopackage 2 concurent UPDATES (base) -> (A) and (base) -> (B)" << std::endl;
  std::cout << "both (A) and (B) are (base) edit feature 2, but each edit different" << std::endl;
  std::cout << "attribute: (A) updates 'name' while (B) updates 'rating'" << std::endl;
  std::cout << "expected result: feature 2 has both 'name' from (A) and 'rating' from (B) and no conflicts" << std::endl;

  bool ret = _test(
               "base.gpkg",
               "2_updates_2",
               "updated_A.gpkg",
               "updated_B.gpkg",
               "merged_A_B.gpkg",
               1,
               1,
               1,
               0
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
               1,
               0,
               1,
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
               1,
               0,
               1,
               0
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
               1,
               1,
               1,
               0
             );
  ASSERT_TRUE( ret );
}

TEST( ConcurrentCommitsSqlite3Test, test_update_extent )
{
  std::cout << "the base2theirs contains only a change to \"simple\" table, ";
  std::cout << "but base2modified contains an update to a gpkg_contents table (extent)" << std::endl;
  std::cout << "https://github.com/merginmaps/geodiff/issues/30" << std::endl;

  bool ret = _test(
               "base.gpkg",
               "updates_to_different_tables",
               "add_1.gpkg",
               "add_1_outside_extent.gpkg",
               "final_extent.gpkg",
               1,
               1,
               2,
               0
             );
  ASSERT_TRUE( ret );
}

TEST( ConcurrentCommitsSqlite3Test, test_update_2_different_tables )
{
  std::cout << "both concurrent changes different tables" << std::endl;
  std::cout << "https://github.com/merginmaps/geodiff/issues/29" << std::endl;

  bool ret = _test(
               "base3.gpkg",
               "updates_to_different_tables",
               "add_point.gpkg",
               "add_line.gpkg",
               "final_add_line.gpkg",
               1,
               1,
               2,
               0
             );
  ASSERT_TRUE( ret );

  ret = _test(
          "base3.gpkg",
          "updates_to_different_tables",
          "add_point.gpkg",
          "modify_line.gpkg",
          "final_modify_line.gpkg",
          1,
          2,
          3,
          0
        );
  ASSERT_TRUE( ret );

  ret = _test(
          "base3.gpkg",
          "updates_to_different_tables",
          "add_point.gpkg",
          "remove_line.gpkg",
          "final_delete_line.gpkg",
          1,
          1,
          2,
          0
        );
  ASSERT_TRUE( ret );
}

TEST( ConcurrentCommitsSqlite3Test, test_insert_multiple )
{
  std::cout << "both concurrent insert rows, e.g. A adds 4,5, B adds fids 4,5,6, " << std::endl;
  std::cout << "when B is rebasing on top of A, we should get fids 6,7,8 in rebased B" << std::endl;
  std::cout << "https://github.com/merginmaps/geodiff/issues/62" << std::endl;

  bool ret = _test(
               "base.gpkg",
               "insert_multiple",
               "a_4_5.gpkg",
               "b_4_5_6.gpkg",
               "ab_rebased.gpkg",
               2,  // expected_changes_A
               3,  // expected_changes_AB
               5,  // expected_changes_XB
               0   // expected_conflicts
             );
  ASSERT_TRUE( ret );

  bool ret2 = _test(
                "base.gpkg",
                "insert_multiple",
                "a_4_5.gpkg",
                "c_4_5_6_7_8.gpkg",
                "ac_rebased.gpkg",
                2,  // expected_changes_A
                5,  // expected_changes_AB
                7,  // expected_changes_XB
                0   // expected_conflicts
              );
  ASSERT_TRUE( ret2 );
}

TEST( ConcurrentCommitsSqlite3Test, test_fk_2_updates )
{
  std::cout << "new tree specie & tree is added on both A and B" << std::endl;
  std::cout << "https://github.com/merginmaps/geodiff/issues/39" << std::endl;

  bool ret = _test_expect_not_implemented(
               "base_fk.gpkg",
               "fk_2_updates",
               "modified_fk_A.gpkg",
               "modified_fk_B.gpkg",
               2
             );
  ASSERT_TRUE( ret );

  std::cout << "new tree specie & tree is added on A and just new tree on B" << std::endl;
  std::cout << "https://github.com/merginmaps/geodiff/issues/39" << std::endl;
  ret = _test_expect_not_implemented(
          "base_fk.gpkg",
          "fk_2_updates",
          "modified_fk_A.gpkg",
          "modified_fk_only_new_tree.gpkg",
          2
        );
  ASSERT_TRUE( ret );

  std::cout << "new tree specie & tree is added on B and just new tree on A" << std::endl;
  std::cout << "https://github.com/merginmaps/geodiff/issues/39" << std::endl;
  ret = _test_expect_not_implemented(
          "base_fk.gpkg",
          "fk_2_updates",
          "modified_fk_only_new_tree.gpkg",
          "modified_fk_A.gpkg",
          6
        );
  ASSERT_TRUE( ret );
}

TEST( ConcurrentCommitsSqlite3Test, test_conflict )
{
  std::cout << "test that the conflict is raised when different base is used" << std::endl;
  makedir( pathjoin( tmpdir(), "test_conflict" ) );

  std::string base = pathjoin( testdir(), "base.gpkg" );
  std::string modifiedA = pathjoin( testdir(), "2_updates/updated_A.gpkg" );
  std::string modifiedB = pathjoin( testdir(), "2_updates/updated_B.gpkg" );
  std::string baseB = pathjoin( tmpdir(), "test_conflict/baseB.gpkg" );
  std::string changesetbaseA = pathjoin( tmpdir(), "test_conflict/changeset_base_to_A.bin" );

  filecopy( baseB, modifiedB );

  // create changeset base to A
  ASSERT_TRUE( GEODIFF_createChangeset( testContext(), base.c_str(), modifiedA.c_str(), changesetbaseA.c_str() ) == GEODIFF_SUCCESS );


  // use modifiedC as base --> conflict
  ASSERT_TRUE( GEODIFF_applyChangeset( testContext(), baseB.c_str(), changesetbaseA.c_str() ) != GEODIFF_SUCCESS );
}

TEST( ConcurrentCommitsSqlite3Test, test_rebase_conflict )
{
  // Check various scenarios where a single row of a table is updated by different users
  // and verify that rebased diffs and conflict files are produced correctly.
  // All tests are done on a single row of table "simple" at fid=2

  // CASE 1: change of one column, each diff different column:
  // input:
  // - A: column "name": "feature2" -> "feature222"
  // - B: column "rating": 2 -> 222
  // output:
  // - A rebased on top of B: column "name": "feature2" -> "feature222"
  // - no conflict file

  bool res1 = _test_createRebasedChangesetEx( "test_rebase_conflict_case1",
              pathjoin( testdir(), "base.gpkg" ),
              pathjoin( testdir(), "rebase_conflict", "case1a.diff" ),
              pathjoin( testdir(), "rebase_conflict", "case1b.diff" ),
              pathjoin( testdir(), "rebase_conflict", "case1a-rebased.diff" ),
              std::string() );  // no conflict file
  ASSERT_TRUE( res1 );

  // CASE 2: change of two columns, both to the same value
  // input:
  // - A: column "name": "feature2" -> "feature222" and column "rating": 2 -> 222
  // - B: column "name": "feature2" -> "feature222" and column "rating": 2 -> 222
  // output:
  // - A rebased on top of B: empty changeset
  // - no conflict file

  bool res2 = _test_createRebasedChangesetEx( "test_rebase_conflict_case2",
              pathjoin( testdir(), "base.gpkg" ),
              pathjoin( testdir(), "rebase_conflict", "case2a.diff" ),
              pathjoin( testdir(), "rebase_conflict", "case2b.diff" ),
              pathjoin( testdir(), "rebase_conflict", "case2a-rebased.diff" ),
              std::string() );  // no conflict file
  ASSERT_TRUE( res2 );

  // CASE 3: change of two columns, both to different values
  // input:
  // - A: column "name": "feature2" -> "feature2A" and column "rating": 2 -> 20
  // - B: column "name": "feature2" -> "feature2B" and column "rating": 2 -> 21
  // output:
  // - A rebased on top of B: column "name": "feature2B" -> "feature2A" and column "rating": 21 -> 20
  // - conflict file with an entry for both "name" and "rating" columns

  bool res3 = _test_createRebasedChangesetEx( "test_rebase_conflict_case3",
              pathjoin( testdir(), "base.gpkg" ),
              pathjoin( testdir(), "rebase_conflict", "case3a.diff" ),
              pathjoin( testdir(), "rebase_conflict", "case3b.diff" ),
              pathjoin( testdir(), "rebase_conflict", "case3a-rebased.diff" ),
              pathjoin( testdir(), "rebase_conflict", "case3a-rebased.conflicts" ) );
  ASSERT_TRUE( res3 );

  // CASE 4: change of two columns, one to the same value, one to other value
  // input:
  // - A: column "name": "feature2" -> "feature2A" and column "rating": 2 -> 222
  // - B: column "name": "feature2" -> "feature2B" and column "rating": 2 -> 222
  // output:
  // - A rebased on top of B: column "name": "feature2B" -> "feature2A"
  // - conflict file with an entry for column "name"

  bool res4 = _test_createRebasedChangesetEx( "test_rebase_conflict_case4",
              pathjoin( testdir(), "base.gpkg" ),
              pathjoin( testdir(), "rebase_conflict", "case4a.diff" ),
              pathjoin( testdir(), "rebase_conflict", "case4b.diff" ),
              pathjoin( testdir(), "rebase_conflict", "case4a-rebased.diff" ),
              pathjoin( testdir(), "rebase_conflict", "case4a-rebased.conflicts" ) );
  ASSERT_TRUE( res4 );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
