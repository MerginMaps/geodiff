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

  PQfinish( c );
}


TEST( PostgresDriverTest, test_basic )
{
  std::string conninfo = "";
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
  ASSERT_EQ( sch.columns.size(), 4 );
  ASSERT_EQ( sch.columns[0].name, "fid" );
  ASSERT_EQ( sch.columns[0].type, "integer" );
  ASSERT_EQ( sch.columns[0].isPrimaryKey, true );
  ASSERT_EQ( sch.columns[0].isGeometry, false );

  ASSERT_EQ( sch.columns[1].name, "geometry" );
  ASSERT_EQ( sch.columns[1].type, "geometry(Point,4326)" );
  ASSERT_EQ( sch.columns[1].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[1].isGeometry, true );
  ASSERT_EQ( sch.columns[1].geomType, "POINT" );
  ASSERT_EQ( sch.columns[1].geomSrsId, 4326 );

  ASSERT_EQ( sch.columns[2].name, "name" );
  ASSERT_EQ( sch.columns[2].type, "text" );
  ASSERT_EQ( sch.columns[2].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[2].isGeometry, false );

  ASSERT_EQ( sch.columns[3].name, "rating" );
  ASSERT_EQ( sch.columns[3].type, "integer" );
  ASSERT_EQ( sch.columns[3].isPrimaryKey, false );
  ASSERT_EQ( sch.columns[3].isGeometry, false );
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
  std::string conninfo = "";
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "inserted_1_a.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "updated_a.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "deleted_a.sql" ) );

  testCreateChangeset( "test_postgres_insert", conninfo, "gd_base", "gd_inserted_1_a", "inserted_1_a.diff" );
  testCreateChangeset( "test_postgres_update", conninfo, "gd_base", "gd_updated_a", "updated_a.diff" );
  testCreateChangeset( "test_postgres_delete", conninfo, "gd_base", "gd_deleted_a", "deleted_a.diff" );
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

  std::string changesetCompare = pathjoin( tmpdir(), "test_postgres", "compare.diff" );
  makedir( pathjoin( tmpdir(), "test_postgres" ) );
  remove( changesetCompare.c_str() );
  EXPECT_FALSE( fileExists( changesetCompare ) );

  {
    DriverParametersMap paramsCompare;
    paramsCompare["conninfo"] = conninfo;
    paramsCompare["base"] = schemaFinal;
    paramsCompare["modified"] = "gd_test_apply";

    std::unique_ptr<Driver> driver( Driver::createDriver( "postgres" ) );
    ASSERT_TRUE( driver );
    driver->open( paramsCompare );

    ChangesetWriter writer;
    writer.open( changesetCompare );
    driver->createChangeset( writer );
  }

  EXPECT_TRUE( fileExists( changesetCompare ) );
  EXPECT_TRUE( isFileEmpty( changesetCompare ) );
}

TEST( PostgresDriverTest, test_apply_changeset )
{
  std::string conninfo = "";

  testApplyChangeset( pathjoin( testdir(), "postgres", "inserted_1_a.diff" ), conninfo, "gd_inserted_1_a" );
  testApplyChangeset( pathjoin( testdir(), "postgres", "updated_a.diff" ), conninfo, "gd_updated_a" );
  testApplyChangeset( pathjoin( testdir(), "postgres", "deleted_a.diff" ), conninfo, "gd_deleted_a" );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
