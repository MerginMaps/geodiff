/*
 GEODIFF - MIT License
 Copyright (C) 2019 Peter Petrik
*/

#ifndef GEODIFFDRIVERMANAGER_HPP
#define GEODIFFDRIVERMANAGER_HPP

#include "geodiffdriver.hpp"

#include <vector>
#include <memory>


class DriverManager
{
  public:
    static DriverManager &instance()
    {
      static DriverManager sInstance;
      return sInstance;
    }

    DriverManager( DriverManager const & )   = delete;
    void operator=( DriverManager const & )  = delete;

    size_t driversCount() const;
    std::shared_ptr<Driver> driver( const std::string &name ) const;
    std::shared_ptr<Driver> driver( size_t index ) const;

  private:
    DriverManager();
    std::vector<std::shared_ptr<Driver>> mDrivers;
};
#endif //GEODIFFDRIVERMANAGER_HPP
