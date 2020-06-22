/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"

#include "changesetreader.h"
#include "changesetutils.h"
#include "changesetwriter.h"
#include "driver.h"

extern "C"
{
#include <libpq-fe.h>
}

void execSqlCommands( const std::string &conninfo, const std::string &filename )
{
  std::ifstream f( filename );
  ASSERT_TRUE( f.is_open() );

  std::string content( ( std::istreambuf_iterator<char>( f ) ), ( std::istreambuf_iterator<char>() ) );

  PGconn *c = PQconnectdb( conninfo.c_str() );

  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  PGresult *res = PQexec( c, content.c_str() );
  ASSERT_TRUE( res );

  EXPECT_EQ( PQresultStatus( res ), PGRES_COMMAND_OK );

  if ( PQresultStatus( res ) != PGRES_COMMAND_OK )
  {
    std::cerr << "execSqlCommands error: " << ::PQresultErrorMessage( res ) << std::endl;
  }

  PQclear( res );
  PQfinish( c );
}


TEST( PostgresDriverTest, test_basic )
{
  std::vector<std::string> driverNames = Driver::drivers();
  EXPECT_TRUE( std::find( driverNames.begin(), driverNames.end(), "postgres" ) != driverNames.end() );

  std::string conninfo = pgTestConnInfo();
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );

  DriverParametersMap params;
  params["conninfo"] = conninfo;
  params["base"] = "gd_base";

  std::unique_ptr<Driver> driver( Driver::createDriver( "postgres" ) );
  ASSERT_TRUE( driver );
  driver->open( params );

  std::vector<std::string> tables = driver->listTables();
  ASSERT_EQ( tables.size(), 1 );
  ASSERT_EQ( tables[0], "simple" );

  TableSchema sch = driver->tableSchema( "simple" );
  ASSERT_EQ( sch.name, "simple" );
  ASSERT_EQ( sch.columns.size(), 4 );
  ASSERT_EQ( sch.columns[0].name, "fid" );
  ASSERT_EQ( sch.columns[0].type, "integer" );
  ASSERT_EQ( sch.columns[0].isPrimaryKey, true );
  ASSERT_EQ( sch.columns[0].isNotNull, true );
  ASSERT_EQ( sch.columns[0].isAutoIncrement, true );
  ASSERT_EQ( sch.columns[0].isGeometry, false );

  ASSERT_EQ( sch.columns[1].name, "geometry" );
  ASSERT_EQ( sch.columns[1].type, "geometry(Point,4326)" );
  ASSERT_EQ( sch.columns[1].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[1].isNotNull, false );
  ASSERT_EQ( sch.columns[1].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[1].isGeometry, true );
  ASSERT_EQ( sch.columns[1].geomType, "POINT" );
  ASSERT_EQ( sch.columns[1].geomSrsId, 4326 );
  ASSERT_EQ( sch.columns[1].geomHasZ, false );
  ASSERT_EQ( sch.columns[1].geomHasM, false );

  ASSERT_EQ( sch.columns[2].name, "name" );
  ASSERT_EQ( sch.columns[2].type, "text" );
  ASSERT_EQ( sch.columns[2].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[2].isNotNull, false );
  ASSERT_EQ( sch.columns[2].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[2].isGeometry, false );

  ASSERT_EQ( sch.columns[3].name, "rating" );
  ASSERT_EQ( sch.columns[3].type, "integer" );
  ASSERT_EQ( sch.columns[3].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[3].isNotNull, false );
  ASSERT_EQ( sch.columns[3].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[3].isGeometry, false );

  ASSERT_EQ( sch.crs.srsId, 4326 );
  ASSERT_EQ( sch.crs.authName, "EPSG" );
  ASSERT_EQ( sch.crs.authCode, 4326 );
  ASSERT_TRUE( sch.crs.wkt.rfind( "GEOGCS[\"WGS 84\"", 0 ) == 0 );
}

void testCreateChangeset( const std::string &testname, const std::string &conninfo, const std::string &schemaBase, const std::string &schemaModified, const std::string &expectedChangeset )
{
  makedir( pathjoin( tmpdir(), testname ) );
  std::string output = pathjoin( tmpdir(), testname, "output.diff" );
  std::string outputJson = pathjoin( tmpdir(), testname, "output.json" );

  DriverParametersMap params;
  params["conninfo"] = conninfo;
  params["base"] = schemaBase;
  params["modified"] = schemaModified;

  std::unique_ptr<Driver> driver( Driver::createDriver( "postgres" ) );
  ASSERT_TRUE( driver );
  driver->open( params );

  {
    ChangesetWriter writer;
    writer.open( output );
    driver->createChangeset( writer );
  }

  {
    ChangesetReader reader;
    reader.open( output );
    std::string json = changesetToJSON( reader );
    std::ofstream f( outputJson );
    EXPECT_TRUE( f.is_open() );
    f << json;
  }

  EXPECT_TRUE( fileContentEquals( output, pathjoin( testdir(), "postgres", expectedChangeset ) ) );
}

