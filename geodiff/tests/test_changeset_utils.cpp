/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"

#include "changesetutils.h"
#include "changesetreader.h"
#include "changesetwriter.h"

#include "geodiffutils.hpp"

#include "json.hpp"

static void doInvert( const std::string &changeset, const std::string &invChangeset )
{
  ChangesetReader reader;
  ChangesetWriter writer;
  EXPECT_TRUE( reader.open( changeset ) );
  ASSERT_NO_THROW( writer.open( invChangeset ) );
  invertChangeset( reader, writer );
}

static std::string checkDoubleInvertEqual( const std::string &testName, const std::string &changeset )
{
  makedir( pathjoin( tmpdir(), testName ) );
  std::string invChangeset = pathjoin( tmpdir(), testName, "inv.diff" );
  std::string invInvChangeset = pathjoin( tmpdir(), testName, "inv_inv.diff" );

  doInvert( changeset, invChangeset );
  doInvert( invChangeset, invInvChangeset );

  EXPECT_TRUE( fileContentEquals( changeset, invInvChangeset ) );
  return invChangeset;
}

TEST( ChangesetUtils, test_invert_insert )
{
  std::string invChangeset =
    checkDoubleInvertEqual( "test_invert_insert",
                            pathjoin( testdir(), "2_inserts", "base-inserted_1_A.diff" ) );

  ChangesetReader readerInv;
  EXPECT_TRUE( readerInv.open( invChangeset ) );

  ChangesetEntry entry;
  EXPECT_TRUE( readerInv.nextEntry( entry ) );
  EXPECT_EQ( entry.op, ChangesetEntry::OpDelete );
  EXPECT_EQ( entry.table->name, "simple" );
  EXPECT_EQ( entry.oldValues.size(), 4 );
  EXPECT_EQ( entry.oldValues[0].getInt(), 4 );
  EXPECT_EQ( entry.oldValues[2].getString(), "my new point A" );
  EXPECT_EQ( entry.oldValues[3].getInt(), 1 );

  EXPECT_FALSE( readerInv.nextEntry( entry ) );
}

TEST( ChangesetUtils, test_invert_delete )
{
  std::string invChangeset =
    checkDoubleInvertEqual( "test_invert_delete",
                            pathjoin( testdir(), "2_deletes", "base-deleted_A.diff" ) );

  ChangesetReader readerInv;
  EXPECT_TRUE( readerInv.open( invChangeset ) );

  ChangesetEntry entry;
  EXPECT_TRUE( readerInv.nextEntry( entry ) );
  EXPECT_EQ( entry.op, ChangesetEntry::OpInsert );
  EXPECT_EQ( entry.table->name, "simple" );
  EXPECT_EQ( entry.newValues.size(), 4 );
  EXPECT_EQ( entry.newValues[0].getInt(), 2 );
  EXPECT_EQ( entry.newValues[2].getString(), "feature2" );
  EXPECT_EQ( entry.newValues[3].getInt(), 2 );

  EXPECT_FALSE( readerInv.nextEntry( entry ) );
}

TEST( ChangesetUtils, test_invert_update )
{
  std::string invChangeset =
    checkDoubleInvertEqual( "test_invert_update",
                            pathjoin( testdir(), "2_updates", "base-updated_A.diff" ) );

  ChangesetReader readerInv;
  EXPECT_TRUE( readerInv.open( invChangeset ) );

  ChangesetEntry entry;
  EXPECT_TRUE( readerInv.nextEntry( entry ) );
  EXPECT_EQ( entry.op, ChangesetEntry::OpUpdate );
  EXPECT_EQ( entry.table->name, "simple" );
  EXPECT_EQ( entry.oldValues.size(), 4 );
  EXPECT_EQ( entry.oldValues[0].type(), Value::TypeInt );
  EXPECT_EQ( entry.oldValues[0].getInt(), 2 );
  EXPECT_EQ( entry.oldValues[2].type(), Value::TypeUndefined );
  EXPECT_EQ( entry.oldValues[3].getInt(), 9999 );
  EXPECT_EQ( entry.newValues.size(), 4 );
  EXPECT_EQ( entry.newValues[0].type(), Value::TypeUndefined );
  EXPECT_EQ( entry.newValues[2].type(), Value::TypeUndefined );
  EXPECT_EQ( entry.newValues[3].getInt(), 2 );

  EXPECT_FALSE( readerInv.nextEntry( entry ) );
}

