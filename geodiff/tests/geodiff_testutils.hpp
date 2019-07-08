/*
 GEODIFF (MIT License)
 Copyright (C) Lutra Consulting Ltd. 2019
*/

#ifndef GEODIFF_TESTUTILS_HPP
#define GEODIFF_TESTUTILS_HPP

#include <string>
#include <vector>

#include "geodiff.h"

std::string testdir();
std::string tmpdir();

void init_test();
void finalize_test();

std::string test_file( std::string basename );
std::string tmp_file( std::string basename );

#endif // GEODIFF_TESTUTILS_HPP
