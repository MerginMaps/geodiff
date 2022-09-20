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
#include "sqliteutils.h"
#include "geodiffutils.hpp"

static void testCreateChangeset( const std::string &testname, const std::string &fileBase, const std::string &fileModified, const std::string &fileExpected )
{
  makedir( pathjoin( tmpdir(), testname ) );
  std::string fileOutput = pathjoin( tmpdir(), testname, "output.diff" );

  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
  driver->open( Driver::sqliteParameters( fileBase, fileModified ) );

  {
    ChangesetWriter writer;
    ASSERT_NO_THROW( writer.open( fileOutput ) );
    driver->createChangeset( writer );
  }

  ASSERT_TRUE( fileContentEquals( fileOutput, fileExpected ) );
}


static void testApplyChangeset( const std::string &testname, const std::string &fileBase, const std::string &fileChangeset, const std::string &fileExpected )
{
  makedir( pathjoin( tmpdir(), testname ) );
  std::string testdb = pathjoin( tmpdir(), testname, "output.gpkg" );
  filecopy( testdb, fileBase );

  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
  driver->open( Driver::sqliteParametersSingleSource( testdb ) );

  {
    ChangesetReader reader;
    ASSERT_TRUE( reader.open( fileChangeset ) );
    driver->applyChangeset( reader );
  }

  ASSERT_TRUE( equals( testdb, fileExpected ) );
}


//


TEST( SqliteDriverTest, test_basic )
{
  std::vector<std::string> driverNames = Driver::drivers();
  EXPECT_TRUE( std::find( driverNames.begin(), driverNames.end(), "sqlite" ) != driverNames.end() );

  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
  ASSERT_TRUE( driver );
  driver->open( Driver::sqliteParametersSingleSource( pathjoin( testdir(), "base.gpkg" ) ) );

  std::vector<std::string> tableNames = driver->listTables();
  EXPECT_EQ( tableNames.size(), 1 );
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
    std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
    EXPECT_ANY_THROW( driver->open( conn ) );
  }

  conn["base"] = "invalid_file";
  {
    std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
    EXPECT_ANY_THROW( driver->open( conn ) );
  }

  conn["base"] = pathjoin( testdir(), "base.gpkg" );
  {
    std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
    EXPECT_NO_THROW( driver->open( conn ) );
  }

  conn["modified"] = "invalid_file";
  {
    std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
    EXPECT_ANY_THROW( driver->open( conn ) );
  }

  conn["modified"] = pathjoin( testdir(), "base.gpkg" );
  {
    std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
    EXPECT_NO_THROW( driver->open( conn ) );
  }
}


//
TEST( SqliteDriverApi, test_driver_sqlite_api )
{
  int ndrivers = 1;
#ifdef HAVE_POSTGRES
  ++ndrivers;
#endif

  EXPECT_EQ( GEODIFF_driverCount( testContext() ), ndrivers );

  char driverName[256];
  EXPECT_EQ( GEODIFF_driverNameFromIndex( testContext(), 0, driverName ), GEODIFF_SUCCESS );
  EXPECT_EQ( GEODIFF_driverNameFromIndex( testContext(), 99, driverName ), GEODIFF_ERROR );

  EXPECT_EQ( std::string( driverName ), "sqlite" );

  EXPECT_TRUE( GEODIFF_driverIsRegistered( testContext(), "sqlite" ) );
}

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

