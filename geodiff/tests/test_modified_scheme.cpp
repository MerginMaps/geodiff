/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include <filesystem>
#include <memory>

#include "gtest/gtest.h"

#include "changesetreader.h"
#include "changesetutils.h"
#include "changesetwriter.h"
#include "driver.h"
#include "geodiff.h"
#include "geodiff_testutils.hpp"
#include "geodiffutils.hpp"
#include "tableschema.h"

TEST( ModifiedSchemeSqlite3Test, add_attribute )
{
  std::cout << "geopackage add attribute to table" << std::endl;
  std::string testname = "added_attribute";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "base.gpkg" );
  std::string modified = pathjoin( testdir(), "modified_scheme", "added_attribute.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.diff" );
  std::string expected = pathjoin( testdir(), "modified_scheme", "changesets", "added_attribute.diff" );

  ASSERT_EQ( GEODIFF_createChangeset( testContext(), base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );
  EXPECT_TRUE( fileContentEquals( changeset, expected ) );
}

TEST( ModifiedSchemeSqlite3Test, add_table )
{
  std::cout << "geopackage add table to database" << std::endl;
  std::string testname = "add_table";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "base.gpkg" );
  std::string modified = pathjoin( testdir(), "modified_scheme", "added_table.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.diff" );
  std::string expected = pathjoin( testdir(), "modified_scheme", "changesets", "added_table.diff" );

  ASSERT_EQ( GEODIFF_createChangeset( testContext(), base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );
  EXPECT_TRUE( fileContentEquals( changeset, expected ) );
}

TEST( ModifiedSchemeSqlite3Test, delete_attribute )
{
  std::cout << "geopackage delete attribute from table" << std::endl;
  std::string testname = "delete_attribute";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "modified_scheme", "added_attribute.gpkg" );
  std::string modified = pathjoin( testdir(), "base.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.diff" );
  std::string expected = pathjoin( testdir(), "modified_scheme", "changesets", "delete_attribute.diff" );

  ASSERT_EQ( GEODIFF_createChangeset( testContext(), base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );
  EXPECT_TRUE( fileContentEquals( changeset, expected ) );
}

TEST( ModifiedSchemeSqlite3Test, delete_table )
{
  std::cout << "geopackage delete table" << std::endl;
  std::string testname = "delete_table";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "modified_scheme", "added_table.gpkg" );
  std::string modified = pathjoin( testdir(), "base.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.diff" );
  std::string expected = pathjoin( testdir(), "modified_scheme", "changesets", "delete_table.diff" );

  ASSERT_EQ( GEODIFF_createChangeset( testContext(), base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );
  EXPECT_TRUE( fileContentEquals( changeset, expected ) );
}

TEST( ModifiedSchemeSqlite3Test, rename_table )
{
  std::cout << "geopackage table count is same, but tables have different name" << std::endl;
  std::string testname = "rename_table";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "modified_scheme", "added_table.gpkg" );
  std::string modified = pathjoin( testdir(), "modified_scheme", "added_table2.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.diff" );
  std::string expected = pathjoin( testdir(), "modified_scheme", "changesets", "rename_table.diff" );

  ASSERT_EQ( GEODIFF_createChangeset( testContext(), base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );
  EXPECT_TRUE( fileContentEquals( changeset, expected ) );
}

