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
std::string pathjoin( const std::string &dir, const std::string &filename );
std::string pathjoin( const std::string &dir, const std::string &dir2, const std::string &filename );
void makedir( const std::string &dir );
void filecopy( const std::string &to, const std::string &from );

void init_test();
void finalize_test();

std::string test_file( std::string basename );
std::string tmp_file( std::string basename );

int fileContains( const std::string &filepath, const std::string key );
void printJSON( const std::string &changeset, const std::string &json, const std::string &json_summary );
void printFileToStdout( const std::string &caption, const std::string &filepath );
int countConflicts( const std::string &conflictFile );

/**
 * \param ignore_timestamp_change ignore last_change in table gpkg_contents
 */
bool equals( const std::string &file1,
             const std::string &file2,
             bool ignore_timestamp_change = false );

#endif // GEODIFF_TESTUTILS_HPP
