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

extern "C"
{
#include <libpq-fe.h>
}

//#include "geodiffutils.hpp"

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
  //params["modified"] = "gd_inserted_1_a";

  std::unique_ptr<Driver> driver( Driver::createDriver( "postgres" ) );
  ASSERT_TRUE( driver );
  driver->open( params );

  for ( auto t : driver->listTables() )
  {
    std::cout << "TBL " << t << std::endl;
  }

  TableSchema sch = driver->tableSchema( "simple" );
  for ( size_t i = 0; i < sch.columns.size(); ++i )
  {
    std::cout << "- " << sch.columns[i].name << " -- " << sch.columns[i].type << " -- " << sch.columns[i].isPrimaryKey << std::endl;
  }
}
#include "changesetutils.h"
void testCreateChangeset( const std::string &testname, const std::string &conninfo, const std::string &schemaBase, const std::string &schemaModified )
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
}

TEST( PostgresDriverTest, test_create_changeset )
{
  std::string conninfo = "";
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "inserted_1_a.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "updated_a.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "deleted_a.sql" ) );

  testCreateChangeset( "test_postgres_insert", conninfo, "gd_base", "gd_inserted_1_a" );
  testCreateChangeset( "test_postgres_update", conninfo, "gd_base", "gd_updated_a" );
  testCreateChangeset( "test_postgres_delete", conninfo, "gd_base", "gd_deleted_a" );
}

void testApplyChangeset( const std::string &testname, const std::string &conninfo )
{
  std::string schemaBase = "gd_test_apply";

  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "base.sql" ) );
  execSqlCommands( conninfo, pathjoin( testdir(), "postgres", "test_apply.sql" ) );

  DriverParametersMap params;
  params["conninfo"] = conninfo;
  params["base"] = schemaBase;

  std::unique_ptr<Driver> driver( Driver::createDriver( "postgres" ) );
  ASSERT_TRUE( driver );
  driver->open( params );

  ChangesetReader reader;
  reader.open( pathjoin( tmpdir(), testname, "output.diff" ) );
  driver->applyChangeset( reader );

  // TODO: compare
}

TEST( PostgresDriverTest, test_apply_changeset )
{
  std::string conninfo = "";

  // TODO: use existing diff files from sqlite

  //testApplyChangeset( "test_postgres_insert", conninfo );
  //testApplyChangeset( "test_postgres_delete", conninfo );
  testApplyChangeset( "test_postgres_update", conninfo );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
