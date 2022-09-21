/*
 GEODIFF (MIT License)
 Copyright (C) Lutra Consulting Ltd. 2019
*/

#ifndef GEODIFF_TESTUTILS_HPP
#define GEODIFF_TESTUTILS_HPP

#include <string>
#include <vector>
#include <unordered_map>

#include "geodiff.h"
#include "geodiff_config.hpp"
#include "geodiffcontext.hpp"

#ifdef WIN32
#define UNICODE
#endif

std::string testdir();
std::string pathjoin( const std::string &dir, const std::string &filename );
std::string pathjoin( const std::string &dir, const std::string &dir2, const std::string &filename );
void makedir( const std::string &dir );

void init_test();
GEODIFF_ContextH testContext();
void finalize_test();

std::string test_file( std::string basename );
std::string tmp_file( std::string basename );

int fileContains( const std::string &filepath, const std::string key );
void printJSON( const std::string &changeset, const std::string &json, const std::string &json_summary );
void printFileToStdout( const std::string &caption, const std::string &filepath );
int countConflicts( const std::string &conflictFile );

std::wstring stringToWString( const std::string &str );
std::string wstringToString( const std::wstring &wStr );

/**
 * \param ignore_timestamp_change ignore last_change in table gpkg_contents
 */
bool equals( const std::string &file1,
             const std::string &file2,
             bool ignore_timestamp_change = false );

//! Tests whether two files are binary equal
bool fileContentEquals( const std::string &file1, const std::string &file2 );

//! Tests whether a file is empty (has zero size). \note returns false when file does not exist
bool isFileEmpty( const std::string &filepath );

struct ChangesetTable;
struct ChangesetEntry;

//! Helper function to write a diff file for a couple of tables
void writeChangeset( std::string filename, const std::unordered_map<std::string, ChangesetTable> &tables,
                     const std::unordered_map<std::string, std::vector<ChangesetEntry> > &entries );

//! Writes a diff file with a couple of entries for a single table
void writeSingleTableChangeset( std::string filename, const ChangesetTable &table, std::vector<ChangesetEntry> entries );

//! A single changeset can be stored in different ways (e.g. different order of entries)
//! so this function tests whether they are actually the same
bool compareDiffsByContent( std::string diffA, std::string diffB );

#ifdef HAVE_POSTGRES
/**
 * Returns the connection info for the postgres databases
 * Use GEODIFF_PG_CONNINFO and GEODIFF_PG_CONNINFO2 env variables for setup.
 * By default returns connection info defined by GEODIFF_PG_CONNINFO, to
 * access connection info from GEODIFF_PG_CONNINFO2 env variable set
 * secondInstance to true.
 * Returns empty string by default
 */
std::string pgTestConnInfo( bool secondInstance = false );
#endif

#endif // GEODIFF_TESTUTILS_HPP