TEST( PostgresDriverTest, test_create_changeset )
{
  std::string conninfo = pgTestConnInfo();
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "inserted_1_a.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "updated_a.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "deleted_a.sql" ) );

  testCreateChangeset( "test_postgres_insert", conninfo, "gd_base", "gd_inserted_1_a", "inserted_1_a.diff" );
  testCreateChangeset( "test_postgres_update", conninfo, "gd_base", "gd_updated_a", "updated_a.diff" );
  testCreateChangeset( "test_postgres_delete", conninfo, "gd_base", "gd_deleted_a", "deleted_a.diff" );
}

bool schemasEqual( const std::string &conninfo, const std::string &schema1, const std::string &schema2, const std::string &changesetComparePath )
{
  std::string changesetCompare = pathjoin( changesetComparePath, "compare.diff" );
  makedir( changesetComparePath );
  remove( changesetCompare.c_str() );
  EXPECT_FALSE( fileExists( changesetCompare ) );

  {
    DriverParametersMap paramsCompare;
    paramsCompare["conninfo"] = conninfo;
    paramsCompare["base"] = schema1;
    paramsCompare["modified"] = schema2;

    std::unique_ptr<Driver> driver( Driver::createDriver( "postgres" ) );
    EXPECT_TRUE( driver );
    driver->open( paramsCompare );

    ChangesetWriter writer;
    writer.open( changesetCompare );
    driver->createChangeset( writer );
  }

  EXPECT_TRUE( fileExists( changesetCompare ) );

  return isFileEmpty( changesetCompare );
}

void testApplyChangeset( const std::string &changeset, const std::string &conninfo, const std::string &schemaFinal )
{
  // create table in the base schema and move it to our "gd_test_apply" schema where we'll apply changeset
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "test_apply.sql" ) );

  DriverParametersMap params;
  params["conninfo"] = conninfo;
  params["base"] = "gd_test_apply";

  {
    std::unique_ptr<Driver> driver( Driver::createDriver( "postgres" ) );
    ASSERT_TRUE( driver );
    driver->open( params );

    ChangesetReader reader;
    reader.open( changeset );
    driver->applyChangeset( reader );
  }

  // now check that if we compare the table with applied changeset, we get no changes

  EXPECT_TRUE( schemasEqual( conninfo, schemaFinal, "gd_test_apply", pathjoin( tmpdir(), "test_postgres" ) ) );
}

TEST( PostgresDriverTest, test_apply_changeset )
{
  std::string conninfo = pgTestConnInfo();

  testApplyChangeset( pathjoin( testdir(), "postgres", "inserted_1_a.diff" ), conninfo, "gd_inserted_1_a" );
  testApplyChangeset( pathjoin( testdir(), "postgres", "updated_a.diff" ), conninfo, "gd_updated_a" );
  testApplyChangeset( pathjoin( testdir(), "postgres", "deleted_a.diff" ), conninfo, "gd_deleted_a" );
}

TEST( PostgresDriverTest, test_apply_changeset_conflict )
{
  // the diff file contains one regular delete and one wrong delete
  // we test that 1. applyChangeset will fail AND 2. the first (regular) delete will be rolled back
  std::string fileChangeset = pathjoin( testdir(), "conflict", "base-conflict-delete.diff" );
  std::string conninfo = pgTestConnInfo();

  // create table in the base schema and move it to our "gd_test_apply" schema where we'll apply changeset
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "test_apply.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );

  DriverParametersMap params;
  params["conninfo"] = conninfo;
  params["base"] = "gd_test_apply";

  {
    std::unique_ptr<Driver> driver( Driver::createDriver( "postgres" ) );
    ASSERT_TRUE( driver );
    driver->open( params );

    ChangesetReader reader;
    reader.open( fileChangeset );
    EXPECT_ANY_THROW( driver->applyChangeset( reader ) );
  }

  // now check that if we compare the table with applied changeset, we get no changes

  EXPECT_TRUE( schemasEqual( conninfo, "gd_base", "gd_test_apply", pathjoin( tmpdir(), "test_postgres" ) ) );
}