static void doExportAndCompare( const std::string &changesetBase, const std::string &changesetDest, bool summary = false )
{
  ChangesetReader reader;
  EXPECT_TRUE( reader.open( changesetBase + ".diff" ) );

  nlohmann::json json = summary ? changesetToJSONSummary( reader ) : changesetToJSON( reader );
  std::string expectedFilename = changesetBase + ( summary ? "-summary.json" : ".json" );

  std::ifstream f( expectedFilename );
  nlohmann::json expected = nlohmann::json::parse( f );

  EXPECT_TRUE( json == expected );
}

TEST( ChangesetUtils, test_export_json )
{
  makedir( pathjoin( tmpdir(), "test_export_json" ) );

  doExportAndCompare( pathjoin( testdir(), "2_inserts", "base-inserted_1_A" ),
                      pathjoin( tmpdir(), "test_export_json", "insert-diff.json" ) );

  doExportAndCompare( pathjoin( testdir(), "2_updates", "base-updated_A" ),
                      pathjoin( tmpdir(), "test_export_json", "update-diff.json" ) );

  doExportAndCompare( pathjoin( testdir(), "2_deletes", "base-deleted_A" ),
                      pathjoin( tmpdir(), "test_export_json", "delete-diff.json" ) );
}

TEST( ChangesetUtils, test_export_json_summary )
{
  makedir( pathjoin( tmpdir(), "test_export_json_summary" ) );

  doExportAndCompare( pathjoin( testdir(), "2_updates", "base-updated_A" ),
                      pathjoin( tmpdir(), "test_export_json_summary", "update-diff-summary.json" ), true );

}

TEST( ChangesetUtils, test_hex_conversion )
{
  EXPECT_EQ( bin2hex( "A\xff" ), "41FF" );
  EXPECT_EQ( hex2bin( "41FF" ), "A\xff" );
  EXPECT_EQ( hex2bin( "41ff" ), "A\xff" );
}


void testConcat( std::string testName,
                 const std::unordered_map<std::string, ChangesetTable> &tables,
                 const std::unordered_map<std::string, std::vector<ChangesetEntry> > &entries1,
                 const std::unordered_map<std::string, std::vector<ChangesetEntry> > &entries2,
                 const std::unordered_map<std::string, std::vector<ChangesetEntry> > &entriesExpected )
{
  std::string input1 = pathjoin( tmpdir(), "test_concat", testName + "-1.diff" );
  std::string input2 = pathjoin( tmpdir(), "test_concat", testName + "-2.diff" );
  std::string expected = pathjoin( tmpdir(), "test_concat", testName + "-expected.diff" );
  std::string output = pathjoin( tmpdir(), "test_concat", testName + "-result.diff" );

  writeChangeset( input1, tables, entries1 );
  writeChangeset( input2, tables, entries2 );
  writeChangeset( expected, tables, entriesExpected );

  const char *inputs[] = { input1.data(), input2.data() };
  int res = GEODIFF_concatChanges( testContext(), 2, inputs, output.data() );
  EXPECT_EQ( res, GEODIFF_SUCCESS );

  // check result
  EXPECT_TRUE( compareDiffsByContent( output, expected ) );
}


void testConcatOneTable( std::string testName,
                         const ChangesetTable &table,
                         std::vector<ChangesetEntry> entries1,
                         std::vector<ChangesetEntry> entries2,
                         std::vector<ChangesetEntry> entriesExpected )
{
  testConcat( testName,
  { std::make_pair( table.name, table ) },
  { std::make_pair( table.name, entries1 ) },
  { std::make_pair( table.name, entries2 ) },
  { std::make_pair( table.name, entriesExpected ) } );
}


