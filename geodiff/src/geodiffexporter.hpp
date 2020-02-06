/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFEXPORTER_H
#define GEODIFFEXPORTER_H

#include <string>
#include <memory>
#include <exception>
#include <vector>

#include "sqlite3.h"
#include "geodiffutils.hpp"

class GeoDiffExporter
{
  public:
    GeoDiffExporter();

    static std::string toString( sqlite3_changeset_iter *pp );

    std::string toJSON( Buffer &buf ) const;
    std::string toJSON( Sqlite3ChangesetIter &pp ) const;
    std::string toJSON( const ConflictFeature &conflict ) const;
    std::string toJSON( const std::vector<ConflictFeature> &conflicts ) const;
    std::string toJSONSummary( Buffer &buf ) const;

  private:
    void addValue( std::string &stream,
                   std::shared_ptr<Sqlite3Value> value, const std::string &type ) const;

    void addValue( std::string &stream,
                   sqlite3_value *ppValue, const std::string &type ) const;

    std::shared_ptr<Sqlite3Db> mDb; // to be able to run ST_* functions
};

#endif // GEODIFFEXPORTER_H
