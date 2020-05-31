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

static void doInvert( const std::string &changeset, const std::string &invChangeset )
{
  ChangesetReader reader;
  ChangesetWriter writer;
  EXPECT_TRUE( reader.open( changeset ) );
  EXPECT_TRUE( writer.open( invChangeset ) );

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
  entry.table->name = "gpkg_contents";   // this one is not too interesting at this point

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
  entry.table->name = "gpkg_contents";   // this one is not too interesting at this point

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
  entry.table->name = "gpkg_contents";   // this one is not too interesting at this point

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

static void doExportAndCompare( const std::string &changesetBase, const std::string &changesetDest )
{
  ChangesetReader reader;
  EXPECT_TRUE( reader.open( changesetBase + ".diff" ) );

  std::string json = changesetToJSON( reader );

  {
    std::ofstream f( changesetDest );
    EXPECT_TRUE( f.is_open() );
    f << json;
  }

  EXPECT_TRUE( fileContentEquals( changesetBase + ".json", changesetDest ) );
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

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
