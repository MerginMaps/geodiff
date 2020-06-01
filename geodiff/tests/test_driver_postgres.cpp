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

  GEODIFF_listChanges( output.c_str(), outputJson.c_str() );
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


int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
