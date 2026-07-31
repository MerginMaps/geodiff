/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "gtest/gtest.h"
#include <variant>
#include "changeset.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"

#include "changesetreader.h"

TEST( ChangesetReaderTest, test_open )
{
  std::string changeset = "invalid_file";

  ChangesetReader reader;
  EXPECT_FALSE( reader.open( changeset ) );
}

TEST( ChangesetReaderTest, test_read_insert )
{
  std::string changeset = pathjoin( testdir(), "2_inserts", "base-inserted_1_A.diff" );

  ChangesetReader reader;
  EXPECT_TRUE( reader.open( changeset ) );

  ChangesetEntry entry;
  EXPECT_TRUE( reader.nextEntry( entry ) );
  EXPECT_TRUE( std::holds_alternative<ChangesetDataEntry>( entry ) );
  ChangesetDataEntry &dataEntry = std::get<ChangesetDataEntry>( entry );
  EXPECT_EQ( dataEntry.op, ChangesetDataEntry::OpInsert );
  EXPECT_EQ( dataEntry.table->name, "simple" );
  EXPECT_EQ( dataEntry.table->primaryKeys.size(), 4 );
  EXPECT_EQ( dataEntry.table->primaryKeys[0], true );
  EXPECT_EQ( dataEntry.table->primaryKeys[1], false );
  EXPECT_EQ( dataEntry.newValues.size(), 4 );
  EXPECT_EQ( dataEntry.newValues[0].type(), Value::TypeInt );
  EXPECT_EQ( dataEntry.newValues[0].getInt(), 4 );
  EXPECT_EQ( dataEntry.newValues[1].type(), Value::TypeBlob );
  EXPECT_EQ( dataEntry.newValues[2].type(), Value::TypeText );
  EXPECT_EQ( dataEntry.newValues[2].getString(), "my new point A" );

  EXPECT_FALSE( reader.nextEntry( entry ) );
  EXPECT_FALSE( reader.nextEntry( entry ) );
}

TEST( ChangesetReaderTest, test_read_update )
{
  std::string changeset = pathjoin( testdir(), "2_updates", "base-updated_A.diff" );

  ChangesetReader reader;
  EXPECT_TRUE( reader.open( changeset ) );

  ChangesetEntry entry;
  EXPECT_TRUE( reader.nextEntry( entry ) );
  EXPECT_TRUE( std::holds_alternative<ChangesetDataEntry>( entry ) );
  ChangesetDataEntry &dataEntry = std::get<ChangesetDataEntry>( entry );

  EXPECT_EQ( dataEntry.op, ChangesetDataEntry::OpUpdate );
  EXPECT_EQ( dataEntry.table->name, "simple" );

  EXPECT_EQ( dataEntry.oldValues.size(), 4 );
  EXPECT_EQ( dataEntry.newValues.size(), 4 );
  // pkey - unchanged
  EXPECT_EQ( dataEntry.oldValues[0].type(), Value::TypeInt );
  EXPECT_EQ( dataEntry.oldValues[0].getInt(), 2 );
  EXPECT_EQ( dataEntry.newValues[0].type(), Value::TypeUndefined );
  // geometry - changed
  EXPECT_EQ( dataEntry.oldValues[1].type(), Value::TypeBlob );
  EXPECT_EQ( dataEntry.newValues[1].type(), Value::TypeBlob );
  // unchanged
  EXPECT_EQ( dataEntry.oldValues[2].type(), Value::TypeUndefined );
  EXPECT_EQ( dataEntry.newValues[2].type(), Value::TypeUndefined );
  // changed
  EXPECT_EQ( dataEntry.oldValues[3].type(), Value::TypeInt );
  EXPECT_EQ( dataEntry.oldValues[3].getInt(), 2 );
  EXPECT_EQ( dataEntry.newValues[3].type(), Value::TypeInt );
  EXPECT_EQ( dataEntry.newValues[3].getInt(), 9999 );

  EXPECT_FALSE( reader.nextEntry( entry ) );
}

TEST( ChangesetReaderTest, test_read_delete )
{
  std::string changeset = pathjoin( testdir(), "2_deletes", "base-deleted_A.diff" );

  ChangesetReader reader;
  EXPECT_TRUE( reader.open( changeset ) );

  ChangesetEntry entry;
  EXPECT_TRUE( reader.nextEntry( entry ) );
  EXPECT_TRUE( std::holds_alternative<ChangesetDataEntry>( entry ) );
  ChangesetDataEntry &dataEntry = std::get<ChangesetDataEntry>( entry );

  EXPECT_EQ( dataEntry.op, ChangesetDataEntry::OpDelete );
  EXPECT_EQ( dataEntry.table->name, "simple" );

  EXPECT_EQ( dataEntry.oldValues.size(), 4 );
  EXPECT_EQ( dataEntry.oldValues[0].type(), Value::TypeInt );
  EXPECT_EQ( dataEntry.oldValues[0].getInt(), 2 );
  EXPECT_EQ( dataEntry.oldValues[1].type(), Value::TypeBlob );
  EXPECT_EQ( dataEntry.oldValues[2].type(), Value::TypeText );
  EXPECT_EQ( dataEntry.oldValues[2].getString(), "feature2" );
  EXPECT_EQ( dataEntry.oldValues[3].type(), Value::TypeInt );
  EXPECT_EQ( dataEntry.oldValues[3].getInt(), 2 );

  EXPECT_FALSE( reader.nextEntry( entry ) );
  EXPECT_FALSE( reader.nextEntry( entry ) );
}


int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
