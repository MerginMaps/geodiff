/*
 GEODIFF - MIT License
 Copyright (C) 2021 Martin Dobias
*/

#include "gtest/gtest.h"
#include "geodiff_testutils.hpp"
#include "geodiff.h"

#include "geodiffutils.hpp"

TEST( UtilsTest, test_unicode )
{
  std::string diffSource = pathjoin( testdir(), "utf_test_ščé", "changes.diff" );
  std::string json = pathjoin( tmpdir(), "čúčo.json" );

  // test whether unicode characters are working
  EXPECT_EQ( GEODIFF_listChanges( diffSource.c_str(), json.c_str() ), GEODIFF_SUCCESS );

  // make sure our test util functions can deal with unicode
  EXPECT_TRUE( fileExists( json ) );
  EXPECT_FALSE( isFileEmpty( json ) );
  EXPECT_TRUE( fileContains( json, "geodiff" ) );

  printFileToStdout( "CUCO.JSON", json );
}


int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  init_test();
  int ret =  RUN_ALL_TESTS();
  finalize_test();
  return ret;
}
