/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef CHANGESETUTILS_H
#define CHANGESETUTILS_H

#include "geodiff.h"
#include <string>

class ChangesetReader;
class ChangesetWriter;
struct ChangesetTable;
struct TableSchema;

ChangesetTable schemaToChangesetTable( const std::string &tableName, const TableSchema &tbl );

GEODIFF_EXPORT void invertChangeset( ChangesetReader &reader, ChangesetWriter &writer );

GEODIFF_EXPORT std::string changesetToJSON( ChangesetReader &reader );

GEODIFF_EXPORT std::string changesetToJSONSummary( ChangesetReader &reader );

std::string hex2bin( const std::string &str );
std::string bin2hex( const std::string &str );

#endif // CHANGESETUTILS_H
