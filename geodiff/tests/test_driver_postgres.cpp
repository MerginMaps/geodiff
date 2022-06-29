/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"

#include "geodiffcontext.hpp"
#include "changesetreader.h"
#include "changesetutils.h"
#include "changesetwriter.h"
#include "driver.h"
#include "postgresutils.h"

#include "json.hpp"

extern "C"
{
#include <libpq-fe.h>
}

void execSqlCommandsFromString( const std::string &conninfo, const std::string &sql )
{
  PGconn *c = PQconnectdb( conninfo.c_str() );

  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  PGresult *res = PQexec( c, sql.c_str() );
  ASSERT_TRUE( res );

  EXPECT_EQ( PQresultStatus( res ), PGRES_COMMAND_OK );

  if ( PQresultStatus( res ) != PGRES_COMMAND_OK )
  {
    std::cerr << "execSqlCommands error: " << ::PQresultErrorMessage( res ) << std::endl;
  }

  PQclear( res );
  PQfinish( c );
}

void execSqlCommands( const std::string &conninfo, const std::string &filename )
{
  std::ifstream f( filename );
  ASSERT_TRUE( f.is_open() );

  std::string content( ( std::istreambuf_iterator<char>( f ) ), ( std::istreambuf_iterator<char>() ) );
  execSqlCommandsFromString( conninfo, content );
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

  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
  ASSERT_TRUE( driver );
  driver->open( params );

  std::vector<std::string> tables = driver->listTables();
  ASSERT_EQ( tables.size(), 1 );
  ASSERT_EQ( tables[0], "simple" );

  TableSchema sch = driver->tableSchema( "simple" );
  ASSERT_EQ( sch.name, "simple" );
  ASSERT_EQ( sch.columns.size(), 4 );
  ASSERT_EQ( sch.columns[0].name, "fid" );
  ASSERT_EQ( sch.columns[0].type.dbType, "integer" );
  ASSERT_EQ( sch.columns[0].isPrimaryKey, true );
  ASSERT_EQ( sch.columns[0].isNotNull, true );
  ASSERT_EQ( sch.columns[0].isAutoIncrement, true );
  ASSERT_EQ( sch.columns[0].isGeometry, false );

  ASSERT_EQ( sch.columns[1].name, "geometry" );
  ASSERT_EQ( sch.columns[1].type.dbType, "geometry(Point,4326)" );
  ASSERT_EQ( sch.columns[1].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[1].isNotNull, false );
  ASSERT_EQ( sch.columns[1].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[1].isGeometry, true );
  ASSERT_EQ( sch.columns[1].geomType, "POINT" );
  ASSERT_EQ( sch.columns[1].geomSrsId, 4326 );
  ASSERT_EQ( sch.columns[1].geomHasZ, false );
  ASSERT_EQ( sch.columns[1].geomHasM, false );

  ASSERT_EQ( sch.columns[2].name, "name" );
  ASSERT_EQ( sch.columns[2].type.dbType, "text" );
  ASSERT_EQ( sch.columns[2].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[2].isNotNull, false );
  ASSERT_EQ( sch.columns[2].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[2].isGeometry, false );

  ASSERT_EQ( sch.columns[3].name, "rating" );
  ASSERT_EQ( sch.columns[3].type.dbType, "integer" );
  ASSERT_EQ( sch.columns[3].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[3].isNotNull, false );
  ASSERT_EQ( sch.columns[3].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[3].isGeometry, false );

  ASSERT_EQ( sch.crs.srsId, 4326 );
  ASSERT_EQ( sch.crs.authName, "EPSG" );
  ASSERT_EQ( sch.crs.authCode, 4326 );
  ASSERT_TRUE( sch.crs.wkt.rfind( "GEOGCS[\"WGS 84\"", 0 ) == 0 );
}

