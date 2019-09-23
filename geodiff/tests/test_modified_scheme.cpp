/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"

TEST( ModifiedSchemeSqlite3Test, add_attribute )
{
  std::cout << "geopackage add attribute to table" << std::endl;
  std::string testname = "added_attribute";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "base.gpkg" );
  std::string modified = pathjoin( testdir(), "modified_scheme", "added_attribute.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.bin" );

  ASSERT_EQ( GEODIFF_createChangeset( base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_UNSUPPORTED_CHANGE );
}

TEST( ModifiedSchemeSqlite3Test, add_table )
{
  std::cout << "geopackage add table to table" << std::endl;
  std::string testname = "add_table";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "base.gpkg" );
  std::string modified = pathjoin( testdir(), "modified_scheme", "added_table.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.bin" );

  ASSERT_EQ( GEODIFF_createChangeset( base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_UNSUPPORTED_CHANGE );
}

TEST( ModifiedSchemeSqlite3Test, delete_attribute )
{
  std::cout << "geopackage add attribute to table" << std::endl;
  std::string testname = "delete_attribute";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "modified_scheme", "added_attribute.gpkg" );
  std::string modified = pathjoin( testdir(), "base.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.bin" );

  ASSERT_EQ( GEODIFF_createChangeset( base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_UNSUPPORTED_CHANGE );
}

TEST( ModifiedSchemeSqlite3Test, delete_table )
{
  std::cout << "geopackage delete table" << std::endl;
  std::string testname = "delete_table";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "modified_scheme", "added_table.gpkg" );
  std::string modified = pathjoin( testdir(), "base.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.bin" );

  ASSERT_EQ( GEODIFF_createChangeset( base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_UNSUPPORTED_CHANGE );
}

TEST( ModifiedSchemeSqlite3Test, rename_table )
{
  std::cout << "geopackage table count is same, but tables have different name" << std::endl;
  std::string testname = "delete_table";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "modified_scheme", "added_table.gpkg" );
  std::string modified = pathjoin( testdir(), "modified_scheme", "added_table2.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.bin" );

  ASSERT_EQ( GEODIFF_createChangeset( base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_UNSUPPORTED_CHANGE );
}

TEST( ModifiedSchemeSqlite3Test, rename_attribute )
{
  std::cout << "geopackage attribute count is same, but have different name" << std::endl;
  std::string testname = "rename_attribute";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "modified_scheme", "added_attribute.gpkg" );
  std::string modified = pathjoin( testdir(), "modified_scheme", "added_attribute2.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.bin" );

  ASSERT_EQ( GEODIFF_createChangeset( base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_UNSUPPORTED_CHANGE );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