TEST( ModifiedSchemeSqlite3Test, rename_attribute )
{
  std::cout << "geopackage attribute count is same, but have different name" << std::endl;
  std::string testname = "rename_attribute";
  makedir( pathjoin( tmpdir(), testname ) );

  std::string base = pathjoin( testdir(), "modified_scheme", "added_attribute.gpkg" );
  std::string modified = pathjoin( testdir(), "modified_scheme", "added_attribute2.gpkg" );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.diff" );
  std::string expected = pathjoin( testdir(), "modified_scheme", "changesets", "rename_attribute.diff" );

  ASSERT_EQ( GEODIFF_createChangeset( testContext(), base.c_str(), modified.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );
  EXPECT_TRUE( fileContentEquals( changeset, expected ) );
}

// Create driver and fill DB with sample data
static std::unique_ptr<Driver> createSampleDb( std::string driverName, std::string testname, std::string dbname )
{
  DriverParametersMap params;
  if ( driverName == "sqlite" )
  {
    std::string dir = pathjoin( tmpdir(), testname );
    makedir( dir );
    params["base"] = pathjoin( dir, dbname + ".gpkg" );
  }
  else if ( driverName == "postgres" )
  {
    params["base"] = testname + "_" + dbname;
    params["conninfo"] = pgTestConnInfo();
  }

  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), driverName ) );
  driver->create( params, true );

  TableColumnInfo fidCol;
  fidCol.name = "fid";
  fidCol.type = columnType( static_cast<const Context *>( testContext() ), "integer", driverName );
  fidCol.isPrimaryKey = true;
  fidCol.isAutoIncrement = true;

  TableColumnInfo geometryCol;
  geometryCol.name = "geometry";
  geometryCol.type = columnType( static_cast<const Context *>( testContext() ), "point", driverName, true );
  geometryCol.isGeometry = true;

  TableColumnInfo nameCol;
  nameCol.name = "name";
  nameCol.type = columnType( static_cast<const Context *>( testContext() ), "text", driverName );

  driver->createTables(
  {
    {"tram_stops", { fidCol, geometryCol, nameCol } },
  } );

  driver->executeSql( "INSERT INTO tram_stops(fid, name) VALUES "
                      "(1, 'Ohrada'), "
                      "(2, 'Petřiny'), "
                      "(3, 'Park Maxe van der Stoela')" );

  return driver;
}

// Open driver with both base & modified as created by createBaseDb above
static std::unique_ptr<Driver> openBaseModifiedDb( std::string driverName, std::string testname, std::string baseName, std::string modifiedName )
{
  DriverParametersMap params;
  if ( driverName == "sqlite" )
  {
    std::string dir = pathjoin( tmpdir(), testname );
    makedir( dir );
    params["base"] = pathjoin( dir, baseName + ".gpkg" );
    if ( modifiedName.size() )
      params["modified"] = pathjoin( dir, modifiedName + ".gpkg" );
  }
  else if ( driverName == "postgres" )
  {
    params["base"] = testname + "_" + baseName;
    if ( modifiedName.size() )
      params["modified"] = testname + "_" + modifiedName;
    params["conninfo"] = pgTestConnInfo();
  }

  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), driverName ) );
  driver->open( params );
  return driver;
}

static void testSchemaDiffWith( std::string driverName, std::string testname, std::function<void ( Driver & )> modification )
{
  // Create base and modified DB
  {
    std::unique_ptr<Driver> baseDb = createSampleDb( driverName, testname, "base" );
    std::unique_ptr<Driver> modifiedDb = createSampleDb( driverName, testname, "modified" );
    modification( *modifiedDb );
  }

  // Create diff base->modified
  std::string diffPath = pathjoin( tmpdir(), testname, "diff" );
  {
    std::unique_ptr<Driver> baseModifiedDriver = openBaseModifiedDb( driverName, testname, "base", "modified" );
    ChangesetWriter writer;
    writer.open( diffPath );
    baseModifiedDriver->createChangeset( writer );
  }

  // Apply diff to base
  {
    ChangesetReader reader;
    reader.open( diffPath );
    std::unique_ptr<Driver> baseDb = openBaseModifiedDb( driverName, testname, "base", "" );
    baseDb->applyChangeset( reader );
  }

  // Check that base and modified are now equal
  std::string diff2Path = pathjoin( tmpdir(), testname, "diff2" );
  {
    std::unique_ptr<Driver> baseModifiedDriver = openBaseModifiedDb( driverName, testname, "base", "modified" );
    ChangesetWriter writer;
    writer.open( diff2Path );
    baseModifiedDriver->createChangeset( writer );
  }
  uintmax_t diff2Size = std::filesystem::file_size( diff2Path );
  ASSERT_EQ( diff2Size, 0 );

  // Invert diff
  std::string invertedDiffPath = pathjoin( tmpdir(), testname, "diff-inv" );
  {
    ChangesetReader reader;
    reader.open( diffPath );
    ChangesetWriter writer;
    writer.open( invertedDiffPath );
    invertChangeset( reader, writer );
  }

  // Apply inverted diff to base
  {
    ChangesetReader reader;
    reader.open( invertedDiffPath );
    std::unique_ptr<Driver> baseDb = openBaseModifiedDb( driverName, testname, "base", "" );
    baseDb->applyChangeset( reader );
  }

  // Check that base and original base are now equal
  std::string diff3Path = pathjoin( tmpdir(), testname, "diff2" );
  {
    createSampleDb( driverName, testname, "base2" );
    std::unique_ptr<Driver> baseModifiedDriver = openBaseModifiedDb( driverName, testname, "base2", "base" );
    ChangesetWriter writer;
    writer.open( diff3Path );
    baseModifiedDriver->createChangeset( writer );
  }
  uintmax_t diff3Size = std::filesystem::file_size( diff3Path );
  ASSERT_EQ( diff3Size, 0 );
}

