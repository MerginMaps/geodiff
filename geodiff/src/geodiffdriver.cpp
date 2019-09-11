/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#include "geodiffdriver.hpp"


Driver::Driver( const std::string &name ): mName( name ) {}

Driver::~Driver() {}

std::string Driver::name() const
{
  return mName;
}
