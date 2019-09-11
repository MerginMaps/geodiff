/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFSQLITEDRIVER_HPP
#define GEODIFFSQLITEDRIVER_HPP

#include "geodiffdriver.hpp"

class SqliteDriver: public Driver
{
  public:
    SqliteDriver();
    SqliteDriver( const std::string &name );
    ~SqliteDriver();

    static void init();

    virtual int createChangeset( const std::string &base, const std::string &modified, const std::string &changeset );
    virtual int createRebasedChangeset( const std::string &base, const std::string &modified, const std::string &changeset_their, const std::string &changeset );
    virtual int applyChangeset( const std::string &base, const std::string &patched, const std::string &changeset );
    virtual int listChanges( const std::string &changeset, int &nchanges );

  private:
};

#endif //GEODIFFSQLITEDRIVER_HPP