TEST( PostgresDriverTest, test_datatypes )
{
  std::vector<std::string> driverNames = Driver::drivers();
  EXPECT_TRUE( std::find( driverNames.begin(), driverNames.end(), "postgres" ) != driverNames.end() );

  std::string conninfo = pgTestConnInfo();
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "datatypes.sql" ) );

  DriverParametersMap params;
  params["conninfo"] = conninfo;
  params["base"] = "gd_datatypes";

  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
  ASSERT_TRUE( driver );
  driver->open( params );

  std::vector<std::string> tables = driver->listTables();
  ASSERT_EQ( tables.size(), 1 );
  ASSERT_EQ( tables[0], "simple" );

  TableSchema sch = driver->tableSchema( "simple" );
  ASSERT_EQ( sch.name, "simple" );
  ASSERT_EQ( sch.columns.size(), 7 );

  ASSERT_EQ( sch.columns[0].name, "fid" );
  ASSERT_EQ( sch.columns[0].type.baseType, TableColumnType::INTEGER );
  ASSERT_EQ( sch.columns[0].type.dbType, "integer" );
  ASSERT_EQ( sch.columns[0].isPrimaryKey, true );
  ASSERT_EQ( sch.columns[0].isNotNull, true );
  ASSERT_EQ( sch.columns[0].isAutoIncrement, true );
  ASSERT_EQ( sch.columns[0].isGeometry, false );

  ASSERT_EQ( sch.columns[1].name, "geometry" );
  ASSERT_EQ( sch.columns[1].type.baseType, TableColumnType::GEOMETRY );
  ASSERT_EQ( sch.columns[1].type.dbType, "geometry(Point,4326)" );
  ASSERT_EQ( sch.columns[1].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[1].isNotNull, false );
  ASSERT_EQ( sch.columns[1].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[1].isGeometry, true );
  ASSERT_EQ( sch.columns[1].geomType, "POINT" );
  ASSERT_EQ( sch.columns[1].geomSrsId, 4326 );
  ASSERT_EQ( sch.columns[1].geomHasZ, false );
  ASSERT_EQ( sch.columns[1].geomHasM, false );

  ASSERT_EQ( sch.columns[2].name, "name_text" );
  ASSERT_EQ( sch.columns[2].type.baseType, TableColumnType::TEXT );
  ASSERT_EQ( sch.columns[2].type.dbType, "text" );
  ASSERT_EQ( sch.columns[2].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[2].isNotNull, false );
  ASSERT_EQ( sch.columns[2].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[2].isGeometry, false );

  ASSERT_EQ( sch.columns[3].name, "name_varchar" );
  ASSERT_EQ( sch.columns[3].type, TableColumnType::TEXT );
  ASSERT_EQ( sch.columns[3].type.dbType, "character varying" );
  ASSERT_EQ( sch.columns[3].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[3].isNotNull, false );
  ASSERT_EQ( sch.columns[3].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[3].isGeometry, false );

  ASSERT_EQ( sch.columns[4].name, "name_varchar_len" );
  ASSERT_EQ( sch.columns[4].type.baseType, TableColumnType::TEXT );
  ASSERT_EQ( sch.columns[4].type.dbType, "character varying(50)" );
  ASSERT_EQ( sch.columns[4].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[4].isNotNull, false );
  ASSERT_EQ( sch.columns[4].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[4].isGeometry, false );

  ASSERT_EQ( sch.columns[5].name, "name_char_len" );
  ASSERT_EQ( sch.columns[5].type.baseType, TableColumnType::TEXT );
  ASSERT_EQ( sch.columns[5].type.dbType, "character(100)" );
  ASSERT_EQ( sch.columns[5].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[5].isNotNull, false );
  ASSERT_EQ( sch.columns[5].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[5].isGeometry, false );

  ASSERT_EQ( sch.columns[6].name, "feature_id" );
  ASSERT_EQ( sch.columns[6].type.baseType, TableColumnType::TEXT );
  ASSERT_EQ( sch.columns[6].type.dbType, "uuid" );
  ASSERT_EQ( sch.columns[6].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[6].isNotNull, false );
  ASSERT_EQ( sch.columns[6].isAutoIncrement, false );
  ASSERT_EQ( sch.columns[6].isGeometry, false );
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

  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
  ASSERT_TRUE( driver );
  driver->open( params );

  {
    ChangesetWriter writer;
    ASSERT_NO_THROW( writer.open( output ) );
    driver->createChangeset( writer );
  }

  {
    ChangesetReader reader;
    EXPECT_TRUE( reader.open( output ) );
    nlohmann::json json = changesetToJSON( reader );
    std::ofstream f( outputJson );
    EXPECT_TRUE( f.is_open() );
    f << json.dump( 2 );
  }

  EXPECT_TRUE( fileContentEquals( output, pathjoin( testdir(), "postgres", expectedChangeset ) ) );
}