TEST( ChangesetUtils, test_concat_changesets_simple_table )
{
  // basic table with one pkey column
  ChangesetTable tableFoo;
  tableFoo.name = "foo";
  tableFoo.primaryKeys.push_back( true ); // fid (pkey)
  tableFoo.primaryKeys.push_back( false ); // name
  tableFoo.primaryKeys.push_back( false ); // rating

  ChangesetEntry fooInsert123 = ChangesetEntry::make(
  &tableFoo, ChangesetEntry::OpInsert, {},
  { Value::makeInt( 123 ), Value::makeText( "hello" ), Value::makeInt( 5 ) } );

  ChangesetEntry fooDelete123 = ChangesetEntry::make(
                                  &tableFoo, ChangesetEntry::OpDelete,
  { Value::makeInt( 123 ), Value::makeText( "hello" ), Value::makeInt( 5 ) }, {} );

  ChangesetEntry fooUpdate123 = ChangesetEntry::make(
                                  &tableFoo, ChangesetEntry::OpUpdate,
  { Value::makeInt( 123 ), Value::makeText( "hello" ), Value::makeInt( 5 ) },
  { Value(), Value::makeText( "world" ), Value::makeInt( 4 ) } );

  ChangesetEntry fooDelete123_2 = ChangesetEntry::make(
                                    &tableFoo, ChangesetEntry::OpDelete,
  { Value::makeInt( 123 ), Value::makeText( "world" ), Value::makeInt( 4 ) }, {} );

  ChangesetEntry fooUpdate123_2 = ChangesetEntry::make(
                                    &tableFoo, ChangesetEntry::OpUpdate,
  { Value::makeInt( 123 ), Value(), Value::makeInt( 4 ) },
  { Value(), Value(), Value::makeInt( 1 ) } );

  ChangesetEntry fooUpdate123_inverse = ChangesetEntry::make(
                                          &tableFoo, ChangesetEntry::OpUpdate,
  { Value::makeInt( 123 ), Value::makeText( "world" ), Value::makeInt( 4 ) },
  { Value(), Value::makeText( "hello" ), Value::makeInt( 5 ) } );

  ChangesetEntry fooUpdate123_pkey = ChangesetEntry::make(
                                       &tableFoo, ChangesetEntry::OpUpdate,
  { Value::makeInt( 123 ), Value(), Value() },
  { Value::makeInt( 124 ), Value(), Value() } );

  ChangesetEntry fooUpdate456 = ChangesetEntry::make(
                                  &tableFoo, ChangesetEntry::OpUpdate,
  { Value::makeInt( 456 ), Value(), Value::makeInt( 1 ) },
  { Value(), Value(), Value::makeInt( 2 ) } );

  makedir( pathjoin( tmpdir(), "test_concat" ) );

  testConcatOneTable( "foo-insert-update", tableFoo, { fooInsert123 }, { fooUpdate123 },
  {
    ChangesetEntry::make( &tableFoo, ChangesetEntry::OpInsert, {},
    { Value::makeInt( 123 ), Value::makeText( "world" ), Value::makeInt( 4 ) }
                        )
  } );

  testConcatOneTable( "foo-insert-delete", tableFoo, { fooInsert123 }, { fooDelete123 }, {} );

  testConcatOneTable( "foo-update-update", tableFoo, { fooUpdate123 }, { fooUpdate123_2 },
  {
    ChangesetEntry::make( &tableFoo, ChangesetEntry::OpUpdate,
    { Value::makeInt( 123 ), Value::makeText( "hello" ), Value::makeInt( 5 ) },
    { Value(), Value::makeText( "world" ), Value::makeInt( 1 ) }
                        )
  } );

  testConcatOneTable( "foo-update-inv-update", tableFoo, { fooUpdate123 }, { fooUpdate123_inverse }, { } );

  testConcatOneTable( "foo-update-delete", tableFoo, { fooUpdate123 }, { fooDelete123_2 },
  {
    ChangesetEntry::make( &tableFoo, ChangesetEntry::OpDelete,
    { Value::makeInt( 123 ), Value::makeText( "hello" ), Value::makeInt( 5 ) },
    {}
                        )
  } );

  testConcatOneTable( "foo-delete-insert", tableFoo, { fooDelete123_2 }, { fooInsert123 },
  {
    ChangesetEntry::make( &tableFoo, ChangesetEntry::OpUpdate,
    { Value::makeInt( 123 ), Value::makeText( "world" ), Value::makeInt( 4 ) },
    { Value(), Value::makeText( "hello" ), Value::makeInt( 5 ) }
                        )
  } );

  testConcatOneTable( "foo-delete-inv-insert", tableFoo, { fooDelete123 }, { fooInsert123 }, { } );

  testConcatOneTable( "foo-unrelated-insert-update", tableFoo, { fooInsert123 }, { fooUpdate456 },
  { fooInsert123, fooUpdate456 } );

  // TODO: merging of updates when there was change of pkey seems not to work correctly with
  // neither our sqlite3session nor our implementation. (is this valid at all?)
//  testConcatOneTable( "update-pkey-update", tableFoo, { fooUpdate123 }, { fooUpdate123_pkey }, {
//                      ChangesetEntry::make( &tableFoo, ChangesetEntry::OpUpdate,
//                           { Value::makeInt( 123 ), Value::makeText( "hello" ), Value::makeInt( 5 ) },
//                           { Value::makeInt( 124 ), Value::makeText( "world" ), Value::makeInt( 4 ) }
//                   ) } );

}

