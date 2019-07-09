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
#include <boost/filesystem.hpp>

std::string testdir()
{
  return TEST_DATA_DIR;
}

std::string tmpdir()
{
  return boost::filesystem::temp_directory_path().string();
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
  GEODIFF_init();
}

void finalize_test()
{
}
