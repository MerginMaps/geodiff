/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#define xstr(a) str(a)
#define str(a) #a

#include "geodiff_testutils.hpp"
#include <vector>
#include <math.h>
#include <assert.h>

std::string testdir()
{
  return TEST_DATA_DIR;
}

std::string tmpdir()
{
  return TEST_TMP_DIR;
}

std::string test_file( std::string basename )
{
  std::string path( testdir() );
  path += basename;
  return path;
}

std::string tmp_file( std::string basename )
{
  std::string path( tmpdir() );
  path += basename;
  return path;
}

void init_test()
{
  init();
}

void finalize_test()
{
}