TEST( PostgresDriverApi, test_driver_postgres_api )
{
  int ndrivers = 2;
  EXPECT_EQ( GEODIFF_driverCount( testContext() ), ndrivers );

  char driverName[256];
  EXPECT_EQ( GEODIFF_driverNameFromIndex( testContext(), 1, driverName ), GEODIFF_SUCCESS );

  EXPECT_EQ( std::string( driverName ), "postgres" );

  EXPECT_TRUE( GEODIFF_driverIsRegistered( testContext(), "postgres" ) );
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
  EXPECT_FALSE( fileexists( changesetCompare ) );

  {
    DriverParametersMap paramsCompare;
    paramsCompare["conninfo"] = conninfo;
    paramsCompare["base"] = schema1;
    paramsCompare["modified"] = schema2;

    std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
    EXPECT_TRUE( driver );
    driver->open( paramsCompare );

    ChangesetWriter writer;
    EXPECT_NO_THROW( writer.open( changesetCompare ) );
    driver->createChangeset( writer );
  }

  EXPECT_TRUE( fileexists( changesetCompare ) );

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
    std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
    ASSERT_TRUE( driver );
    driver->open( params );

    ChangesetReader reader;
    EXPECT_TRUE( reader.open( changeset ) );
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
    std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
    ASSERT_TRUE( driver );
    driver->open( params );

    ChangesetReader reader;
    EXPECT_TRUE( reader.open( fileChangeset ) );
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

  std::unique_ptr<Driver> driverBase( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
  ASSERT_TRUE( driverBase );
  driverBase->open( paramsBase );

  {
    ChangesetWriter writer;
    ASSERT_NO_THROW( writer.open( output ) );
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

  std::unique_ptr<Driver> driverBase( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
  ASSERT_TRUE( driverBase );
  driverBase->open( paramsBase );

  std::vector<TableSchema> tables;
  for ( std::string tableName : driverBase->listTables() )
    tables.push_back( driverBase->tableSchema( tableName ) );

  DriverParametersMap paramsCreate;
  paramsCreate["conninfo"] = conninfo;
  paramsCreate["base"] = "gd_schema_test";

  std::unique_ptr<Driver> driverCreate( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
  ASSERT_TRUE( driverCreate );
  driverCreate->create( paramsCreate, true );

  ASSERT_EQ( driverCreate->listTables().size(), 0 );

  driverCreate->createTables( tables );

  ASSERT_EQ( driverCreate->listTables().size(), 1 );
  ASSERT_TRUE( driverCreate->tableSchema( "simple" ) == tables[0] );

  {
    ChangesetReader reader;
    EXPECT_TRUE( reader.open( pathjoin( testdir(), "postgres", "dump_data.diff" ) ) );
    driverCreate->applyChangeset( reader );
  }

  // check the table is populated (+ create changeset gives no results)

  DriverParametersMap paramsCheck;
  paramsCheck["conninfo"] = conninfo;
  paramsCheck["base"] = "gd_base";
  paramsCheck["modified"] = "gd_schema_test";

  std::unique_ptr<Driver> driverCheck( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
  ASSERT_TRUE( driverCheck );
  driverCheck->open( paramsCheck );

  makedir( pathjoin( tmpdir(), "test_postgres_create_tables" ) );
  std::string output = pathjoin( tmpdir(), "test_postgres_create_tables", "output.diff" );
  {
    ChangesetWriter writerCheck;
    ASSERT_NO_THROW( writerCheck.open( output ) );
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
  std::unique_ptr<Driver> driverBase( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
  driverBase->open( connBase );
  TableSchema tblBaseSimple = driverBase->tableSchema( "simple" );

  tableSchemaConvert( "sqlite", tblBaseSimple );   // make it sqlite driver friendly

  // create the new database
  std::map<std::string, std::string> conn;
  conn["base"] = testdb;
  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
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

TEST( PostgresDriverTest, test_create_postgres_from_sqlite )
{
  TableColumnInfo test1c1;
  test1c1.name = "c1";
  test1c1.type = columnType( static_cast<const Context *>( testContext() ), "mediumint", Driver::SQLITEDRIVERNAME );
  test1c1.isPrimaryKey = true;
  test1c1.isAutoIncrement = true;

  TableColumnInfo test1c2;
  test1c2.name = "c2";
  test1c2.type = columnType( static_cast<const Context *>( testContext() ), "bool", Driver::SQLITEDRIVERNAME );

  TableColumnInfo test1c3;
  test1c3.name = "c3";
  test1c3.type = columnType( static_cast<const Context *>( testContext() ), "mediumint", Driver::SQLITEDRIVERNAME );

  TableColumnInfo test1c4;
  test1c4.name = "c4";
  test1c4.type = columnType( static_cast<const Context *>( testContext() ), "varchar(255)", Driver::SQLITEDRIVERNAME );

  TableSchema tblTest1;
  tblTest1.name = "test_1";
  tblTest1.columns.push_back( test1c1 );
  tblTest1.columns.push_back( test1c2 );
  tblTest1.columns.push_back( test1c3 );
  tblTest1.columns.push_back( test1c4 );

  tableSchemaConvert( "postgres", tblTest1 );   // make it postgres driver friendly

  // check exported types
  EXPECT_EQ( tblTest1.columns[0].type.dbType, "serial" );
  EXPECT_EQ( tblTest1.columns[1].type.dbType, "boolean" );
  EXPECT_EQ( tblTest1.columns[2].type.dbType, "integer" );
  EXPECT_EQ( tblTest1.columns[3].type.dbType, "text" );

  execSqlCommandsFromString( pgTestConnInfo(), "DROP SCHEMA IF EXISTS gd_test_postgres_from_sqlite CASCADE;" );

  DriverParametersMap paramsBase;
  paramsBase["conninfo"] = pgTestConnInfo();
  paramsBase["base"] = "gd_test_postgres_from_sqlite";
  std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
  ASSERT_TRUE( driver );
  driver->create( paramsBase );
  std::vector<TableSchema> tables;
  tables.push_back( tblTest1 );
  driver->createTables( tables );
}

TEST( PostgresDriverTest, test_updated_sequence )
{
  // here we test that after inserting rows within a changeset, the auto-increment primary key's
  // sequence object gets updated accordingly

  std::string conninfo = pgTestConnInfo();
  // create table in the base schema and move it to our "gd_test_apply" schema where we'll apply changeset
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "test_apply.sql" ) );
  std::string changeset = pathjoin( testdir(), "postgres", "inserted_1_a.diff" );

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  PostgresResult res1( execSql( c, "SELECT last_value FROM gd_test_apply.simple_fid_seq" ) );
  int lastValue1 = std::stoi( res1.value( 0, 0 ) );
  EXPECT_TRUE( lastValue1 <= 3 );  // even though there are 3 rows in the table, sequence may be uninitialized

  DriverParametersMap params;
  params["conninfo"] = conninfo;
  params["base"] = "gd_test_apply";

  {
    std::unique_ptr<Driver> driver( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
    ASSERT_TRUE( driver );
    driver->open( params );

    ChangesetReader reader;
    EXPECT_TRUE( reader.open( changeset ) );
    driver->applyChangeset( reader );
  }

  PostgresResult res2( execSql( c, "SELECT last_value FROM gd_test_apply.simple_fid_seq" ) );
  int lastValue2 = std::stoi( res2.value( 0, 0 ) );
  EXPECT_EQ( lastValue2, 4 );

  // try to insert a new row - it should not fail
  PostgresResult resInsert( execSql( c, "INSERT INTO gd_test_apply.simple (name, rating) VALUES ('test seq', 999);" ) );
  EXPECT_EQ( resInsert.affectedRows(), "1" );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_rebase )
{
  std::string conninfo = pgTestConnInfo();
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );  // gd_base
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "inserted_1_a.sql" ) );  // gd_inserted_1_a
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "inserted_1_b.sql" ) );  // gd_inserted_1_b

  std::string testname = "test_postgres_rebase";
  makedir( pathjoin( tmpdir(), testname ) );
  std::string base2our = pathjoin( tmpdir(), testname, "base2our" );
  std::string base2their = pathjoin( tmpdir(), testname, "base2their" );
  std::string conflictfile = pathjoin( tmpdir(), testname, "conflict" );

  int res1 = GEODIFF_createChangesetEx( testContext(), "postgres", conninfo.c_str(), "gd_base", "gd_inserted_1_a", base2our.c_str() );
  int res2 = GEODIFF_createChangesetEx( testContext(), "postgres", conninfo.c_str(), "gd_base", "gd_inserted_1_b", base2their.c_str() );
  EXPECT_EQ( res1, GEODIFF_SUCCESS );
  EXPECT_EQ( res2, GEODIFF_SUCCESS );

  int rc = GEODIFF_rebaseEx( testContext(), "postgres", conninfo.c_str(), "gd_base", "gd_inserted_1_a", base2their.c_str(), conflictfile.c_str() );
  EXPECT_EQ( rc, GEODIFF_SUCCESS );

  // check the actual results

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  // check the actual number of rows: 3 rows from base + 1 row from 1_a + 1 row from 1_b = 5
  // first there should be the new row from inserted_1_b ("my new point B") and then there
  // should be the new row from inserted_1_a ("my new point A") which was rebased

  PostgresResult resTestCount( execSql( c, "select count(*) from gd_inserted_1_a.simple" ) );
  EXPECT_EQ( std::stoi( resTestCount.value( 0, 0 ) ), 5 );

  PostgresResult resTestFid4( execSql( c, "select name from gd_inserted_1_a.simple where fid = 4" ) );
  EXPECT_EQ( resTestFid4.value( 0, 0 ), "my new point B" );

  PostgresResult resTestFid5( execSql( c, "select name from gd_inserted_1_a.simple where fid = 5" ) );
  EXPECT_EQ( resTestFid5.value( 0, 0 ), "my new point A" );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_capital_letters )
{
  std::string conninfo = pgTestConnInfo();

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string testname = "test_capital_letters";

  makedir( pathjoin( tmpdir(), testname ) );
  std::string changeset = pathjoin( tmpdir(), testname, "changes.diff" );
  std::string baseGpkg( pathjoin( testdir(), "capital-letters", "db-capital-base.gpkg" ) );
  std::string modifiedGpkg( pathjoin( testdir(), "capital-letters", "db-capital-modified.gpkg" ) );

  // try capital letters when in schema and table names
  EXPECT_EQ( GEODIFF_makeCopy( testContext(), "sqlite", "", baseGpkg.c_str(), "postgres", conninfo.c_str(), "GD_base" ), GEODIFF_SUCCESS );
  EXPECT_EQ( GEODIFF_makeCopy( testContext(), "sqlite", "", modifiedGpkg.c_str(), "postgres", conninfo.c_str(), "GD_modified" ), GEODIFF_SUCCESS );
  EXPECT_EQ( GEODIFF_makeCopy( testContext(), "postgres", conninfo.c_str(), "GD_base", "postgres", conninfo.c_str(), "GD_base_copy" ), GEODIFF_SUCCESS );

  EXPECT_EQ( GEODIFF_createChangesetEx( testContext(), "postgres", conninfo.c_str(), "GD_base", "GD_modified", changeset.c_str() ), GEODIFF_SUCCESS );

  ASSERT_TRUE( fileexists( changeset ) );

  EXPECT_EQ( GEODIFF_applyChangesetEx( testContext(), "postgres", conninfo.c_str(), "GD_base", changeset.c_str() ), GEODIFF_SUCCESS );

  // check value
  PostgresResult resTestFid5( execSql( c, "SELECT \"Name\" FROM \"GD_base\".\"CapitalCity\" where fid = 6" ) );
  EXPECT_EQ( resTestFid5.value( 0, 0 ), "London" );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_gpkg_with_envelope )
{
  std::string conninfo = pgTestConnInfo();

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string envelopeGpkg( pathjoin( testdir(), "envelope_gpkg", "db-envelope.gpkg" ) );

  EXPECT_EQ( GEODIFF_makeCopy( testContext(), "sqlite", "", envelopeGpkg.c_str(), "postgres", conninfo.c_str(), "gd_envelope_test" ), GEODIFF_SUCCESS );

  std::string testName( "test_gpkg_with_envelope" );
  makedir( pathjoin( tmpdir(), testName ) );

  std::string changeset( pathjoin( tmpdir(), testName, "changeset.diff" ) );
  std::string tmpEnvelopeGpkg( pathjoin( tmpdir(), testName, "db-envelope-tmp.gpkg" ) );

  EXPECT_EQ( GEODIFF_makeCopy( testContext(), "postgres", conninfo.c_str(), "gd_envelope_test", "sqlite", "", tmpEnvelopeGpkg.c_str() ), GEODIFF_SUCCESS );

  EXPECT_EQ( GEODIFF_createChangeset( testContext(), envelopeGpkg.c_str(), tmpEnvelopeGpkg.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );

  ChangesetReader r;
  EXPECT_TRUE( r.open( changeset ) );
  EXPECT_TRUE( r.isEmpty() );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_conversion_with_dates )
{
  std::string conninfo = pgTestConnInfo();

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string testName( "test_conversion_with_dates" );
  std::string timeGpkg( pathjoin( testdir(), "conversions", "db-time.gpkg" ) );

  EXPECT_EQ( GEODIFF_makeCopy( testContext(), "sqlite", "", timeGpkg.c_str(), "postgres", conninfo.c_str(), "gd_time" ), GEODIFF_SUCCESS );

  DriverParametersMap params;
  params["conninfo"] = conninfo;
  params["base"] = "gd_time";

  std::unique_ptr<Driver> driverPG( Driver::createDriver( static_cast<Context *>( testContext() ), "postgres" ) );
  ASSERT_TRUE( driverPG );
  driverPG->open( params );

  std::vector<std::string> tables = driverPG->listTables();

  TableSchema sch = driverPG->tableSchema( "city" );
  ASSERT_EQ( sch.name, "city" );
  ASSERT_EQ( sch.columns.size(), 5 );
  ASSERT_EQ( sch.columns[3].type.dbType, "date" );
  ASSERT_EQ( sch.columns[4].type.dbType, "timestamp without time zone" );

  // convert back to sqlite
  makedir( pathjoin( tmpdir(), testName ) );
  std::string tmpGpkg( pathjoin( tmpdir(), testName, "tmpGpkg.gpkg" ) );

  EXPECT_EQ( GEODIFF_makeCopy( testContext(), "postgres", conninfo.c_str(), "gd_time",  "sqlite", "", tmpGpkg.c_str() ), GEODIFF_SUCCESS );

  std::unique_ptr<Driver> driverGPKG( Driver::createDriver( static_cast<Context *>( testContext() ), "sqlite" ) );
  ASSERT_TRUE( driverGPKG );
  driverGPKG->open( Driver::sqliteParametersSingleSource( tmpGpkg ) );

  tables = driverGPKG->listTables();
  sch = driverGPKG->tableSchema( "city" );
  ASSERT_EQ( sch.name, "city" );
  ASSERT_EQ( sch.columns.size(), 5 );
  ASSERT_EQ( sch.columns[3].type.dbType, "DATE" );
  ASSERT_EQ( sch.columns[4].type.dbType, "DATETIME" );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_ignore_gpkg_meta_tables )
{
  std::string conninfo = pgTestConnInfo();

  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string tmpdirectory( pathjoin( tmpdir(), "test_ignore_gpkg_meta_tables" ) );
  makedir( tmpdirectory );

  // Some geopackages in testdata (like base or inserted_1_A) do not contain meta tables coming from former versions on GPKG.
  // This resulted in table mismatch between sources and thus failing some operations in geodiff API.
  std::string modifiedGpkg( pathjoin( testdir(), "2_inserts", "inserted_1_A.gpkg" ) );
  std::string baseGpkg( pathjoin( tmpdirectory, "comparingBase.gpkg" ) );
  std::string changeset( pathjoin( tmpdirectory, "changeset.diff" ) );

  EXPECT_EQ( GEODIFF_makeCopy( testContext(), "postgres", conninfo.c_str(), "gd_base", "sqlite", "", baseGpkg.c_str() ), GEODIFF_SUCCESS );

  EXPECT_EQ( GEODIFF_createChangeset( testContext(), baseGpkg.c_str(), modifiedGpkg.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );

  ASSERT_TRUE( fileexists( changeset ) );

  EXPECT_EQ( GEODIFF_applyChangeset( testContext(), baseGpkg.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_changesetdr_sqlite_to_pg )
{
  std::string conninfo = pgTestConnInfo();

  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "inserted_1_a.sql" ) );

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string testname = "test_changesetdr_sqlite_to_pg";
  std::string baseGpkg = pathjoin( testdir(), "base.gpkg" );

  makedir( pathjoin( tmpdir(), testname ) );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.diff" );

  // compare sqlite base, PG modified
  int ret = GEODIFF_createChangesetDr( testContext(), "sqlite", "", baseGpkg.c_str(), "postgres", conninfo.c_str(), "gd_inserted_1_a", changeset.c_str() );
  EXPECT_EQ( ret, GEODIFF_SUCCESS );

  EXPECT_TRUE( fileexists( changeset ) );

  std::string toApplyGpkg = pathjoin( tmpdir(), testname, "to-apply.gpkg" );
  filecopy( toApplyGpkg, baseGpkg );

  ret = GEODIFF_applyChangesetEx( testContext(), "sqlite", "", toApplyGpkg.c_str(), changeset.c_str() );
  EXPECT_EQ( ret, GEODIFF_SUCCESS );

  std::string comparingGpkg = pathjoin( testdir(), "changeset-drivers", "modified.gpkg" );

  EXPECT_TRUE( equals( toApplyGpkg, comparingGpkg ) );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_changesetdr_pg_to_sqlite )
{
  std::string conninfo = pgTestConnInfo();

  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string testname = "test_changesetdr_pg_to_sqlite";
  std::string modifiedGpkg( pathjoin( testdir(), "2_inserts", "inserted_1_A.gpkg" ) );

  makedir( pathjoin( tmpdir(), testname ) );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.diff" );

  // compare PG base, sqlite modified
  int ret = GEODIFF_createChangesetDr( testContext(), "postgres", conninfo.c_str(), "gd_base", "sqlite", "", modifiedGpkg.c_str(), changeset.c_str() );
  EXPECT_EQ( ret, GEODIFF_SUCCESS );

  EXPECT_TRUE( fileexists( changeset ) );

  ret = GEODIFF_applyChangesetEx( testContext(), "postgres", conninfo.c_str(), "gd_base", changeset.c_str() );
  EXPECT_EQ( ret, GEODIFF_SUCCESS );

  // check value
  PostgresResult resTestFid5( execSql( c, "SELECT \"name\" FROM \"gd_base\".\"simple\" where fid = 4" ) );
  EXPECT_EQ( resTestFid5.value( 0, 0 ), "my new point A" );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_changesetdr_pg_to_pg )
{
  std::string conninfo = pgTestConnInfo();

  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "inserted_1_a.sql" ) );

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string testname = "test_changesetdr_pg_to_pg";

  makedir( pathjoin( tmpdir(), testname ) );
  std::string changeset = pathjoin( tmpdir(), testname, "changeset.diff" );

  // compare the same drivers
  int ret = GEODIFF_createChangesetDr( testContext(), "postgres", conninfo.c_str(), "gd_base", "postgres", conninfo.c_str(), "gd_inserted_1_a", changeset.c_str() );
  EXPECT_EQ( ret, GEODIFF_SUCCESS );

  ChangesetReader reader;
  EXPECT_TRUE( reader.open( changeset ) );

  EXPECT_TRUE( !reader.isEmpty() );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_multipart_geometries )
{
  std::string conninfo = pgTestConnInfo();

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string from = pathjoin( testdir(), "conversions", "db-multi-geometries.gpkg" );
  ASSERT_EQ( GEODIFF_makeCopy( testContext(), "sqlite", "", from.c_str(), "postgres", conninfo.c_str(), "db_multipart" ), GEODIFF_SUCCESS );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_3d_geometries )
{
  std::string conninfo = pgTestConnInfo();

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string gpkgBase = pathjoin( testdir(), "3d", "db3d-base.gpkg" );
  std::string gpkgModified = pathjoin( testdir(), "3d", "db3d-modified.gpkg" );

  std::string pgBase( "gd_3d_base" );
  std::string pgAux( "gd_3d_aux" );

  ASSERT_EQ( GEODIFF_makeCopy( testContext(), "sqlite", "", gpkgBase.c_str(), "postgres", conninfo.c_str(), pgAux.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_makeCopy( testContext(), "postgres", conninfo.c_str(), pgAux.c_str(), "postgres", conninfo.c_str(), pgBase.c_str() ), GEODIFF_SUCCESS );

  makedir( pathjoin( tmpdir(), "test_3d_geometries" ) );
  std::string changeset( pathjoin( tmpdir(), "test_3d_geometries", "changeset.diff" ) );

  ASSERT_EQ( GEODIFF_createChangesetDr( testContext(), "postgres", conninfo.c_str(), pgBase.c_str(), "sqlite", "", gpkgModified.c_str(), changeset.c_str() ), GEODIFF_SUCCESS );

  ChangesetReader reader;
  EXPECT_TRUE( reader.open( changeset ) );
  EXPECT_TRUE( !reader.isEmpty() );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_floating_point_values )
{
  // Copy GPKG with a table that contains some double numbers with many decimal places to Postgres
  // and back and check whether the copy still has correct values (not truncated when copying).

  std::string conninfo = pgTestConnInfo();

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string gpkgBase = pathjoin( testdir(), "floating_point_values", "db-floating.gpkg" );
  std::string pgBase( "gd_floating_point_values" );

  makedir( pathjoin( tmpdir(), "test_floating_point_values" ) );
  std::string gpkgCopy( pathjoin( tmpdir(), "test_floating_point_values", "db-floating-copy.gpkg" ) );
  std::string diff( pathjoin( tmpdir(), "test_floating_point_values", "db-floating.diff" ) );

  ASSERT_EQ( GEODIFF_makeCopy( testContext(), "sqlite", "", gpkgBase.c_str(), "postgres", conninfo.c_str(), pgBase.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_makeCopy( testContext(), "postgres", conninfo.c_str(), pgBase.c_str(), "sqlite", "", gpkgCopy.c_str() ), GEODIFF_SUCCESS );

  ASSERT_EQ( GEODIFF_createChangesetEx( testContext(), "sqlite", "", gpkgBase.c_str(), gpkgCopy.c_str(), diff.c_str() ), GEODIFF_SUCCESS );

  ASSERT_TRUE( isFileEmpty( diff ) );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_floating_point_values_2 )
{
  // Check that when we copy floats from PG and insert them back, there's not a single digit lost

  std::string conninfo = pgTestConnInfo();

  execSqlCommandsFromString( conninfo, "DROP SCHEMA IF EXISTS gd_floats_copy CASCADE;" );

  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "floats.sql" ) );

  ASSERT_EQ( GEODIFF_makeCopy( testContext(), "postgres", conninfo.c_str(), "gd_floats", "postgres", conninfo.c_str(), "gd_floats_copy" ), GEODIFF_SUCCESS );

  makedir( pathjoin( tmpdir(), "test_floating_point_values_2" ) );
  std::string diff( pathjoin( tmpdir(), "test_floating_point_values_2", "orig-copy.diff" ) );

  ASSERT_EQ( GEODIFF_createChangesetEx( testContext(), "postgres", conninfo.c_str(), "gd_floats", "gd_floats_copy", diff.c_str() ), GEODIFF_SUCCESS );

  ASSERT_TRUE( isFileEmpty( diff ) );
}

TEST( PostgresDriverTest, test_empty_geom )
{
  // Copy GPKG with a table that contains some empty geometries
  // and back and check whether the copy still has correct values
  // (GPKG geometry encoding keeps "empty" flag in its header, so this tests if we set it correctly from WKB)

  std::string conninfo = pgTestConnInfo();

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string gpkgBase = pathjoin( testdir(), "empty_geom", "db-empty.gpkg" );
  std::string pgBase( "gd_empty_geom" );

  makedir( pathjoin( tmpdir(), "test_empty_geom" ) );
  std::string gpkgCopy( pathjoin( tmpdir(), "test_empty_geom", "db-empty.gpkg" ) );
  std::string diff( pathjoin( tmpdir(), "test_empty_geom", "db-empty.diff" ) );

  ASSERT_EQ( GEODIFF_makeCopy( testContext(), "sqlite", "", gpkgBase.c_str(), "postgres", conninfo.c_str(), pgBase.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_makeCopy( testContext(), "postgres", conninfo.c_str(), pgBase.c_str(), "sqlite", "", gpkgCopy.c_str() ), GEODIFF_SUCCESS );

  ASSERT_EQ( GEODIFF_createChangesetEx( testContext(), "sqlite", "", gpkgBase.c_str(), gpkgCopy.c_str(), diff.c_str() ), GEODIFF_SUCCESS );

  ASSERT_TRUE( isFileEmpty( diff ) );

  PQfinish( c );
}

TEST( PostgresDriverTest, test_edge_cases )
{
  std::string conninfo = pgTestConnInfo();

  PGconn *c = PQconnectdb( conninfo.c_str() );
  ASSERT_EQ( PQstatus( c ), CONNECTION_OK );

  std::string gpkgBase = pathjoin( testdir(), "edge-cases", "db-edge-cases.gpkg" );
  std::string gpkgMod = pathjoin( testdir(), "edge-cases", "db-edge-cases-mod.gpkg" );

  std::string pgBase( "gd_edge_base" );
  std::string pgMod( "gd_edge_mod" );

  makedir( pathjoin( tmpdir(), "test_edge_cases" ) );
  std::string gpkgChangeset( pathjoin( tmpdir(), "test_edge_cases", "gpkgChangeset.diff" ) );
  std::string pgChangeset( pathjoin( tmpdir(), "test_edge_cases", "pgChangeset.diff" ) );

  ASSERT_EQ( GEODIFF_makeCopy( testContext(), "sqlite", "", gpkgBase.c_str(), "postgres", conninfo.c_str(), pgBase.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_makeCopy( testContext(), "sqlite", "", gpkgMod.c_str(), "postgres", conninfo.c_str(), pgMod.c_str() ), GEODIFF_SUCCESS );

  ASSERT_EQ( GEODIFF_createChangesetEx( testContext(), "sqlite", "", gpkgBase.c_str(), gpkgMod.c_str(), gpkgChangeset.c_str() ), GEODIFF_SUCCESS );
  ASSERT_EQ( GEODIFF_createChangesetEx( testContext(), "postgres", conninfo.c_str(), pgBase.c_str(), pgMod.c_str(), pgChangeset.c_str() ), GEODIFF_SUCCESS );

  EXPECT_TRUE( fileContentEquals( gpkgChangeset, pgChangeset ) );

  PQfinish( c );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
