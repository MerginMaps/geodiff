/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#include "gtest/gtest.h"
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
  EXPECT_EQ( entry.op, ChangesetEntry::OpInsert );
  EXPECT_EQ( entry.table->name, "simple" );
  EXPECT_EQ( entry.table->primaryKeys.size(), 4 );
  EXPECT_EQ( entry.table->primaryKeys[0], true );
  EXPECT_EQ( entry.table->primaryKeys[1], false );
  EXPECT_EQ( entry.newValues.size(), 4 );
  EXPECT_EQ( entry.newValues[0].type(), Value::TypeInt );
  EXPECT_EQ( entry.newValues[0].getInt(), 4 );
  EXPECT_EQ( entry.newValues[1].type(), Value::TypeBlob );
  EXPECT_EQ( entry.newValues[2].type(), Value::TypeText );
  EXPECT_EQ( entry.newValues[2].getString(), "my new point A" );

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
  EXPECT_EQ( entry.op, ChangesetEntry::OpUpdate );
  EXPECT_EQ( entry.table->name, "simple" );

  EXPECT_EQ( entry.oldValues.size(), 4 );
  EXPECT_EQ( entry.newValues.size(), 4 );
  // pkey - unchanged
  EXPECT_EQ( entry.oldValues[0].type(), Value::TypeInt );
  EXPECT_EQ( entry.oldValues[0].getInt(), 2 );
  EXPECT_EQ( entry.newValues[0].type(), Value::TypeUndefined );
  // geometry - changed
  EXPECT_EQ( entry.oldValues[1].type(), Value::TypeBlob );
  EXPECT_EQ( entry.newValues[1].type(), Value::TypeBlob );
  // unchanged
  EXPECT_EQ( entry.oldValues[2].type(), Value::TypeUndefined );
  EXPECT_EQ( entry.newValues[2].type(), Value::TypeUndefined );
  // changed
  EXPECT_EQ( entry.oldValues[3].type(), Value::TypeInt );
  EXPECT_EQ( entry.oldValues[3].getInt(), 2 );
  EXPECT_EQ( entry.newValues[3].type(), Value::TypeInt );
  EXPECT_EQ( entry.newValues[3].getInt(), 9999 );

  EXPECT_FALSE( reader.nextEntry( entry ) );
}

TEST( ChangesetReaderTest, test_read_delete )
{
  std::string changeset = pathjoin( testdir(), "2_deletes", "base-deleted_A.diff" );

  ChangesetReader reader;
  EXPECT_TRUE( reader.open( changeset ) );

  ChangesetEntry entry;
  EXPECT_TRUE( reader.nextEntry( entry ) );
  EXPECT_EQ( entry.op, ChangesetEntry::OpDelete );
  EXPECT_EQ( entry.table->name, "simple" );

  EXPECT_EQ( entry.oldValues.size(), 4 );
  EXPECT_EQ( entry.oldValues[0].type(), Value::TypeInt );
  EXPECT_EQ( entry.oldValues[0].getInt(), 2 );
  EXPECT_EQ( entry.oldValues[1].type(), Value::TypeBlob );
  EXPECT_EQ( entry.oldValues[2].type(), Value::TypeText );
  EXPECT_EQ( entry.oldValues[2].getString(), "feature2" );
  EXPECT_EQ( entry.oldValues[3].type(), Value::TypeInt );
  EXPECT_EQ( entry.oldValues[3].getInt(), 2 );

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
