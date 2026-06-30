/*
 GEODIFF - MIT License
 Copyright (C) 2026 Jan Caha
*/

#include "gtest/gtest.h"
#include "geodiffcontext.hpp"
#include "geodiffutils.hpp"

TEST( ContextTest, defaultFilterModeIsNothing )
{
  Context ctx;
  EXPECT_EQ( ctx.tableFilterMode(), TablesFilterMode::None );
}

TEST( ContextTest, setTablesToSkip )
{
  Context ctx;
  ctx.setTablesToSkip( { "lines", "polygons" } );
  EXPECT_EQ( ctx.tableFilterMode(), TablesFilterMode::SkippedTables );
}

TEST( ContextTest, setTablesToInclude )
{
  Context ctx;
  ctx.setTablesToInclude( { "points" } );
  EXPECT_EQ( ctx.tableFilterMode(), TablesFilterMode::IncludedTables );
}

TEST( ContextTest, cannotSetIncludeAfterSkip )
{
  Context ctx;
  ctx.setTablesToSkip( { "lines" } );
  EXPECT_THROW( ctx.setTablesToInclude( { "points" } ), GeoDiffException );
}

TEST( ContextTest, cannotSetSkipAfterInclude )
{
  Context ctx;
  ctx.setTablesToInclude( { "points" } );
  EXPECT_THROW( ctx.setTablesToSkip( { "lines" } ), GeoDiffException );
}

TEST( ContextTest, skipModeSkipsListedTable )
{
  Context ctx;
  ctx.setTablesToSkip( { "lines" } );
  EXPECT_TRUE( ctx.isTableSkipped( "lines" ) );
  EXPECT_FALSE( ctx.isTableSkipped( "points" ) );
}

TEST( ContextTest, includeModeSkipsUnlistedTable )
{
  Context ctx;
  ctx.setTablesToInclude( { "points" } );
  EXPECT_FALSE( ctx.isTableSkipped( "points" ) );
  EXPECT_TRUE( ctx.isTableSkipped( "lines" ) );
}

TEST( ContextTest, nothingModeSkipsNothing )
{
  Context ctx;
  EXPECT_FALSE( ctx.isTableSkipped( "points" ) );
  EXPECT_FALSE( ctx.isTableSkipped( "lines" ) );
}

TEST( ContextTest, clearSkipByEmptyList )
{
  Context ctx;
  ctx.setTablesToSkip( { "lines" } );
  EXPECT_EQ( ctx.tableFilterMode(), TablesFilterMode::SkippedTables );
  ctx.setTablesToSkip( {} );
  EXPECT_EQ( ctx.tableFilterMode(), TablesFilterMode::None );
}

TEST( ContextTest, clearIncludeByEmptyList )
{
  Context ctx;
  ctx.setTablesToInclude( { "points" } );
  EXPECT_EQ( ctx.tableFilterMode(), TablesFilterMode::IncludedTables );
  ctx.setTablesToInclude( {} );
  EXPECT_EQ( ctx.tableFilterMode(), TablesFilterMode::None );
}

TEST( ContextTest, canSetIncludeAfterClearingSkip )
{
  Context ctx;
  ctx.setTablesToSkip( { "lines" } );
  ctx.setTablesToSkip( {} );
  EXPECT_NO_THROW( ctx.setTablesToInclude( { "points" } ) );
  EXPECT_EQ( ctx.tableFilterMode(), TablesFilterMode::IncludedTables );
}

TEST( ContextTest, canSetSkipAfterClearingInclude )
{
  Context ctx;
  ctx.setTablesToInclude( { "points" } );
  ctx.setTablesToInclude( {} );
  EXPECT_NO_THROW( ctx.setTablesToSkip( { "lines" } ) );
  EXPECT_EQ( ctx.tableFilterMode(), TablesFilterMode::SkippedTables );
}

TEST( ContextTest, clearSkipResetsIsTableSkipped )
{
  Context ctx;
  ctx.setTablesToSkip( { "lines" } );
  EXPECT_TRUE( ctx.isTableSkipped( "lines" ) );
  ctx.setTablesToSkip( {} );
  EXPECT_FALSE( ctx.isTableSkipped( "lines" ) );
}

TEST( ContextTest, clearIncludeResetsIsTableSkipped )
{
  Context ctx;
  ctx.setTablesToInclude( { "points" } );
  EXPECT_TRUE( ctx.isTableSkipped( "lines" ) );
  ctx.setTablesToInclude( {} );
  EXPECT_FALSE( ctx.isTableSkipped( "lines" ) );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  return RUN_ALL_TESTS();
}
