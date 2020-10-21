/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"

bool _test(
  const std::string &testname,
  const std::string &basename,
  const std::string &modifiedname,
  int expected_changes,
  bool ignore_timestamp_change = false
)
{
  std::cout << testname << std::endl;
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), basename );
  std::string modified = pathjoin( testdir(), modifiedname );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.bin" );
  std::string changeset_inv = pathjoin( tmpdir(), testname, "changeset_inv.bin" );
  std::string patched = pathjoin( tmpdir(), testname, "patched.gpkg" );
  std::string patched2 = pathjoin( tmpdir(), testname, "patched2.gpkg" );
  std::string conflict = pathjoin( tmpdir(), testname, "conflict.json" );
  std::string json = pathjoin( tmpdir(), testname, testname + ".json" );
  std::string json_summary = pathjoin( tmpdir(), testname, testname + "_summary.json" );

  if ( GEODIFF_createChangeset( base.c_str(), modified.c_str(), changeset.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_createChangeset" << std::endl;
    return false;
  }

  int nchanges = GEODIFF_changesCount( changeset.c_str() );
  if ( nchanges != expected_changes )
  {
    std::cout << "err GEODIFF_listChanges " <<  nchanges << " vs " << expected_changes << std::endl;
    return false;
  }

  filecopy( patched, base );
  if ( GEODIFF_applyChangeset( patched.c_str(), changeset.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_applyChangeset" << std::endl;
    return false;
  }

  // check that now it is same file with modified
  if ( !equals( patched, modified, ignore_timestamp_change ) )
  {
    std::cout << "err equals" << std::endl;
    return false;
  }

  // create inversed changeset
  if ( GEODIFF_invertChangeset( changeset.c_str(), changeset_inv.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_invertChangeset" << std::endl;
    return false;
  }

  // apply inversed changeset
  if ( GEODIFF_applyChangeset( patched.c_str(), changeset_inv.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_applyChangeset inversed" << std::endl;
    return false;
  }

  // check that now it is same file with base
  if ( !equals( patched, base, ignore_timestamp_change ) )
  {
    std::cout << "err equals" << std::endl;
    return false;
  }

  // check that direct rebase works
  filecopy( patched2, modified );
  if ( GEODIFF_rebase( base.c_str(), base.c_str(), patched2.c_str(), conflict.c_str() ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_rebase inversed" << std::endl;
    return false;
  }

  int nConflicts = countConflicts( conflict );
  if ( nConflicts != 0 )
  {
    std::cout << "conflicts found" << std::endl;
    return false;
  }

  if ( !equals( patched2, modified, ignore_timestamp_change ) )
  {
    std::cout << "err equals" << std::endl;
    return false;
  }

  printJSON( changeset, json, json_summary );

  return true;
}


TEST( SingleCommitSqlite3Test, test_sqlite_no_gis )
{
  std::cout << "sqlite 2 updated 1 added 1 deleted" << std::endl;
  bool ret = _test( "pure_sqlite",
                    "base.sqlite",
                    pathjoin( "pure_sqlite", "modified_base.sqlite" ),
                    4
                  );
  ASSERT_TRUE( ret );
}

TEST( SingleCommitSqlite3Test, geopackage )
{
  std::cout << "geopackage 1 updated geometry" << std::endl;
  bool ret = _test( "1_geopackage",
                    "base.gpkg",
                    pathjoin( "1_geopackage", "modified_1_geom.gpkg" ),
                    2
                  );
  ASSERT_TRUE( ret );
}

TEST( SingleCommitSqlite3Test, geopackage_complex )
{
  std::cout << "geopackage 2 new, 1 move, 1 changed attr, 1 delete" << std::endl;
  bool ret = _test( "complex",
                    "base.gpkg",
                    pathjoin( "complex", "complex1.gpkg" ),
                    7
                  );
  ASSERT_TRUE( ret );
}

TEST( SingleCommitSqlite3Test, retype_attribute )
{
  std::cout << "geopackage attribute count is same, have same name, but different type" << std::endl;
  bool ret = _test( "retype_attribute",
                    pathjoin( "modified_scheme", "added_attribute.gpkg" ),
                    pathjoin( "modified_scheme", "added_attribute_different_type.gpkg" ),
                    4
                  );
  ASSERT_TRUE( ret );
}

TEST( SingleCommitSqlite3Test, reprojected )
{
  std::cout << "geopackage change of crs" << std::endl;
  bool ret = _test( "reprojected",
                    pathjoin( "modified_scheme", "reprojected.gpkg" ),
                    pathjoin( "modified_scheme", "reprojected2.gpkg" ),
                    6
                  );
  ASSERT_TRUE( ret );
}

TEST( SingleCommitSqlite3Test, SingleCommitFkTest )
{
  std::cout << "database with foreign keys" << std::endl;
  bool ret = _test( "fk_1_update",
                    "base_fk.gpkg",
                    pathjoin( "fk_1_update", "modified_fk.gpkg" ),
                    5
                  );
  ASSERT_TRUE( ret );
}

TEST( SingleCommitSqlite3Test, GpkgTriggersTest )
{
  std::cout << "geopackage with many gpkg_ triggers" << std::endl;

  bool ret1 = _test( "gpkg_triggers",
                     pathjoin( "gpkg_triggers", "db-base.gpkg" ),
                     pathjoin( "gpkg_triggers", "db-modified.gpkg" ),
                     GEODIFF_changesCount( pathjoin( testdir(), "gpkg_triggers", "modified-changeset.diff" ).c_str() )
                   );

  bool ret2 = GEODIFF_createRebasedChangeset(
                pathjoin( testdir(), "gpkg_triggers", "db-base.gpkg" ).c_str(),
                pathjoin( testdir(), "gpkg_triggers", "db-modified.gpkg" ).c_str(),
                pathjoin( testdir(), "gpkg_triggers", "modified-changeset.diff" ).c_str(),
                pathjoin( testdir(), "gpkg_triggers", "res.diff" ).c_str(),
                pathjoin( testdir(), "gpkg_triggers", "res.conflict" ).c_str()
              ) == GEODIFF_SUCCESS;

  ASSERT_TRUE( ret1 && ret2 );

  // remove created diff and conflict files
  remove( pathjoin( testdir(), "gpkg_triggers", "res.diff" ).c_str() );
  remove( pathjoin( testdir(), "gpkg_triggers", "res.conflict" ).c_str() );
}

TEST( SingleCommitSqlite3Test, NonAsciiCharactersTest )
{
  std::cout << "non ascii characters in path test" << std::endl;

  bool ret = _test( "non_ascii_\xc5\xa1", // add special sign also here, because changeset file is created from it
                    pathjoin( "utf_test_\xc5\xa1\xc4\x8d\xc3\xa9", "test\xc3\xa1\xc3\xa1.gpkg" ), // testaa
                    pathjoin( "utf_test_\xc5\xa1\xc4\x8d\xc3\xa9", "test\xc4\x8d\xc4\x8d.gpkg" ), // testcc
                    3
                  );
  ASSERT_TRUE( ret );
}

TEST( SingleCommitSqlite3Test, QuoteCharacterGpkgName )
{
    std::cout << "path with quote character" << std::endl;
    bool ret = _test( "quote's test",
                      pathjoin( "dir_with_quote's's", "base.gpkg" ),
                      pathjoin( "dir_with_quote's's", "recreated.gpkg" ),
                      9 
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
