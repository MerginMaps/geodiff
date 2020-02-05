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
  int nConflicts;
  if ( GEODIFF_rebase( base.c_str(), base.c_str(), patched2.c_str(), conflict.c_str(), &nConflicts ) != GEODIFF_SUCCESS )
  {
    std::cout << "err GEODIFF_rebase inversed" << std::endl;
    return false;
  }

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

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
