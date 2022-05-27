/*
 GEODIFF - MIT License
 Copyright (C) 2020 Martin Dobias
*/

#ifndef CHANGESETUTILS_H
#define CHANGESETUTILS_H

#include "geodiff.h"
#include <string>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

class ConflictFeature;
class ChangesetReader;
class ChangesetWriter;
struct ChangesetEntry;
struct ChangesetTable;
struct TableSchema;
struct Value;

ChangesetTable schemaToChangesetTable( const std::string &tableName, const TableSchema &tbl );

void invertChangeset( ChangesetReader &reader, ChangesetWriter &writer );

void concatChangesets( const std::vector<std::string> &filenames, const std::string &outputChangeset );

json changesetEntryToJSON( const ChangesetEntry &entry );

json changesetToJSON( ChangesetReader &reader );

json changesetToJSONSummary( ChangesetReader &reader );

json conflictsToJSON( const std::vector<ConflictFeature> &conflicts );

json valueToJSON( const Value &value );

std::string hex2bin( const std::string &str );
std::string bin2hex( const std::string &str );

#endif // CHANGESETUTILS_H