TEST( PostgresDriverTest, test_dump_data )
{
  std::string conninfo = pgTestConnInfo();

  makedir( pathjoin( tmpdir(), "test_postgres_dump_data" ) );
  std::string output = pathjoin( tmpdir(), "test_postgres_dump_data", "output.diff" );

  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );

  DriverParametersMap paramsBase;
  paramsBase["conninfo"] = conninfo;
  paramsBase["base"] = "gd_base";

  std::unique_ptr<Driver> driverBase( Driver::createDriver( "postgres" ) );
  ASSERT_TRUE( driverBase );
  driverBase->open( paramsBase );

  {
    ChangesetWriter writer;
    writer.open( output );
    driverBase->dumpData( writer );
  }

  ASSERT_TRUE( fileContentEquals( output, pathjoin( testdir(), "postgres", "dump_data.diff" ) ) );
}

TEST( PostgresDriverTest, test_create_tables )
{
  std::string conninfo = pgTestConnInfo();

  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );

  DriverParametersMap paramsBase;
  paramsBase["conninfo"] = conninfo;
  paramsBase["base"] = "gd_base";

  std::unique_ptr<Driver> driverBase( Driver::createDriver( "postgres" ) );
  ASSERT_TRUE( driverBase );
  driverBase->open( paramsBase );

  std::vector<TableSchema> tables;
  for ( std::string tableName : driverBase->listTables() )
    tables.push_back( driverBase->tableSchema( tableName ) );

  DriverParametersMap paramsCreate;
  paramsCreate["conninfo"] = conninfo;
  paramsCreate["base"] = "gd_schema_test";

  std::unique_ptr<Driver> driverCreate( Driver::createDriver( "postgres" ) );
  ASSERT_TRUE( driverCreate );
  driverCreate->create( paramsCreate, true );

  ASSERT_EQ( driverCreate->listTables().size(), 0 );

  driverCreate->createTables( tables );

  ASSERT_EQ( driverCreate->listTables().size(), 1 );
  ASSERT_TRUE( driverCreate->tableSchema( "simple" ) == tables[0] );

  {
    ChangesetReader reader;
    reader.open( pathjoin( testdir(), "postgres", "dump_data.diff" ) );
    driverCreate->applyChangeset( reader );
  }

  // check the table is populated (+ create changeset gives no results)

  DriverParametersMap paramsCheck;
  paramsCheck["conninfo"] = conninfo;
  paramsCheck["base"] = "gd_base";
  paramsCheck["modified"] = "gd_schema_test";

  std::unique_ptr<Driver> driverCheck( Driver::createDriver( "postgres" ) );
  ASSERT_TRUE( driverCheck );
  driverCheck->open( paramsCheck );

  makedir( pathjoin( tmpdir(), "test_postgres_create_tables" ) );
  std::string output = pathjoin( tmpdir(), "test_postgres_create_tables", "output.diff" );
  {
    ChangesetWriter writerCheck;
    writerCheck.open( output );
    driverCheck->createChangeset( writerCheck );
  }

  ASSERT_TRUE( isFileEmpty( output ) );
}


TEST( PostgresDriverTest, test_create_sqlite_from_postgres )
{
  std::string conninfo = pgTestConnInfo();

  std::string testname = "test_create_from_postgres";
  makedir( pathjoin( tmpdir(), testname ) );
  std::string testdb = pathjoin( tmpdir(), testname, "output.gpkg" );

  // get table schema in the base database
  std::map<std::string, std::string> connBase;
  connBase["conninfo"] = conninfo;
  connBase["base"] = "gd_base";
  std::unique_ptr<Driver> driverBase( Driver::createDriver( "postgres" ) );
  driverBase->open( connBase );
  TableSchema tblBaseSimple = driverBase->tableSchema( "simple" );

  tableSchemaPostgresToSqlite( tblBaseSimple );   // make it sqlite driver friendly

  // create the new database
  std::map<std::string, std::string> conn;
  conn["base"] = testdb;
  std::unique_ptr<Driver> driver( Driver::createDriver( "sqlite" ) );
  EXPECT_NO_THROW( driver->create( conn, true ) );
  EXPECT_ANY_THROW( driver->tableSchema( "simple" ) );

  // create table
  std::vector<TableSchema> schemas;
  schemas.push_back( tblBaseSimple );
  driver->createTables( schemas );

  // verify it worked
  EXPECT_NO_THROW( driver->tableSchema( "simple" ) );

  TableSchema tblNewSimple = driver->tableSchema( "simple" );
  EXPECT_EQ( tblBaseSimple.name, tblNewSimple.name );
  EXPECT_EQ( tblBaseSimple.columns.size(), tblNewSimple.columns.size() );
}


int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