TEST( ModifiedSchemeTest, create_table )
{
  // TODO: Postgres support
  std::string driverName = "sqlite";

  testSchemaDiffWith( driverName, "create_table", [ = ]( Driver & modifiedDb )
  {
    TableColumnInfo fidCol;
    fidCol.name = "fid";
    fidCol.type = columnType( static_cast<const Context *>( testContext() ), "integer", driverName );
    fidCol.isPrimaryKey = true;
    fidCol.isAutoIncrement = true;

    TableColumnInfo geometryCol;
    geometryCol.name = "geometry";
    geometryCol.type = columnType( static_cast<const Context *>( testContext() ), "point", driverName, true );
    geometryCol.isGeometry = true;

    TableColumnInfo materialCol;
    materialCol.name = "material";
    materialCol.type = columnType( static_cast<const Context *>( testContext() ), "text", driverName );

    TableSchema benchesTable{"benches", {fidCol, geometryCol, materialCol}};

    modifiedDb.createTables( {benchesTable} );
    modifiedDb.executeSql( "INSERT INTO benches (fid, material) VALUES (1, 'wood'), (2, 'steel')" );
  } );
}

TEST( ModifiedSchemeTest, add_column )
{
  // TODO: Postgres support
  std::string driverName = "sqlite";

  testSchemaDiffWith( driverName, "add_column", [ = ]( Driver & modifiedDb )
  {
    modifiedDb.executeSql( "INSERT INTO tram_stops (fid, name) VALUES (4, 'Palmovka')" );
    modifiedDb.executeSql( "UPDATE tram_stops SET name = 'Pohořelec' WHERE fid = 1" );
    modifiedDb.executeSql( "ALTER TABLE tram_stops ADD COLUMN bench_count integer" );
    modifiedDb.executeSql( "UPDATE tram_stops SET bench_count = 1 WHERE fid = 1" );
    modifiedDb.executeSql( "UPDATE tram_stops SET bench_count = 4 WHERE fid = 2" );
  } );
}

TEST( ModifiedSchemeTest, drop_column )
{
  // TODO: Postgres support
  std::string driverName = "sqlite";

  testSchemaDiffWith( driverName, "drop_column", [ = ]( Driver & modifiedDb )
  {
    modifiedDb.executeSql( "INSERT INTO tram_stops (fid, name) VALUES (4, 'Palmovka')" );
    modifiedDb.executeSql( "UPDATE tram_stops SET name = 'Pohořelec' WHERE fid = 1" );
    modifiedDb.executeSql( "ALTER TABLE tram_stops DROP COLUMN name" );
    modifiedDb.executeSql( "INSERT INTO tram_stops (fid) VALUES (5)" );
  } );
}

TEST( ModifiedSchemeTest, drop_table )
{
  // TODO: Postgres support
  std::string driverName = "sqlite";

  testSchemaDiffWith( driverName, "drop_table", [ = ]( Driver & modifiedDb )
  {
    modifiedDb.executeSql( "INSERT INTO tram_stops (fid, name) VALUES (4, 'Palmovka')" );
    modifiedDb.executeSql( "UPDATE tram_stops SET name = 'Pohořelec' WHERE fid = 1" );
    modifiedDb.executeSql( "DROP TABLE tram_stops" );
  } );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