TEST( ChangesetUtils, test_concat_changesets_no_pkey_table )
{
  // a table with no pkey
  ChangesetTable tableNoPkey;
  tableNoPkey.name = "table_no_pkey";
  tableNoPkey.primaryKeys.push_back( false );
  tableNoPkey.primaryKeys.push_back( false );

  ChangesetEntry noPkeyInsert1 = ChangesetEntry::make(
  &tableNoPkey, ChangesetEntry::OpInsert, {},
  { Value::makeInt( 1 ), Value::makeText( "hey" ) } );

  ChangesetEntry noPkeyUpdate2 = ChangesetEntry::make(
                                   &tableNoPkey, ChangesetEntry::OpUpdate,
  { Value::makeInt( 2 ), Value::makeText( "huh" ) },
  { Value(), Value::makeText( "ho!" ) } );

  // TODO: neither sqlite3session nor our implementation work correctly. (is this valid at all?)
  //testConcatOneTable( "no-pkey", tableNoPkey, { noPkeyInsert1 }, { noPkeyUpdate2 }, { noPkeyInsert1, noPkeyUpdate2 } );
}


TEST( ChangesetUtils, test_concat_changesets_multiple_tables )
{
  ChangesetTable tableFoo;
  tableFoo.name = "foo";
  tableFoo.primaryKeys.push_back( true ); // fid (pkey)
  tableFoo.primaryKeys.push_back( false ); // name
  tableFoo.primaryKeys.push_back( false ); // rating

  ChangesetTable tableBar;
  tableBar.name = "bar";
  tableBar.primaryKeys.push_back( true ); // fid (pkey)
  tableBar.primaryKeys.push_back( false ); // name

  ChangesetEntry fooInsert123 = ChangesetEntry::make(
  &tableFoo, ChangesetEntry::OpInsert, {},
  { Value::makeInt( 123 ), Value::makeText( "hello" ), Value::makeInt( 5 ) } );

  ChangesetEntry barInsert123 = ChangesetEntry::make(
  &tableBar, ChangesetEntry::OpInsert, {},
  { Value::makeInt( 123 ), Value::makeText( "ha!" ) } );

  ChangesetEntry barUpdate123 = ChangesetEntry::make(
                                  &tableFoo, ChangesetEntry::OpUpdate,
  { Value::makeInt( 123 ), Value::makeText( "ha!" ) },
  { Value(), Value::makeText( ":-)" ) } );

  testConcat( "multi-related-insert-update",
  { std::make_pair( "foo", tableFoo ), std::make_pair( "bar", tableBar ) },
  // changeset 1
  {
    std::make_pair( "foo", std::vector<ChangesetEntry>( { fooInsert123 } ) ),
    std::make_pair( "bar", std::vector<ChangesetEntry>( { barInsert123 } ) )
  },
  // changeset 2
  { std::make_pair( "bar", std::vector<ChangesetEntry>( { barUpdate123 } ) ) },
  // expected result
  {
    std::make_pair( "foo", std::vector<ChangesetEntry>( {
      ChangesetEntry::make( &tableFoo, ChangesetEntry::OpInsert, {},
      { Value::makeInt( 123 ), Value::makeText( "hello" ), Value::makeInt( 5 ) }
                          ) } ) ),
    std::make_pair( "bar", std::vector<ChangesetEntry>( {
      ChangesetEntry::make( &tableBar, ChangesetEntry::OpInsert, {},
      { Value::makeInt( 123 ), Value::makeText( ":-)" ) }
                          ) } ) )
  } );

  testConcat( "multi-unrelated-insert-update",
  { std::make_pair( "foo", tableFoo ), std::make_pair( "bar", tableBar ) },
  // changeset 1
  { std::make_pair( "foo", std::vector<ChangesetEntry>( { fooInsert123 } ) ) },
  // changeset 2
  { std::make_pair( "bar", std::vector<ChangesetEntry>( { barUpdate123 } ) ) },
  // expected result
  {
    std::make_pair( "foo", std::vector<ChangesetEntry>( {
      ChangesetEntry::make( &tableFoo, ChangesetEntry::OpInsert, {},
      { Value::makeInt( 123 ), Value::makeText( "hello" ), Value::makeInt( 5 ) }
                          ) } ) ),
    std::make_pair( "bar", std::vector<ChangesetEntry>( {
      ChangesetEntry::make( &tableBar, ChangesetEntry::OpUpdate,
      { Value::makeInt( 123 ), Value::makeText( "ha!" ) },
      { Value(), Value::makeText( ":-)" ) }
                          ) } ) )
  } );
}

