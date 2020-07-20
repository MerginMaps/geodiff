/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"
#include "geodiff_config.hpp"

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
  ASSERT_EQ( tbl.name, "simple" );
  EXPECT_EQ( tbl.columns.size(), 4 );
  EXPECT_EQ( tbl.columns[0].name, "fid" );
  EXPECT_EQ( tbl.columns[1].name, "geometry" );
  EXPECT_EQ( tbl.columns[2].name, "name" );
  EXPECT_EQ( tbl.columns[3].name, "rating" );

  EXPECT_EQ( tbl.columns[0].type, "INTEGER" );
  EXPECT_EQ( tbl.columns[1].type, "POINT" );
  EXPECT_EQ( tbl.columns[2].type, "TEXT" );
  EXPECT_EQ( tbl.columns[3].type, "MEDIUMINT" );

  EXPECT_EQ( tbl.columns[0].isPrimaryKey, true );
  EXPECT_EQ( tbl.columns[1].isPrimaryKey, false );
  EXPECT_EQ( tbl.columns[2].isPrimaryKey, false );
  EXPECT_EQ( tbl.columns[3].isPrimaryKey, false );

  EXPECT_EQ( tbl.columns[0].isNotNull, true );
  EXPECT_EQ( tbl.columns[1].isNotNull, false );
  EXPECT_EQ( tbl.columns[2].isNotNull, false );
  EXPECT_EQ( tbl.columns[3].isNotNull, false );

  EXPECT_EQ( tbl.columns[0].isAutoIncrement, true );
  EXPECT_EQ( tbl.columns[1].isAutoIncrement, false );
  EXPECT_EQ( tbl.columns[2].isAutoIncrement, false );
  EXPECT_EQ( tbl.columns[3].isAutoIncrement, false );

  EXPECT_EQ( tbl.columns[0].isGeometry, false );
  EXPECT_EQ( tbl.columns[1].isGeometry, true );
  EXPECT_EQ( tbl.columns[2].isGeometry, false );
  EXPECT_EQ( tbl.columns[3].isGeometry, false );

  EXPECT_EQ( tbl.columns[1].geomType, "POINT" );
  EXPECT_EQ( tbl.columns[1].geomSrsId, 4326 );
  EXPECT_EQ( tbl.columns[1].geomHasZ, false );
  EXPECT_EQ( tbl.columns[1].geomHasM, false );
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

TEST( SqliteDriverTest, apply_changeset_conflict )
{
  // the diff file contains one regular delete and one wrong delete
  // we test that 1. applyChangeset will fail AND 2. the first (regular) delete will be rolled back
  std::string testname = "test_apply_changeset_conflict";
  std::string fileBase = pathjoin( testdir(), "base.gpkg" );
  std::string fileChangeset = pathjoin( testdir(), "conflict", "base-conflict-delete.diff" );

  makedir( pathjoin( tmpdir(), testname ) );
  std::string testdb = pathjoin( tmpdir(), testname, "output.gpkg" );
  filecopy( testdb, fileBase );

  std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
  driver->open( Driver::sqliteParametersSingleSource( testdb ) );

  {
    ChangesetReader reader;
    bool res = reader.open( fileChangeset );
    ASSERT_TRUE( res );
    EXPECT_ANY_THROW( driver->applyChangeset( reader ) );
  }

  ASSERT_TRUE( equals( testdb, fileBase ) );
}


TEST( SqliteDriverTest, test_create_from_gpkg )
{
  std::string testname = "test_create_from_gpkg";
  makedir( pathjoin( tmpdir(), testname ) );
  std::string testdb = pathjoin( tmpdir(), testname, "output.gpkg" );
  std::string dumpfile = pathjoin( tmpdir(), testname, "dump.diff" );

  // get table schema in the base database
  std::map<std::string, std::string> connBase;
  connBase["base"] = pathjoin( testdir(), "base.gpkg" );
  std::unique_ptr<Driver> driverBase( Driver::createDriver( "sqlite" ) );
  EXPECT_NO_THROW( driverBase->open( connBase ) );
  TableSchema tblBaseSimple = driverBase->tableSchema( "simple" );

  // create the new database
  std::map<std::string, std::string> conn;
  conn["base"] = testdb;
  std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
  EXPECT_NO_THROW( driver->create( conn, true ) );
  EXPECT_ANY_THROW( driver->tableSchema( "simple" ) );

  // create table
  std::vector<TableSchema> tables;
  tables.push_back( tblBaseSimple );
  driver->createTables( tables );

  // verify it worked
  EXPECT_NO_THROW( driver->tableSchema( "simple" ) );

  TableSchema tblNewSimple = driver->tableSchema( "simple" );
  EXPECT_EQ( tblBaseSimple.name, tblNewSimple.name );
  EXPECT_EQ( tblBaseSimple.columns.size(), tblNewSimple.columns.size() );
  EXPECT_EQ( tblBaseSimple.columns[0], tblNewSimple.columns[0] );
  EXPECT_EQ( tblBaseSimple.columns[1], tblNewSimple.columns[1] );
  EXPECT_EQ( tblBaseSimple.columns[2], tblNewSimple.columns[2] );
  EXPECT_EQ( tblBaseSimple.columns[3], tblNewSimple.columns[3] );
  EXPECT_EQ( tblBaseSimple.crs.srsId, tblNewSimple.crs.srsId );
  EXPECT_EQ( tblBaseSimple.crs.authName, tblNewSimple.crs.authName );
  EXPECT_EQ( tblBaseSimple.crs.authCode, tblNewSimple.crs.authCode );
  //EXPECT_EQ( tblBaseSimple.crs.wkt, tblNewSimple.crs.wkt );  // WKTs differ in whitespaces
}


int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
