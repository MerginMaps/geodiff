/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFDRIVER_HPP
#define GEODIFFDRIVER_HPP

#include <string>

class Driver
{
  public:
    Driver( const std::string &name );
    virtual ~Driver();

    std::string name() const;

    virtual int createChangeset( const std::string &base, const std::string &modified, const std::string &changeset ) = 0;
    virtual int createRebasedChangeset( const std::string &base, const std::string &modified, const std::string &changeset_their, const std::string &changeset ) = 0;
    virtual int applyChangeset( const std::string &base, const std::string &patched, const std::string &changeset ) = 0;
    virtual int listChanges( const std::string &changeset, int &nchanges ) = 0;
  private:
    std::string mName;
};

#endif //GEODIFFDRIVER_HPP
