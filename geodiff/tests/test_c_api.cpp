/*
 GEODIFF - MIT License
 Copyright (C) 2023 Peter Petrik
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"
#include "geodiffutils.hpp"

#include "json.hpp"

#include <fstream>
#include <string>
#include <vector>

#include <sqlite3.h>

static void execSql( const std::string &dbPath, const std::string &sql )
{
  sqlite3 *db = nullptr;
  ASSERT_EQ( SQLITE_OK, sqlite3_open( dbPath.c_str(), &db ) );
  ASSERT_EQ( SQLITE_OK, sqlite3_exec( db, sql.c_str(), nullptr, nullptr, nullptr ) );
  sqlite3_close( db );
}

//! Overwrites the root page of the geometry columns table, so that reading rows from it fails
static void damageGeometryColumns( const std::string &dbPath )
{
  sqlite3 *db = nullptr;
  ASSERT_EQ( SQLITE_OK, sqlite3_open( dbPath.c_str(), &db ) );
  const char *sql = "SELECT (rootpage - 1) * page_size, page_size FROM sqlite_master, pragma_page_size "
                    "WHERE name = 'gpkg_geometry_columns'";
  sqlite3_stmt *stmt = nullptr;
  ASSERT_EQ( SQLITE_OK, sqlite3_prepare_v2( db, sql, -1, &stmt, nullptr ) );
  ASSERT_EQ( SQLITE_ROW, sqlite3_step( stmt ) );
  const sqlite3_int64 offset = sqlite3_column_int64( stmt, 0 );
  const std::string garbage( sqlite3_column_int( stmt, 1 ), '\xff' );
  sqlite3_finalize( stmt );
  sqlite3_close( db );

  std::fstream file( dbPath, std::ios::in | std::ios::out | std::ios::binary );
  file.seekp( offset );
  file.write( garbage.data(), garbage.size() );
  ASSERT_TRUE( file.good() );
}

static nlohmann::json readJson( const std::string &path )
{
  std::ifstream in( path );
  if ( !in.is_open() )
  {
    ADD_FAILURE() << "Cannot open " << path;
    return nullptr;
  }
  return nlohmann::json::parse( in );
}

TEST( CAPITest, invalid_calls )
{
  GEODIFF_ContextH invalidContext = nullptr;
  GEODIFF_ContextH context = GEODIFF_createContext();

  char buffer[200];

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_driverNameFromIndex( invalidContext, 0, buffer ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_driverIsRegistered( invalidContext, "sqlite" ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_driverIsRegistered( context, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_CX_setLoggerCallback( invalidContext, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_CX_setMaximumLoggerLevel( invalidContext, GEODIFF_LoggerLevel::LevelWarning ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_CX_setTablesToSkip( invalidContext, 0, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_CX_setTablesToSkip( context, 1, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_CX_setTablesToInclude( invalidContext, 0, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_CX_setTablesToInclude( context, 1, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createChangesetEx( invalidContext, "sqlite", nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createChangesetEx( context, "sqlite", nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createChangesetEx( context, "invalid driver", " ", " ", " ", " " ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createChangesetDr( invalidContext, "sqlite", nullptr, nullptr, "sqlite", nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createChangesetDr( context, "sqlite", nullptr, nullptr, "sqlite", nullptr, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_applyChangesetEx( invalidContext, "sqlite", nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_applyChangesetEx( context, "sqlite", nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_applyChangesetEx( context, "invalid driver", " ", " ", " " ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createRebasedChangeset( invalidContext, nullptr, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createRebasedChangeset( context, nullptr, nullptr, nullptr, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createRebasedChangesetEx( invalidContext, "sqlite", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createRebasedChangesetEx( context, "sqlite", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );

  ASSERT_EQ( -1, GEODIFF_hasChanges( invalidContext, nullptr ) );
  ASSERT_EQ( -1, GEODIFF_hasChanges( context, nullptr ) );

  ASSERT_EQ( -1, GEODIFF_changesCount( invalidContext, nullptr ) );
  ASSERT_EQ( -1, GEODIFF_changesCount( context, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_listChanges( invalidContext, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_listChanges( context, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_listChangesSummary( invalidContext, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_listChangesSummary( context, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_invertChangeset( invalidContext, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_invertChangeset( context, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_concatChanges( invalidContext, 1, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_concatChanges( context, 1, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_concatChanges( context, 2, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase( invalidContext, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase( context, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase( context, "bad file", "bad file", "bad file", "bad file" ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase( context, pathjoin( testdir(), "base.gpkg" ).c_str(), "bad file", "bad file", "bad file" ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase( context, pathjoin( testdir(), "base.gpkg" ).c_str(), pathjoin( testdir(), "base.gpkg" ).c_str(), "bad file", "bad file" ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebaseEx( invalidContext, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebaseEx( context, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopy( invalidContext, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopy( context, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopy( context, "invalid driver",  " ",  " ",  " ",  " ",  " " ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopy( context, "sqlite",  " ",  " ", "invalid driver",  " ",  " " ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopySqlite( invalidContext, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_makeCopySqlite( context, nullptr, nullptr ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_dumpData( invalidContext, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_dumpData( context, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_dumpData( context, "invalid driver",  " ",  " ",  " " ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_schema( invalidContext, nullptr, nullptr, nullptr, nullptr ) );
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_schema( context, nullptr, nullptr, nullptr, nullptr ) );

  // errors are -1 because 1 means that the schemas are equal
  {
    const std::string base = pathjoin( testdir(), "base.gpkg" );
    const std::string notDatabase = pathjoin( testdir(), "schema", "base-schema.json" );
    const std::string dir = pathjoin( tmpdir(), "invalid_calls" );
    makedir( dir );

    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( invalidContext, "sqlite", "", base.c_str(), base.c_str(), nullptr ) );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, nullptr, "", base.c_str(), base.c_str(), nullptr ) );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", nullptr, base.c_str(), nullptr ) );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", base.c_str(), nullptr, nullptr ) );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "invalid driver", "", base.c_str(), base.c_str(), nullptr ) );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", "bad file", base.c_str(), nullptr ) );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", base.c_str(), "bad file", nullptr ) );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", notDatabase.c_str(), base.c_str(), nullptr ) );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", base.c_str(), notDatabase.c_str(), nullptr ) );

    const std::string unwritable = pathjoin( dir, "no_such_dir", "report.json" );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", base.c_str(), base.c_str(), unwritable.c_str() ) );

    // a report must never overwrite one of the compared databases, however its path is spelled
    const std::string copyBase = pathjoin( dir, "copy_base.gpkg" );
    const std::string copyModified = pathjoin( dir, "copy_modified.gpkg" );
    ASSERT_TRUE( filecopy( copyBase, base ) );
    ASSERT_TRUE( filecopy( copyModified, base ) );
    const std::string respelledBase = pathjoin( dir, ".", "copy_base.gpkg" );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", copyBase.c_str(), copyModified.c_str(), copyBase.c_str() ) );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", copyBase.c_str(), copyModified.c_str(), copyModified.c_str() ) );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", copyBase.c_str(), copyModified.c_str(), respelledBase.c_str() ) );
    ASSERT_TRUE( fileContentEquals( copyBase, base ) );
    ASSERT_TRUE( fileContentEquals( copyModified, base ) );

    // failing in the middle of reading a schema must not be taken for a partial schema
    const std::string damaged = pathjoin( dir, "damaged.gpkg" );
    ASSERT_TRUE( filecopy( damaged, base ) );
    damageGeometryColumns( damaged );
    ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", damaged.c_str(), damaged.c_str(), nullptr ) );
  }

  ASSERT_EQ( nullptr, GEODIFF_readChangeset( invalidContext, nullptr ) );
  ASSERT_EQ( nullptr, GEODIFF_readChangeset( context, nullptr ) );

  ASSERT_EQ( nullptr, GEODIFF_CR_nextEntry( invalidContext, nullptr, nullptr ) );
  bool ok;
  ASSERT_EQ( nullptr, GEODIFF_CR_nextEntry( invalidContext, nullptr, &ok ) );
  ASSERT_EQ( nullptr, GEODIFF_CR_nextEntry( context, nullptr, &ok ) );

  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createWkbFromGpkgHeader( invalidContext, nullptr, size_t( 1 ), nullptr, nullptr ) );

  size_t size;
  const char *wkb;
  ASSERT_EQ( GEODIFF_ERROR, GEODIFF_createWkbFromGpkgHeader( invalidContext, " ", size_t( 0 ), &wkb, &size ) );

  GEODIFF_CX_destroy( context );
}

TEST( CAPITest, has_equal_schema )
{
  GEODIFF_ContextH context = GEODIFF_createContext();
  const std::string dir = pathjoin( tmpdir(), "has_equal_schema" );
  makedir( dir );
  const std::string report = pathjoin( dir, "report.json" );
  const std::string base = pathjoin( testdir(), "base.gpkg" );
  const std::string modifiedScheme = pathjoin( testdir(), "modified_scheme" );

  // same schema, different data
  const std::string updated = pathjoin( testdir(), "2_updates", "updated_A.gpkg" );
  ASSERT_EQ( 1, GEODIFF_hasEqualSchema( context, "sqlite", "", base.c_str(), updated.c_str(), nullptr ) );
  ASSERT_EQ( 1, GEODIFF_hasEqualSchema( context, "sqlite", "", base.c_str(), updated.c_str(), report.c_str() ) );
  ASSERT_EQ( nlohmann::json::parse( R"({"geodiff_schema_diff": []})" ), readJson( report ) );

  // different spellings of the same normalized types
  const std::string aliasA = pathjoin( dir, "alias_a.sqlite" );
  const std::string aliasB = pathjoin( dir, "alias_b.sqlite" );
  fileremove( aliasA );
  fileremove( aliasB );
  execSql( aliasA, "CREATE TABLE t ( fid INTEGER PRIMARY KEY, a VARCHAR(20), b INT, c REAL );" );
  execSql( aliasB, "CREATE TABLE t ( fid INTEGER PRIMARY KEY, a TEXT, b INTEGER, c DOUBLE );" );
  fileremove( report );
  ASSERT_EQ( 1, GEODIFF_hasEqualSchema( context, "sqlite", "", aliasA.c_str(), aliasB.c_str(), report.c_str() ) );
  ASSERT_EQ( nlohmann::json::parse( R"({"geodiff_schema_diff": []})" ), readJson( report ) );

  const std::string orderBase = pathjoin( dir, "order_base.sqlite" );
  const std::string orderModified = pathjoin( dir, "order_modified.sqlite" );
  fileremove( orderBase );
  fileremove( orderModified );
  execSql( orderBase, "CREATE TABLE features ( fid INTEGER PRIMARY KEY, crop_type TEXT, area REAL );" );
  execSql( orderModified, "CREATE TABLE archive ( id INTEGER PRIMARY KEY );"
           "CREATE TABLE features ( fid INTEGER PRIMARY KEY, area REAL, crop_type INTEGER );" );

  const std::string flagsBase = pathjoin( dir, "flags_base.sqlite" );
  const std::string flagsModified = pathjoin( dir, "flags_modified.sqlite" );
  fileremove( flagsBase );
  fileremove( flagsModified );
  execSql( flagsBase, "CREATE TABLE t ( id INTEGER PRIMARY KEY NOT NULL, v TEXT );" );
  execSql( flagsModified, "CREATE TABLE t ( id INTEGER, v TEXT );" );

  const std::string crs = pathjoin( dir, "crs.gpkg" );
  ASSERT_TRUE( filecopy( crs, base ) );
  execSql( crs, "INSERT INTO gpkg_spatial_ref_sys ( srs_name, srs_id, organization, organization_coordsys_id, definition )"
           " VALUES ( 'OSGB36 / British National Grid', 27700, 'EPSG', 27700, 'undefined' );"
           "UPDATE gpkg_geometry_columns SET srs_id = 27700, z = 1;" );

  struct Mismatch
  {
    std::string base;
    std::string modified;
    std::string expected;
  };
  const std::vector<Mismatch> mismatches =
  {
    {
      // added column
      base, pathjoin( modifiedScheme, "added_attribute.gpkg" ),
      R"([{"table": "simple", "column": "added_field", "property": "column", "old": null, "new": "added_field"}])"
    },
    {
      // added table
      base, pathjoin( modifiedScheme, "added_table.gpkg" ),
      R"([{"table": "added_table", "property": "table", "old": null, "new": "added_table"}])"
    },
    {
      // removed table
      pathjoin( modifiedScheme, "added_table.gpkg" ), base,
      R"([{"table": "added_table", "property": "table", "old": "added_table", "new": null}])"
    },
    {
      // tables ordered by name; column order reported separately from the type change of a column
      orderBase, orderModified,
      R"([{"table": "archive", "property": "table", "old": null, "new": "archive"},
          {"table": "features", "property": "column_order", "old": ["fid", "crop_type", "area"], "new": ["fid", "area", "crop_type"]},
          {"table": "features", "column": "crop_type", "property": "type", "old": "text", "new": "integer"}])"
    },
    {
      // type of an existing column
      pathjoin( modifiedScheme, "added_attribute.gpkg" ), pathjoin( modifiedScheme, "added_attribute_different_type.gpkg" ),
      R"([{"table": "simple", "column": "added_field", "property": "type", "old": "integer", "new": "text"}])"
    },
    {
      // column attributes, in a fixed order
      flagsBase, flagsModified,
      R"([{"table": "t", "column": "id", "property": "primary_key", "old": true, "new": false},
          {"table": "t", "column": "id", "property": "not_null", "old": true, "new": false},
          {"table": "t", "column": "id", "property": "auto_increment", "old": true, "new": false}])"
    },
    {
      // CRS of the table, then geometry details of the column
      base, crs,
      R"([{"table": "simple", "property": "crs.srs_id", "old": 4326, "new": 27700},
          {"table": "simple", "property": "crs.auth_code", "old": 4326, "new": 27700},
          {"table": "simple", "column": "geometry", "property": "geometry.srs_id", "old": 4326, "new": 27700},
          {"table": "simple", "column": "geometry", "property": "geometry.has_z", "old": false, "new": true}])"
    },
  };

  for ( const Mismatch &m : mismatches )
  {
    SCOPED_TRACE( m.base + " -> " + m.modified );
    ASSERT_EQ( 0, GEODIFF_hasEqualSchema( context, "sqlite", "", m.base.c_str(), m.modified.c_str(), nullptr ) );

    fileremove( report );
    ASSERT_EQ( 0, GEODIFF_hasEqualSchema( context, "sqlite", "", m.base.c_str(), m.modified.c_str(), report.c_str() ) );
    ASSERT_EQ( nlohmann::json::parse( m.expected ), readJson( report )["geodiff_schema_diff"] );
  }

  // a difference is not an error, a failure is
  ASSERT_STREQ( "", GEODIFF_CX_lastError( context ) );
  ASSERT_EQ( -1, GEODIFF_hasEqualSchema( context, "sqlite", "", base.c_str(), "bad file", nullptr ) );
  ASSERT_STRNE( "", GEODIFF_CX_lastError( context ) );

  GEODIFF_CX_destroy( context );
}

TEST( CAPITest, has_equal_schema_table_filters )
{
  GEODIFF_ContextH context = GEODIFF_createContext();
  const std::string base = pathjoin( testdir(), "base.gpkg" );
  const std::string modified = pathjoin( testdir(), "modified_scheme", "added_table.gpkg" );

  ASSERT_EQ( 0, GEODIFF_hasEqualSchema( context, "sqlite", "", base.c_str(), modified.c_str(), nullptr ) );

  const char *skipped[] = { "added_table" };
  ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_CX_setTablesToSkip( context, 1, skipped ) );
  ASSERT_EQ( 1, GEODIFF_hasEqualSchema( context, "sqlite", "", base.c_str(), modified.c_str(), nullptr ) );
  ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_CX_setTablesToSkip( context, 0, nullptr ) );

  const char *included[] = { "simple" };
  ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_CX_setTablesToInclude( context, 1, included ) );
  ASSERT_EQ( 1, GEODIFF_hasEqualSchema( context, "sqlite", "", base.c_str(), modified.c_str(), nullptr ) );

  GEODIFF_CX_destroy( context );
}

TEST( CAPITest, test_copy )
{
  GEODIFF_ContextH context = GEODIFF_createContext();
  makedir( pathjoin( tmpdir(), "test_copy" ) );

  // database "db-base.gpkg"
  // - table AUDIT has no PK
  // - custom trigger that adds entry to AUDIT table on each update of "simple" table

  {
    std::string fileOutput = pathjoin( tmpdir(), "test_copy", "db-makeCopy.gpkg" );

    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopy(
                 context,
                 "sqlite",
                 "",
                 pathjoin( testdir(), "gpkg_custom_triggers", "db-base.gpkg" ).c_str(),
                 "sqlite",
                 "",
                 fileOutput.c_str() ) );


    // THIS DROPS ALL TRIGGERS!!
    ASSERT_FALSE( fileContentEquals( fileOutput, pathjoin( testdir(), "gpkg_custom_triggers", "db-base.gpkg" ) ) );
  }


  {
    std::string fileOutput = pathjoin( tmpdir(), "test_copy", "db-makeCopySqlite.gpkg" );

    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopySqlite(
                 context,
                 pathjoin( testdir(), "gpkg_custom_triggers", "db-base.gpkg" ).c_str(),
                 fileOutput.c_str() ) );
  }

  GEODIFF_CX_destroy( context );
}

TEST( CAPITest, test_rebases )
{
  GEODIFF_ContextH context = GEODIFF_createContext();
  makedir( pathjoin( tmpdir(), "test_rebases" ) );

  {
    std::string fileOutput = pathjoin( tmpdir(), "test_rebases", "text_pk_A.sqlite" );
    std::string fileConflict = pathjoin( tmpdir(), "test_rebases", "output_text_pk.log" );


    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopySqlite(
                 context,
                 pathjoin( testdir(), "sqlite_pks", "text_pk_A.sqlite" ).c_str(),
                 fileOutput.c_str() ) );


    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_rebase(
                 context,
                 pathjoin( testdir(), "sqlite_pks", "text_pk.sqlite" ).c_str(),
                 pathjoin( testdir(), "sqlite_pks", "text_pk_B.sqlite" ).c_str(),
                 fileOutput.c_str(),
                 fileConflict.c_str() )
             );
  }

  {
    std::string fileOutput = pathjoin( tmpdir(), "test_rebases", "output_compose_pk.sqlite" );
    std::string fileConflict = pathjoin( tmpdir(), "test_rebases", "output_compose_pk.log" );


    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopySqlite(
                 context,
                 pathjoin( testdir(), "sqlite_pks", "multi_primary_key_A.sqlite" ).c_str(),
                 fileOutput.c_str() ) );


    ASSERT_EQ( GEODIFF_ERROR, GEODIFF_rebase(
                 context,
                 pathjoin( testdir(), "sqlite_pks", "multi_primary_key.sqlite" ).c_str(),
                 pathjoin( testdir(), "sqlite_pks", "multi_primary_key_B.sqlite" ).c_str(),
                 fileOutput.c_str(),
                 fileConflict.c_str() )
             );
  }

  {
    std::string fileOutput = pathjoin( tmpdir(), "test_rebases", "output_custom_triggers.gpkg" );
    std::string fileConflict = pathjoin( tmpdir(), "test_rebases", "output_custom_triggers.log" );


    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopySqlite(
                 context,
                 pathjoin( testdir(), "gpkg_custom_triggers", "db-modified_A.gpkg" ).c_str(),
                 fileOutput.c_str() ) );


    ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_rebase(
                 context,
                 pathjoin( testdir(), "gpkg_custom_triggers", "db-base.gpkg" ).c_str(),
                 pathjoin( testdir(), "gpkg_custom_triggers", "db-modified_B.gpkg" ).c_str(),
                 fileOutput.c_str(),
                 fileConflict.c_str() )
             );
  }

  GEODIFF_CX_destroy( context );
}

TEST( CAPITest, test_long_paths )
{
  GEODIFF_ContextH context = GEODIFF_createContext();

  std::string filler1( 150, 'a' );
  std::string filler2( 100, 'b' );

  std::string longdir = pathjoin( tmpdir(), std::string( "test_long_paths" ) + filler1 );
  makedir( longdir );

  std::string baseInput = pathjoin( testdir(), "base.gpkg" );
  std::string baseLong = pathjoin( longdir, filler2 + "_base.gpkg" );
  std::string diffLong = pathjoin( longdir, filler2 + "_base-modified_1_geom.diff" );
  std::string jsonLong = pathjoin( longdir, filler2 + "_base-modified_1_geom.json" );

  ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_makeCopySqlite(
               context,
               baseInput.c_str(),
               baseLong.c_str() ) );

  ASSERT_FALSE( fileContentEquals( baseLong, baseInput ) );

  ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_createChangeset( context, baseLong.c_str(), pathjoin( testdir(), "1_geopackage", "modified_1_geom.gpkg" ).c_str(), diffLong.c_str() ) );

  ASSERT_EQ( GEODIFF_SUCCESS, GEODIFF_listChanges( context, diffLong.c_str(), jsonLong.c_str() ) );

  GEODIFF_CX_destroy( context );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