TEST( ChangesetUtils, test_schema )
{
  makedir( pathjoin( tmpdir(), "test_schema" ) );
  std::string base = pathjoin( testdir(), "base.gpkg" );
  std::string schema = pathjoin( tmpdir(), "test_schema", "schema.json" );
  std::string schema_empty = pathjoin( tmpdir(), "test_schema", "schema-empty.json" );

  // invalid inputs
  EXPECT_EQ( GEODIFF_schema( testContext(), "qqq", nullptr, base.data(), schema.data() ), GEODIFF_ERROR );
  EXPECT_EQ( GEODIFF_schema( testContext(), "sqlite", nullptr, "--bad filename--", schema.data() ), GEODIFF_ERROR );

  // valid input
  EXPECT_EQ( GEODIFF_schema( testContext(), "sqlite", nullptr, base.data(), schema.data() ), GEODIFF_SUCCESS );

  std::ifstream fo( schema );
  nlohmann::json created = nlohmann::json::parse( fo );

  std::ifstream fe( pathjoin( testdir(), "schema", "base-schema.json" ) );
  nlohmann::json expected = nlohmann::json::parse( fe );

  EXPECT_TRUE( created == expected );

  // with --skip-tables
  std::string tablesToSkip( "simple" );
  Context *ctx = static_cast<Context *>( testContext() );
  ctx->setTablesToSkip( tablesToSkip );

  EXPECT_EQ( GEODIFF_schema( testContext(), "sqlite", nullptr, base.data(), schema_empty.data() ), GEODIFF_SUCCESS );

  fo = std::ifstream( schema_empty );
  created = nlohmann::json::parse( fo );

  fe = std::ifstream( pathjoin( testdir(), "schema", "base-schema-no-tables.json" ) );
  expected = nlohmann::json::parse( fe );

  EXPECT_TRUE( created == expected );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
