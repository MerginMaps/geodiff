/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFGEOPACKAGEDRIVER_HPP
#define GEODIFFGEOPACKAGEDRIVER_HPP

#include "geodiffdriver.hpp"
#include "geodiffsqlitedriver.hpp"

class GeopackageDriver: public SqliteDriver
{
  public:
    GeopackageDriver(): SqliteDriver( "GEOPACKAGE" )
    {}

  private:
};

#endif //GEODIFFDRIVER_HPP
