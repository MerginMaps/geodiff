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

class ConflictFeature;
class ChangesetReader;
class ChangesetWriter;
struct ChangesetEntry;
struct ChangesetTable;
struct TableSchema;
struct Value;
class Context;

ChangesetTable schemaToChangesetTable( const std::string &tableName, const TableSchema &tbl );

void invertChangeset( ChangesetReader &reader, ChangesetWriter &writer );

void concatChangesets( const Context *context, const std::vector<std::string> &filenames, const std::string &outputChangeset );

nlohmann::json changesetEntryToJSON( const ChangesetEntry &entry );

nlohmann::json changesetToJSON( ChangesetReader &reader );

nlohmann::json changesetToJSONSummary( ChangesetReader &reader );

nlohmann::json conflictsToJSON( const std::vector<ConflictFeature> &conflicts );

nlohmann::json valueToJSON( const Value &value );

std::string hex2bin( const std::string &str );
std::string bin2hex( const std::string &str );

#endif // CHANGESETUTILS_H