TEST( SqliteDriverTest, apply_changeset_trigger_with_spaces )
{
  // the testing database is just like base.gpkg, but it has an extra dummy trigger:
  // CREATE TRIGGER "trigger with space" AFTER DELETE ON simple BEGIN DELETE FROM simple; END;
  testApplyChangeset( "test_apply_changeset_trigger_with_spaces",
                      pathjoin( testdir(), "quoting", "trigger-with-space.gpkg" ),
                      pathjoin( testdir(), "1_geopackage", "base-modified_1_geom.diff" ),
                      pathjoin( testdir(), "1_geopackage", "modified_1_geom.gpkg" )
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

  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
  driver->open( Driver::sqliteParametersSingleSource( testdb ) );

  {
    ChangesetReader reader;
    ASSERT_TRUE( reader.open( fileChangeset ) );
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
  std::unique_ptr<Driver> driverBase( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
  EXPECT_NO_THROW( driverBase->open( connBase ) );
  TableSchema tblBaseSimple = driverBase->tableSchema( "simple" );

  // create the new database
  std::map<std::string, std::string> conn;
  conn["base"] = testdb;
  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
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

TEST( SqliteDriverTest, create_changeset_datetime )
{
  testCreateChangeset( "test_create_changeset_datetime",
                       pathjoin( testdir(), "datetime", "datetime1.gpkg" ),
                       pathjoin( testdir(), "datetime", "datetime2.gpkg" ),
                       pathjoin( testdir(), "datetime", "datetime1-2.diff" )
                     );
}


TEST( SqliteDriverTest, apply_changeset_datetime )
{
  // check that the datetime handling is robust - a single datetime may have
  // multiple representations, e.g. '2021-03-16T00:00:00' and '2021-03-16T00:00:00Z'
  // represent the same datetime, so things should work fine even if database
  // contains '2021-03-16T00:00:00' but changeset refers to '2021-03-16T00:00:00Z'

  // datetime1-3a.diff refers to date/time value in deleted row as '2021-04-01T15:00:00Z'
  // while datetime1.gpkg has the value stored as '2021-04-01 15:00:00'
  testApplyChangeset( "test_apply_changeset_datetime_delete",
                      pathjoin( testdir(), "datetime", "datetime1.gpkg" ),
                      pathjoin( testdir(), "datetime", "datetime1-3a.diff" ),
                      pathjoin( testdir(), "datetime", "datetime3.gpkg" )
                    );

  // datetime1a.gpkg has the same content as datetime1, but date/time values
  // are represented as '2021-04-01T15:00:00Z' instead of '2021-04-01 15:00:00'
  // (which is used in datetime1.gpkg and in datetime1-2.diff)
  testApplyChangeset( "test_apply_changeset_datetime_update",
                      pathjoin( testdir(), "datetime", "datetime1a.gpkg" ),
                      pathjoin( testdir(), "datetime", "datetime1-2.diff" ),
                      pathjoin( testdir(), "datetime", "datetime2.gpkg" )
                    );
}

TEST( SqliteDriverTest, test_timestamp_miliseconds )
{
  testCreateChangeset( "test_create_changeset_miliseconds",
                       pathjoin( testdir(), "datetime", "datetime1-ms.gpkg" ),
                       pathjoin( testdir(), "datetime", "datetime2-ms.gpkg" ),
                       pathjoin( testdir(), "datetime", "datetime1-2-ms.diff" )
                     );

  testApplyChangeset( "test_apply_changeset_miliseconds",
                      pathjoin( testdir(), "datetime", "datetime1-ms.gpkg" ),
                      pathjoin( testdir(), "datetime", "datetime1-2-ms.diff" ),
                      pathjoin( testdir(), "datetime", "datetime2-ms.gpkg" )
                    );
}

TEST( SqliteDriverTest, apply_with_gpkg_contents )
{
  // In geodiff >= 1.0 we ignore gpkg_* metadata tables. However older geodiff
  // releases may still include changes in these tables when creating changesets
  // so we need to make sure we ignore them when applying changesets.

  // Test diff contains a change in gpkg_contents where the "old" value of last
  // modified column does not match the value in base.gpkg, yet it should not fail
  // if the gpkg_contents changes are ignored from the diff.

  std::string testname = "apply_with_gpkg_contents";
  std::string fileBase = pathjoin( testdir(), "base.gpkg" );
  std::string fileChangeset = pathjoin( testdir(), "apply_with_gpkg_contents", "base-gpkg-contents-conflict.diff" );

  makedir( pathjoin( tmpdir(), testname ) );
  std::string testdb = pathjoin( tmpdir(), testname, "output.gpkg" );
  filecopy( testdb, fileBase );

  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
  driver->open( Driver::sqliteParametersSingleSource( testdb ) );

  {
    ChangesetReader reader;
    ASSERT_TRUE( reader.open( fileChangeset ) );
    EXPECT_NO_THROW( driver->applyChangeset( reader ) );
  }

  ASSERT_TRUE( equals( testdb, pathjoin( testdir(), "1_geopackage", "modified_1_geom.gpkg" ) ) );
}

TEST( SqliteDriverTest, make_copy_sqlite )
{
  // invalid inputs
  EXPECT_EQ( GEODIFF_makeCopySqlite( testContext(), "xxx", nullptr ), GEODIFF_ERROR );
  EXPECT_EQ( GEODIFF_makeCopySqlite( testContext(), nullptr, "yyy" ), GEODIFF_ERROR );
  EXPECT_EQ( GEODIFF_makeCopySqlite( testContext(), "xxx", "yyy" ), GEODIFF_ERROR );

  std::string base = pathjoin( testdir(), "base.gpkg" );

  std::string testname = "test_make_copy_sqlite";
  makedir( pathjoin( tmpdir(), testname ) );
  std::string testdb = pathjoin( tmpdir(), testname, "output.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "output.diff" );

  // test with valid inputs and check whether the output has the same content as the original database
  ASSERT_EQ( GEODIFF_makeCopySqlite( testContext(), base.data(), testdb.data() ), GEODIFF_SUCCESS );
  EXPECT_EQ( GEODIFF_createChangeset( testContext(), base.data(), testdb.data(), changeset.data() ), GEODIFF_SUCCESS );
  EXPECT_FALSE( GEODIFF_hasChanges( testContext(), changeset.data() ) );

  // test overwrite: we will do backup into a different database that already exists
  // and verify that it got update to the content of the source database
  std::string testdb2 = pathjoin( tmpdir(), testname, "output2.gpkg" );
  std::string changeset2 = pathjoin( tmpdir(), testname, "output2.diff" );
  filecopy( testdb2.data(), pathjoin( testdir(), "1_geopackage", "modified_1_geom.gpkg" ) );

  EXPECT_EQ( GEODIFF_createChangeset( testContext(), base.data(), testdb2.data(), changeset2.data() ), GEODIFF_SUCCESS );
  EXPECT_TRUE( GEODIFF_hasChanges( testContext(), changeset2.data() ) );

  ASSERT_EQ( GEODIFF_makeCopySqlite( testContext(), base.data(), testdb2.data() ), GEODIFF_SUCCESS );

  EXPECT_EQ( GEODIFF_createChangeset( testContext(), base.data(), testdb2.data(), changeset2.data() ), GEODIFF_SUCCESS );
  EXPECT_FALSE( GEODIFF_hasChanges( testContext(), changeset2.data() ) );

  // test overwrite of a file that is not SQLite database
  std::string testdb3 = pathjoin( tmpdir(), testname, "output3.gpkg" );
  std::string changeset3 = pathjoin( tmpdir(), testname, "output3.diff" );
  {
    std::ofstream f( testdb3 );
    f << "hello world";
  }
  EXPECT_EQ( GEODIFF_createChangeset( testContext(), base.data(), testdb3.data(), changeset3.data() ), GEODIFF_ERROR );

  ASSERT_EQ( GEODIFF_makeCopySqlite( testContext(), base.data(), testdb3.data() ), GEODIFF_SUCCESS );
  EXPECT_EQ( GEODIFF_createChangeset( testContext(), base.data(), testdb3.data(), changeset3.data() ), GEODIFF_SUCCESS );
  EXPECT_FALSE( GEODIFF_hasChanges( testContext(), changeset3.data() ) );
}


TEST( SqliteDriverTest, make_copy_sqlite_concurrent )
{
  // This will test a database in WAL mode which gets modified, but the changes are not yet flushed back,
  // contrasting "unsafe" copy (simply copying DB file) and "safe" copy using geodiff API

  std::string testname = "test_make_copy_sqlite_concurrent";
  std::string testdb = pathjoin( tmpdir(), testname, "base.gpkg" );
  std::string testdbUnsafeCopy = pathjoin( tmpdir(), testname, "copy-unsafe.gpkg" );
  std::string testdbSafeCopy = pathjoin( tmpdir(), testname, "copy-safe.gpkg" );

  makedir( pathjoin( tmpdir(), testname ) );
  filecopy( testdb, pathjoin( testdir(), "base.gpkg" ) );

  // Make sure DB is in WAL mode. Remove one row - initially there were 3 rows, now there are 2 rows
  Sqlite3Db db;
  db.open( testdb );
  ASSERT_EQ( sqlite3_exec( db.get(), "PRAGMA journal_mode=wal;", nullptr, nullptr, nullptr ), SQLITE_OK );
  ASSERT_EQ( sqlite3_exec( db.get(), "DELETE FROM simple WHERE fid=1;", nullptr, nullptr, nullptr ), SQLITE_OK );
  ASSERT_TRUE( fileexists( pathjoin( tmpdir(), testname, "base.gpkg-wal" ) ) );

  // unsafe copy using regular file copying (changes in WAL but not flushed back should get lost)
  filecopy( testdbUnsafeCopy, testdb );

  // safe copy using SQLite backup API
  ASSERT_EQ( GEODIFF_makeCopySqlite( testContext(), testdb.data(), testdbSafeCopy.data() ), GEODIFF_SUCCESS );

  // unsafe copy still thinks there are 3 rows -> change was lost
  std::shared_ptr<Sqlite3Db> dbUnsafe( new Sqlite3Db );
  dbUnsafe->open( testdbUnsafeCopy );
  Sqlite3Stmt stmtUnsafe;
  stmtUnsafe.prepare( dbUnsafe, "%s", "SELECT count(*) FROM simple" );
  ASSERT_EQ( sqlite3_step( stmtUnsafe.get() ), SQLITE_ROW );
  ASSERT_EQ( sqlite3_column_int( stmtUnsafe.get(), 0 ), 3 );

  // safe copy thinks there are 2 rows -> change got preserved - good!
  std::shared_ptr<Sqlite3Db> dbSafe( new Sqlite3Db );
  dbSafe->open( testdbSafeCopy );
  Sqlite3Stmt stmtSafe;
  stmtSafe.prepare( dbSafe, "%s", "SELECT count(*) FROM simple" );
  ASSERT_EQ( sqlite3_step( stmtSafe.get() ), SQLITE_ROW );
  ASSERT_EQ( sqlite3_column_int( stmtSafe.get(), 0 ), 2 );
}


int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
