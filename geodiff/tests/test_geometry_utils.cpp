/*
 GEODIFF - MIT License
 Copyright (C) 2022 Alexander Bruy
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"

#include "changesetreader.h"
#include "sqliteutils.h"


TEST( GeometryUtilsTest, test_wkb_from_geometry )
{
  std::string changeset = pathjoin( testdir(), "1_geopackage", "base-modified_1_geom.diff" );

  ChangesetReader reader;
  EXPECT_TRUE( reader.open( changeset ) );

  ChangesetEntry entry;
  EXPECT_TRUE( reader.nextEntry( entry ) );
  EXPECT_EQ( entry.table->name, "gpkg_contents" );

  EXPECT_TRUE( reader.nextEntry( entry ) );
  EXPECT_EQ( entry.table->name, "simple" );
  EXPECT_EQ( entry.oldValues[1].type(), Value::TypeBlob );
  std::string gpkgWkb = entry.oldValues[1].getString();
  std::string wkb = createWkbFromGpkgHeader( gpkgWkb );

  // re-create GPKG envelope
  TableColumnInfo col;
  col.geomSrsId = 4326;
  col.geomType = "POINT";
  std::string binHead = createGpkgHeader( wkb, col );

  // fill envelope with geometry
  std::string gpb( binHead.size() + wkb.size(), 0 );
  memcpy( &gpb[0], binHead.data(), binHead.size() );
  memcpy( &gpb[binHead.size()], wkb.data(), wkb.size() );

  EXPECT_EQ( gpkgWkb, gpb );

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
