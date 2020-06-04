/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"

#include "changesetreader.h"
#include "changesetwriter.h"
#include "driver.h"


static void testCreateChangeset( const std::string &testname, const std::string &fileBase, const std::string &fileModified, const std::string &fileExpected )
{
  makedir( pathjoin( tmpdir(), testname ) );
  std::string fileOutput = pathjoin( tmpdir(), testname, "output.diff" );

  std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
  driver->open( Driver::sqliteParameters( fileBase, fileModified ) );

  {
    ChangesetWriter writer;
    bool res = writer.open( fileOutput );
    ASSERT_TRUE( res );
    driver->createChangeset( writer );
  }

  ASSERT_TRUE( fileContentEquals( fileOutput, fileExpected ) );
}


static void testApplyChangeset( const std::string &testname, const std::string &fileBase, const std::string &fileChangeset, const std::string &fileExpected )
{
  makedir( pathjoin( tmpdir(), testname ) );
  std::string testdb = pathjoin( tmpdir(), testname, "output.gpkg" );
  filecopy( testdb, fileBase );

  std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
  driver->open( Driver::sqliteParametersSingleSource( testdb ) );

  {
    ChangesetReader reader;
    bool res = reader.open( fileChangeset );
    ASSERT_TRUE( res );
    driver->applyChangeset( reader );
  }

  ASSERT_TRUE( equals( testdb, fileExpected ) );
}


//


TEST( SqliteDriverTest, test_basic )
{
  std::vector<std::string> driverNames = Driver::drivers();
  EXPECT_TRUE( std::find( driverNames.begin(), driverNames.end(), "sqlite" ) != driverNames.end() );

  std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
  ASSERT_TRUE( driver );
  driver->open( Driver::sqliteParametersSingleSource( pathjoin( testdir(), "base.gpkg" ) ) );

  std::vector<std::string> tableNames = driver->listTables();
  EXPECT_EQ( tableNames.size(), 7 );
  ASSERT_TRUE( std::find( tableNames.begin(), tableNames.end(), "simple" ) != tableNames.end() );

  TableSchema tbl = driver->tableSchema( "simple" );
  EXPECT_EQ( tbl.columns.size(), 4 );
  EXPECT_EQ( tbl.columns[0].name, "fid" );
  EXPECT_EQ( tbl.columns[1].name, "geometry" );
  EXPECT_EQ( tbl.columns[2].name, "name" );
  EXPECT_EQ( tbl.columns[3].name, "rating" );
  EXPECT_EQ( tbl.columns[0].isPrimaryKey, true );
  EXPECT_EQ( tbl.columns[1].isPrimaryKey, false );
  EXPECT_EQ( tbl.columns[2].isPrimaryKey, false );
  EXPECT_EQ( tbl.columns[3].isPrimaryKey, false );
}

TEST( SqliteDriverTest, test_open )
{
  std::map<std::string, std::string> conn;

  {
    std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
    EXPECT_ANY_THROW( driver->open( conn ) );
  }

  conn["base"] = "invalid_file";
  {
    std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
    EXPECT_ANY_THROW( driver->open( conn ) );
  }

  conn["base"] = pathjoin( testdir(), "base.gpkg" );
  {
    std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
    EXPECT_NO_THROW( driver->open( conn ) );
  }

  conn["modified"] = "invalid_file";
  {
    std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
    EXPECT_ANY_THROW( driver->open( conn ) );
  }

  conn["modified"] = pathjoin( testdir(), "base.gpkg" );
  {
    std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
    EXPECT_NO_THROW( driver->open( conn ) );
  }
}


//


TEST( SqliteDriverTest, create_changeset_insert )
{
  testCreateChangeset( "test_create_changeset_insert",
                       pathjoin( testdir(), "base.gpkg" ),
                       pathjoin( testdir(), "2_inserts", "inserted_1_A.gpkg" ),
                       pathjoin( testdir(), "2_inserts", "base-inserted_1_A.diff" )
                     );
}

TEST( SqliteDriverTest, test_changeset_update )
{
  testCreateChangeset( "test_create_changeset_update",
                       pathjoin( testdir(), "base.gpkg" ),
                       pathjoin( testdir(), "2_updates", "updated_A.gpkg" ),
                       pathjoin( testdir(), "2_updates", "base-updated_A.diff" )
                     );
}

TEST( SqliteDriverTest, create_changeset_delete )
{
  testCreateChangeset( "test_create_changeset_insert",
                       pathjoin( testdir(), "base.gpkg" ),
                       pathjoin( testdir(), "2_deletes", "deleted_A.gpkg" ),
                       pathjoin( testdir(), "2_deletes", "base-deleted_A.diff" )
                     );
}


//


TEST( SqliteDriverTest, apply_changeset_insert )
{
  testApplyChangeset( "test_apply_changeset_insert",
                      pathjoin( testdir(), "base.gpkg" ),
                      pathjoin( testdir(), "2_inserts", "base-inserted_1_A.diff" ),
                      pathjoin( testdir(), "2_inserts", "inserted_1_A.gpkg" )
                    );
}

TEST( SqliteDriverTest, apply_changeset_update )
{
  testApplyChangeset( "test_create_changeset_update",
                      pathjoin( testdir(), "base.gpkg" ),
                      pathjoin( testdir(), "2_updates", "base-updated_A.diff" ),
                      pathjoin( testdir(), "2_updates", "updated_A.gpkg" )
                    );

}

TEST( SqliteDriverTest, apply_changeset_delete )
{
  testApplyChangeset( "test_apply_changeset_delete",
                      pathjoin( testdir(), "base.gpkg" ),
                      pathjoin( testdir(), "2_deletes", "base-deleted_A.diff" ),
                      pathjoin( testdir(), "2_deletes", "deleted_A.gpkg" )
                    );
}



int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
